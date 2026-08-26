from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path

from .common import ROOT, load_yaml, resolve
from .identity import recorded_executable
from .native_progress import validate_native_progress
from .recovered_types import TypeExpression, ghidra_type_plan, parse_type_expression


def _require_pyghidra():
    try:
        import pyghidra
    except ImportError as exc:
        raise SystemExit("PyGhidra is not installed. Run: tony setup ghidra") from exc
    return pyghidra


def _exe_path() -> Path:
    return recorded_executable()


def _fingerprints(profile: str = "complete") -> dict[str, str]:
    config = load_yaml("re/config/ghidra.yml")["ghidra"]
    identity = hashlib.sha256()
    identity.update(_exe_path().read_bytes())
    identity.update(str(config["version"]).encode())
    identity.update(profile.encode())
    knowledge = hashlib.sha256()
    for pattern in ("re/symbols/*.yml", "re/types/*.yml", "re/native/*.yml"):
        for path in sorted(ROOT.glob(pattern)):
            knowledge.update(str(path.relative_to(ROOT)).encode())
            knowledge.update(path.read_bytes())
    knowledge.update(Path(__file__).read_bytes())
    return {"identity": identity.hexdigest(), "knowledge": knowledge.hexdigest(), "profile": profile}


def _report_path() -> Path:
    spec = load_yaml("re/config/ghidra.yml")["ghidra"]
    return resolve(spec["project_dir"]) / "recovered-types.json"


def _apply_symbols(program, pyghidra) -> None:
    from ghidra.program.model.symbol import SourceType

    address_space = program.getAddressFactory().getDefaultAddressSpace()
    function_manager = program.getFunctionManager()
    symbol_table = program.getSymbolTable()

    with pyghidra.transaction(program):
        functions = load_yaml("re/symbols/functions.yml").get("functions", [])
        for item in functions:
            address = address_space.getAddress(int(item["address"]))
            name = str(item["name"])
            function = function_manager.getFunctionAt(address)
            if function is not None:
                function.setName(name, SourceType.USER_DEFINED)
            else:
                symbol_table.createLabel(address, name, SourceType.USER_DEFINED)

        for config_path, key in (
            ("re/symbols/globals.yml", "globals"),
            ("re/symbols/data.yml", "data"),
            ("re/symbols/strings.yml", "strings"),
        ):
            for item in load_yaml(config_path).get(key, []):
                address = address_space.getAddress(int(item["address"]))
                symbol_table.createLabel(address, str(item["name"]), SourceType.USER_DEFINED)


def _apply_tracked_functions(program, pyghidra) -> dict:
    from ghidra.app.cmd.function import CreateFunctionCmd
    from ghidra.program.model.address import AddressSet
    from ghidra.program.model.symbol import SourceType

    report = {"created": [], "skipped": []}
    address_space = program.getAddressFactory().getDefaultAddressSpace()
    listing = program.getListing()
    manager = program.getFunctionManager()
    memory = program.getMemory()
    with pyghidra.transaction(program):
        for item in load_yaml("re/symbols/functions.yml").get("functions", []):
            if item.get("confidence") not in {"observed", "confirmed"}:
                continue
            value = int(item["address"])
            address = address_space.getAddress(value)
            if manager.getFunctionAt(address) is not None:
                continue
            block = memory.getBlock(address)
            containing = manager.getFunctionContaining(address)
            reason = None
            if block is None or str(block.getName()) != ".text" or not block.isExecute():
                reason = "not executable .text"
            elif listing.getInstructionAt(address) is None:
                reason = "no instruction at tracked address"
            elif containing is not None:
                reason = f"overlaps {containing.getName()} at {containing.getEntryPoint()}"
            if reason:
                report["skipped"].append({"address": value, "name": item["name"], "reason": reason})
                continue
            command = CreateFunctionCmd(AddressSet(address, address), SourceType.USER_DEFINED)
            if command.applyTo(program, pyghidra.task_monitor()) and manager.getFunctionAt(address) is not None:
                report["created"].append({"address": value, "name": item["name"]})
            else:
                report["skipped"].append({
                    "address": value, "name": item["name"], "reason": str(command.getStatusMsg() or "creation failed"),
                })
    return report


def _apply_recovered_types(program, pyghidra) -> dict:
    from ghidra.program.model.data import (
        ArrayDataType,
        ByteDataType,
        CategoryPath,
        CharDataType,
        DataTypeConflictHandler,
        DoubleDataType,
        DWordDataType,
        FloatDataType,
        IntegerDataType,
        LongLongDataType,
        PointerDataType,
        QWordDataType,
        ShortDataType,
        SignedByteDataType,
        StructureDataType,
        TypedefDataType,
        VoidDataType,
        WordDataType,
    )

    plan = ghidra_type_plan()
    manager = program.getDataTypeManager()
    category = CategoryPath(plan["category"])
    conflict_handler = DataTypeConflictHandler.REPLACE_HANDLER
    named = {}
    alias_targets = {item["name"]: item["target"] for item in plan["aliases"]}

    primitive = {
        "i8": SignedByteDataType.dataType,
        "u8": ByteDataType.dataType,
        "char": CharDataType.dataType,
        "i16": ShortDataType.dataType,
        "u16": WordDataType.dataType,
        "i32": IntegerDataType.dataType,
        "u32": DWordDataType.dataType,
        "q12_i32": IntegerDataType.dataType,
        "q16_i32": IntegerDataType.dataType,
        "i64": LongLongDataType.dataType,
        "u64": QWordDataType.dataType,
        "f32": FloatDataType.dataType,
        "f64": DoubleDataType.dataType,
        "void": VoidDataType.dataType,
    }

    with pyghidra.transaction(program):
        for item in plan["types"]:
            structure = StructureDataType(category, item["name"], item["size"], manager)
            named[item["name"]] = manager.addDataType(structure, conflict_handler)

        def resolve_type(expression: TypeExpression):
            if expression.kind == "primitive":
                return primitive[expression.name]
            if expression.kind == "named":
                name = expression.name
                if name == "void":
                    return VoidDataType.dataType
                while name in alias_targets:
                    name = alias_targets[name]
                return named[name]
            if expression.kind == "pointer":
                return PointerDataType(resolve_type(expression.element), 4, manager)
            if expression.kind == "array":
                element = resolve_type(expression.element)
                return ArrayDataType(element, expression.count, element.getLength(), manager)
            if expression.kind == "bytes":
                return ArrayDataType(ByteDataType.dataType, expression.count, 1, manager)
            raise ValueError(f"cannot import non-fixed Ghidra type expression: {expression.kind}")

        for item in plan["types"]:
            structure = named[item["name"]]
            structure.setDescription(item["description"])
            for field in item["fields"]:
                structure.replaceAtOffset(
                    field["offset"],
                    resolve_type(parse_type_expression(field["type"])),
                    field["size"],
                    field["name"],
                    field["comment"] or None,
                )

        for item in plan["aliases"]:
            target = named.get(item["target"])
            if target is None:
                plan["skipped"].append({"name": item["name"], "reason": f"target {item['target']} was not imported"})
                continue
            alias = TypedefDataType(category, item["name"], target, manager)
            manager.addDataType(alias, conflict_handler)

    plan["skipped"] = sorted(plan["skipped"], key=lambda item: item["name"])
    return plan


def _apply_type_bindings(program, pyghidra) -> dict:
    from ghidra.program.model.data import (
        ArrayDataType,
        ByteDataType,
        CategoryPath,
        CharDataType,
        DoubleDataType,
        DWordDataType,
        FloatDataType,
        IntegerDataType,
        LongLongDataType,
        PointerDataType,
        QWordDataType,
        ShortDataType,
        SignedByteDataType,
        VoidDataType,
        WordDataType,
    )
    from ghidra.program.model.listing import Function, ParameterImpl, ReturnParameterImpl
    from ghidra.program.model.symbol import SourceType
    from java.util import ArrayList

    manager = program.getDataTypeManager()
    category = CategoryPath("/OpenTony/Recovered")
    address_space = program.getAddressFactory().getDefaultAddressSpace()
    primitive = {
        "i8": SignedByteDataType.dataType,
        "u8": ByteDataType.dataType,
        "char": CharDataType.dataType,
        "i16": ShortDataType.dataType,
        "u16": WordDataType.dataType,
        "i32": IntegerDataType.dataType,
        "u32": DWordDataType.dataType,
        "q12_i32": IntegerDataType.dataType,
        "q16_i32": IntegerDataType.dataType,
        "i64": LongLongDataType.dataType,
        "u64": QWordDataType.dataType,
        "f32": FloatDataType.dataType,
        "f64": DoubleDataType.dataType,
    }

    def resolve_type(expression: TypeExpression):
        if expression.kind == "primitive":
            return primitive[expression.name]
        if expression.kind == "named":
            if expression.name == "void":
                return VoidDataType.dataType
            data_type = manager.getDataType(category, expression.name)
            if data_type is None:
                raise ValueError(f"Ghidra type was not generated: {expression.name}")
            return data_type
        if expression.kind == "pointer":
            return PointerDataType(resolve_type(expression.element), 4, manager)
        if expression.kind == "array":
            element = resolve_type(expression.element)
            return ArrayDataType(element, expression.count, element.getLength(), manager)
        if expression.kind == "bytes":
            return ArrayDataType(ByteDataType.dataType, expression.count, 1, manager)
        raise ValueError(f"cannot bind non-fixed Ghidra type expression: {expression.kind}")

    report = {"functions": [], "globals": []}
    conventions = {"cdecl": "__cdecl", "stdcall": "__stdcall", "thiscall": "__thiscall", "fastcall": "__fastcall"}
    with pyghidra.transaction(program):
        for item in load_yaml("re/symbols/functions.yml").get("functions", []):
            signature = item.get("signature")
            if not signature:
                continue
            address = address_space.getAddress(int(item["address"]))
            function = program.getFunctionManager().getFunctionAt(address)
            if function is None:
                raise ValueError(f"cannot bind missing function {item['name']} at {address}")
            parameters = ArrayList()
            for parameter in signature["parameters"]:
                parameters.add(
                    ParameterImpl(
                        parameter["name"], resolve_type(parse_type_expression(parameter["type"])), program
                    )
                )
            return_value = ReturnParameterImpl(resolve_type(parse_type_expression(signature["return"])), program)
            function.updateFunction(
                conventions[signature["calling_convention"]],
                return_value,
                parameters,
                Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS,
                True,
                SourceType.USER_DEFINED,
            )
            report["functions"].append({"address": int(item["address"]), "name": item["name"], **signature})

        # Global/data bindings use the same canonical `type` expression. None
        # are asserted until the corresponding symbol entry carries one.
        from ghidra.program.model.data import DataUtilities

        for path, key in (("re/symbols/globals.yml", "globals"), ("re/symbols/data.yml", "data")):
            for item in load_yaml(path).get(key, []):
                if "type" not in item:
                    continue
                address = address_space.getAddress(int(item["address"]))
                data_type = resolve_type(parse_type_expression(item["type"]))
                existing = program.getListing().getDataAt(address)
                if existing is None or not existing.getDataType().isEquivalent(data_type):
                    DataUtilities.createData(
                        program, address, data_type, -1, DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA
                    )
                report["globals"].append({"address": int(item["address"]), "name": item["name"], "type": item["type"]})
    return report


def _reanalyze_functions(program, addresses: list[int], pyghidra) -> None:
    if not addresses:
        return
    from ghidra.app.plugin.core.analysis import AutoAnalysisManager
    from ghidra.app.script import GhidraScriptUtil
    from ghidra.program.model.address import AddressSet

    address_space = program.getAddressFactory().getDefaultAddressSpace()
    selected = AddressSet()
    for value in sorted(set(addresses)):
        function = program.getFunctionManager().getFunctionAt(address_space.getAddress(value))
        if function is None:
            raise SystemExit(f"no function starts at 0x{value:08x}")
        selected.add(function.getBody())
        for reference in program.getReferenceManager().getReferencesTo(function.getEntryPoint()):
            caller = program.getFunctionManager().getFunctionContaining(reference.getFromAddress())
            if caller is not None:
                selected.add(caller.getBody())
    with pyghidra.transaction(program):
        GhidraScriptUtil.acquireBundleHostReference()
        try:
            manager = AutoAnalysisManager.getAnalysisManager(program)
            manager.reAnalyzeAll(selected)
            manager.startAnalysis(pyghidra.task_monitor())
        finally:
            GhidraScriptUtil.releaseBundleHostReference()


def _set_analysis_profile(program, profile: str, pyghidra) -> list[str]:
    if profile == "complete":
        return []
    from ghidra.app.plugin.core.analysis import AutoAnalysisManager

    AutoAnalysisManager.getAnalysisManager(program).initializeOptions()
    options = pyghidra.analysis_properties(program)
    disabled = []
    expensive = {"Decompiler Parameter ID", "Function ID", "Non-Returning Functions - Discovered"}
    with pyghidra.transaction(program):
        for name in expensive:
            if options.contains(name):
                options.setBoolean(name, False)
                disabled.append(name)
    return sorted(disabled)


def rebuild(profile: str = "complete") -> None:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    install = resolve(spec["install_dir"])
    project_parent = resolve(spec["project_dir"])
    project_name = spec["project_name"]

    if not install.is_dir():
        raise SystemExit("Ghidra is not provisioned. Run: tony setup ghidra")

    # Rebuild means generated state is intentionally disposable.
    shutil.rmtree(project_parent, ignore_errors=True)
    project_parent.mkdir(parents=True, exist_ok=True)

    pyghidra.start(install_dir=install)
    with pyghidra.open_project(project_parent, project_name, create=True) as project:
        loader = pyghidra.program_loader().project(project).source(str(exe))
        with loader.load() as results:
            results.save(pyghidra.task_monitor())

        program_path = f"/{exe.name}"
        with pyghidra.program_context(project, program_path) as program:
            disabled = _set_analysis_profile(program, profile, pyghidra)
            print(f"Analyzing {exe.name} ...")
            pyghidra.analyze(program, pyghidra.task_monitor())
            function_report = _apply_tracked_functions(program, pyghidra)
            _apply_symbols(program, pyghidra)
            type_report = _apply_recovered_types(program, pyghidra)
            type_report["bindings"] = _apply_type_bindings(program, pyghidra)
            type_report["fingerprints"] = _fingerprints(profile)
            type_report["analysis"] = {"profile": profile, "disabled": disabled}
            type_report["tracked_functions"] = function_report
            program.save("OpenTony deterministic rebuild", pyghidra.task_monitor())

    report_path = _report_path()
    report_path.write_text(json.dumps(type_report, indent=2) + "\n", encoding="utf-8")

    print(f"Ghidra project rebuilt: {project_parent} / {project_name}")
    print(
        f"Recovered types: {len(type_report['types'])} layouts, "
        f"{len(type_report['aliases'])} aliases, {len(type_report['skipped'])} skipped; {report_path}"
    )


def sync(addresses: list[int] | None = None, force: bool = False) -> None:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    spec = load_yaml("re/config/ghidra.yml")["ghidra"]
    install = resolve(spec["install_dir"])
    project_parent = resolve(spec["project_dir"])
    report_path = _report_path()
    if not report_path.is_file():
        raise SystemExit("Ghidra project metadata is missing. Run: tony ghidra rebuild")
    previous = json.loads(report_path.read_text(encoding="utf-8"))
    profile = previous.get("fingerprints", {}).get("profile", "complete")
    current = _fingerprints(profile)
    if previous.get("fingerprints", {}).get("identity") != current["identity"]:
        raise SystemExit("Ghidra project identity is stale. Run: tony ghidra rebuild")
    addresses = addresses or []
    if not force and not addresses and previous.get("fingerprints", {}).get("knowledge") == current["knowledge"]:
        print("Ghidra knowledge is already synchronized")
        return

    pyghidra.start(install_dir=install)
    with pyghidra.open_project(project_parent, spec["project_name"], create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        function_report = _apply_tracked_functions(program, pyghidra)
        _apply_symbols(program, pyghidra)
        report = _apply_recovered_types(program, pyghidra)
        report["bindings"] = _apply_type_bindings(program, pyghidra)
        _reanalyze_functions(program, addresses, pyghidra)
        report["fingerprints"] = current
        report["analysis"] = previous.get("analysis", {"profile": profile, "disabled": []})
        report["tracked_functions"] = function_report
        program.save("OpenTony knowledge sync", pyghidra.task_monitor())
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Ghidra knowledge synchronized; targeted functions: {len(set(addresses))}")


def verify() -> bool:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    spec = load_yaml("re/config/ghidra.yml")["ghidra"]
    report_path = _report_path()
    if not report_path.is_file():
        print("FAIL Ghidra report is missing; run: tony ghidra rebuild")
        return False
    report = json.loads(report_path.read_text(encoding="utf-8"))
    profile = report.get("fingerprints", {}).get("profile", "complete")
    expected_fingerprints = _fingerprints(profile)
    if report.get("fingerprints") != expected_fingerprints:
        print("FAIL Ghidra project knowledge or identity is stale; run: tony ghidra sync")
        return False

    errors = []
    native_errors, _native_counts = validate_native_progress()
    errors.extend(native_errors)
    plan = ghidra_type_plan()

    def display(value: str) -> str:
        expression = parse_type_expression(value)
        primitive = {
            "i8": "char", "u8": "byte", "char": "char", "i16": "short", "u16": "word",
            "i32": "int", "u32": "dword", "q12_i32": "int", "q16_i32": "int",
            "i64": "longlong", "u64": "qword", "f32": "float", "f64": "double",
        }
        if expression.kind == "primitive":
            return primitive[expression.name]
        if expression.kind == "named":
            return expression.name
        if expression.kind == "pointer":
            return f"{display_type(expression.element)} *"
        if expression.kind == "array":
            return f"{display_type(expression.element)}[{expression.count}]"
        if expression.kind == "bytes":
            return f"byte[{expression.count}]"
        raise ValueError(f"non-fixed display type: {value}")

    def display_type(expression: TypeExpression) -> str:
        if expression.kind == "primitive":
            return display(expression.name)
        if expression.kind == "named":
            return expression.name
        if expression.kind == "pointer":
            return f"{display_type(expression.element)} *"
        if expression.kind == "array":
            return f"{display_type(expression.element)}[{expression.count}]"
        if expression.kind == "bytes":
            return f"byte[{expression.count}]"
        raise ValueError(f"non-fixed display type: {expression.kind}")
    pyghidra.start(install_dir=resolve(spec["install_dir"]))
    with pyghidra.open_project(resolve(spec["project_dir"]), spec["project_name"], create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        manager = program.getDataTypeManager()
        from ghidra.program.model.data import CategoryPath

        category = CategoryPath(plan["category"])
        for item in plan["types"]:
            data_type = manager.getDataType(category, item["name"])
            if data_type is None or data_type.getLength() != item["size"]:
                errors.append(f"{item['name']}: expected size 0x{item['size']:x}")
                continue
            for field in item["fields"]:
                component = data_type.getComponentAt(field["offset"])
                if component is None or str(component.getFieldName()) != field["name"]:
                    errors.append(f"{item['name']}+0x{field['offset']:x}: expected {field['name']}")
        address_space = program.getAddressFactory().getDefaultAddressSpace()
        for item in load_yaml("re/symbols/functions.yml").get("functions", []):
            signature = item.get("signature")
            if not signature:
                continue
            function = program.getFunctionManager().getFunctionAt(address_space.getAddress(int(item["address"])))
            actual_parameters = list(function.getParameters())
            # Ghidra exposes the implicit object register as a synthetic
            # `this` parameter for __thiscall functions. It is part of the
            # ABI, not an explicit source-level parameter in the tracked
            # signature.
            if signature["calling_convention"] == "thiscall":
                actual_parameters = [
                    parameter for parameter in actual_parameters if str(parameter.getName()) != "this"
                ]
            actual_names = [str(parameter.getName()) for parameter in actual_parameters]
            expected_names = [parameter["name"] for parameter in signature["parameters"]]
            if actual_names != expected_names:
                errors.append(f"{item['name']}: parameters {actual_names!r}, expected {expected_names!r}")
            actual_types = [str(parameter.getDataType().getDisplayName()) for parameter in actual_parameters]
            expected_types = [display(parameter["type"]) for parameter in signature["parameters"]]
            if actual_types != expected_types:
                errors.append(f"{item['name']}: parameter types {actual_types!r}, expected {expected_types!r}")
            if str(function.getReturnType().getDisplayName()) != display(signature["return"]):
                errors.append(f"{item['name']}: return type is not {signature['return']}")
            expected_convention = f"__{signature['calling_convention']}"
            if str(function.getCallingConventionName()) != expected_convention:
                errors.append(f"{item['name']}: calling convention is not {expected_convention}")
        for path, key in (("re/symbols/globals.yml", "globals"), ("re/symbols/data.yml", "data")):
            for item in load_yaml(path).get(key, []):
                if "type" not in item:
                    continue
                address = address_space.getAddress(int(item["address"]))
                data = program.getListing().getDataAt(address)
                actual = str(data.getDataType().getDisplayName()) if data is not None else None
                expected = display(item["type"])
                if actual != expected:
                    errors.append(f"{item['name']}: global type {actual!r}, expected {expected!r}")
        for item in report.get("tracked_functions", {}).get("created", []):
            address = address_space.getAddress(int(item["address"]))
            if program.getFunctionManager().getFunctionAt(address) is None:
                errors.append(f"{item['name']}: generated function is missing")
    for error in errors:
        print(f"FAIL {error}")
    if errors:
        return False
    print(
        f"Ghidra knowledge: VALID ({len(plan['types'])} layouts, "
        f"{len(report['bindings']['functions'])} signatures, {len(report['bindings']['globals'])} globals)"
    )
    return True
