from __future__ import annotations

import hashlib
import json
import re
import shutil
from pathlib import Path

from .common import ROOT, load_yaml, resolve
from .identity import recorded_executable
from .native_progress import load_native_progress, validate_native_progress
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
            actual_names = [str(parameter.getName()) for parameter in function.getParameters()]
            expected_names = [parameter["name"] for parameter in signature["parameters"]]
            if actual_names != expected_names:
                errors.append(f"{item['name']}: parameters {actual_names!r}, expected {expected_names!r}")
            actual_types = [str(parameter.getDataType().getDisplayName()) for parameter in function.getParameters()]
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


def _tracked_function(address: int) -> dict:
    return next(
        (item for item in load_yaml("re/symbols/functions.yml").get("functions", []) if int(item["address"]) == address),
        {},
    )


def _module_for_address(address: int) -> dict | None:
    for module in load_yaml("match/manifest.yml").get("modules", []):
        if int(module["start_va"]) <= address < int(module["end_va"]):
            return {
                "id": module["id"],
                "start_va": int(module["start_va"]),
                "end_va": int(module["end_va"]),
                "status": module["status"],
                "source": module["source"],
            }
    return None


def _subsystem_for_address(address: int) -> dict | None:
    for path in sorted((ROOT / "re/subsystems").glob("*.yml")):
        document = load_yaml(str(path.relative_to(ROOT)))
        focus = document.get("focus", {})
        if int(focus.get("start_va", -1)) <= address < int(focus.get("end_va", -1)):
            return {"name": document.get("subsystem"), "manifest": str(path.relative_to(ROOT))}
    return None


def _decompile(program, function) -> str:
    from ghidra.app.decompiler import DecompInterface
    from ghidra.util.task import ConsoleTaskMonitor

    decompiler = DecompInterface()
    decompiler.openProgram(program)
    result = decompiler.decompileFunction(function, 60, ConsoleTaskMonitor())
    if not result.decompileCompleted():
        raise SystemExit(f"could not decompile {function.getEntryPoint()}: {result.getErrorMessage()}")
    return str(result.getDecompiledFunction().getC())


def inspect_function(address: int, output: Path | None = None) -> dict:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    spec = load_yaml("re/config/ghidra.yml")["ghidra"]
    pyghidra.start(install_dir=resolve(spec["install_dir"]))
    with pyghidra.open_project(resolve(spec["project_dir"]), spec["project_name"], create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        address_space = program.getAddressFactory().getDefaultAddressSpace()
        function = program.getFunctionManager().getFunctionAt(address_space.getAddress(address))
        if function is None:
            raise SystemExit(f"no function starts at 0x{address:08x}")
        monitor = pyghidra.task_monitor()
        body = [
            {"start_va": int(item.getMinAddress().getOffset()), "end_va": int(item.getMaxAddress().getOffset()) + 1}
            for item in function.getBody().getAddressRanges()
        ]
        callers = sorted(
            ({"address": int(item.getEntryPoint().getOffset()), "name": str(item.getName())}
             for item in function.getCallingFunctions(monitor)),
            key=lambda item: item["address"],
        )
        callees = sorted(
            ({"address": int(item.getEntryPoint().getOffset()), "name": str(item.getName())}
             for item in function.getCalledFunctions(monitor)),
            key=lambda item: item["address"],
        )
        globals_by_address = {}
        listing = program.getListing()
        symbols = program.getSymbolTable()
        for instruction in listing.getInstructions(function.getBody(), True):
            for reference in instruction.getReferencesFrom():
                destination = reference.getToAddress()
                if not destination.isMemoryAddress() or function.getBody().contains(destination):
                    continue
                symbol = symbols.getPrimarySymbol(destination)
                if symbol is not None and str(symbol.getName()) not in {item["name"] for item in callees}:
                    value = int(destination.getOffset())
                    globals_by_address[value] = {"address": value, "name": str(symbol.getName())}
        stack = sorted(
            ({
                "name": str(item.getName()), "offset": int(item.getStackOffset()), "size": int(item.getLength()),
                "type": str(item.getDataType().getDisplayName()),
            } for item in function.getStackFrame().getStackVariables()),
            key=lambda item: item["offset"],
        )
        decompiled = _decompile(program, function)
        field_accesses = sorted({f"{base}->{field}" for base, field in re.findall(r"\b(\w+)->(\w+)\b", decompiled)})
        unresolved_fields = [item for item in field_accesses if "->field_" in item]
        parameters = [{
            "name": str(item.getName()), "type": str(item.getDataType().getDisplayName()),
        } for item in function.getParameters()]
        tracked = _tracked_function(address)
        native = load_native_progress().get(address)
        result = {
            "version": 1,
            "function": {
                "address": address,
                "name": str(function.getName()),
                "calling_convention": str(function.getCallingConventionName()),
                "return_type": str(function.getReturnType().getDisplayName()),
                "parameters": parameters,
                "body": body,
            },
            "callers": callers,
            "callees": callees,
            "globals": [globals_by_address[key] for key in sorted(globals_by_address)],
            "stack_variables": stack,
            "field_accesses": field_accesses,
            "unresolved": {
                "void_pointer_parameters": [item["name"] for item in parameters if item["type"] == "void *"],
                "fields": unresolved_fields,
            },
            "matching": _module_for_address(address),
            "evidence": tracked.get("evidence", []),
            "confidence": tracked.get("confidence"),
            "subsystem": _subsystem_for_address(address),
            "native": native or {"status": "not_recorded", "sources": [], "tests": [], "evidence": []},
        }
    text = json.dumps(result, indent=2) + "\n"
    if output is None:
        print(text, end="")
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
        print(f"Inspected 0x{address:08x}: {output}")
    return result


def gaps(output: Path | None = None, limit: int = 50) -> list[dict]:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    spec = load_yaml("re/config/ghidra.yml")["ghidra"]
    tracked = load_yaml("re/symbols/functions.yml").get("functions", [])
    native_progress = load_native_progress()
    modules = load_yaml("match/manifest.yml").get("modules", [])
    subsystem_ranges = []
    for path in sorted((ROOT / "re/subsystems").glob("*.yml")):
        document = load_yaml(str(path.relative_to(ROOT)))
        focus = document.get("focus", {})
        subsystem_ranges.append((
            int(focus.get("start_va", -1)), int(focus.get("end_va", -1)),
            {"name": document.get("subsystem"), "manifest": str(path.relative_to(ROOT))},
        ))

    def module_at(address: int) -> dict | None:
        for module in modules:
            if int(module["start_va"]) <= address < int(module["end_va"]):
                return {
                    "id": module["id"], "start_va": int(module["start_va"]), "end_va": int(module["end_va"]),
                    "status": module["status"], "source": module["source"],
                }
        return None

    def subsystem_at(address: int) -> dict | None:
        return next((item for start, end, item in subsystem_ranges if start <= address < end), None)

    rows = []
    pyghidra.start(install_dir=resolve(spec["install_dir"]))
    with pyghidra.open_project(resolve(spec["project_dir"]), spec["project_name"], create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        address_space = program.getAddressFactory().getDefaultAddressSpace()
        references = program.getReferenceManager()
        for item in tracked:
            address = int(item["address"])
            ghidra_address = address_space.getAddress(address)
            function = program.getFunctionManager().getFunctionAt(ghidra_address)
            reasons = []
            score = 0
            reference_count = int(references.getReferenceCountTo(ghidra_address))
            score += min(reference_count, 20)
            if function is None:
                reasons.append("missing Ghidra function")
                score += 100
            else:
                parameter_types = [str(parameter.getDataType().getDisplayName()) for parameter in function.getParameters()]
                if any(value == "void *" or value.startswith("undefined") for value in parameter_types):
                    reasons.append("unresolved parameter type")
                    score += 25
            if "signature" not in item:
                reasons.append("missing tracked signature")
                score += 50
            module = module_at(address)
            if module and module["status"] == "raw":
                reasons.append("raw matching module")
                score += 15
            subsystem = subsystem_at(address)
            native = native_progress.get(address)
            if native is None:
                reasons.append("native status not recorded")
                score += 5
            elif native["status"] in {"unmodeled", "modeled"}:
                reasons.append(f"native status {native['status']}")
                score += 20 if native["status"] == "unmodeled" else 10
            if reasons:
                rows.append({
                    "address": address, "name": item["name"], "score": score, "reasons": reasons,
                    "incoming_references": reference_count, "matching": module, "confidence": item.get("confidence"),
                    "evidence": item.get("evidence", []), "subsystem": subsystem,
                    "native": native or {"status": "not_recorded", "sources": [], "tests": [], "evidence": []},
                })
    rows.sort(key=lambda item: (-item["score"], item["address"]))
    rows = rows[:limit]
    document = {"version": 1, "count": len(rows), "gaps": rows}
    text = json.dumps(document, indent=2) + "\n"
    if output is None:
        print(text, end="")
    else:
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
        print(f"Exported {len(rows)} reconstruction gaps: {output}")
    return rows


def export_functions(output: Path | None = None) -> Path:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    install = resolve(spec["install_dir"])
    project_parent = resolve(spec["project_dir"])
    project_name = spec["project_name"]
    output = output or (ROOT / "build/ghidra/functions.json")
    output.parent.mkdir(parents=True, exist_ok=True)

    pyghidra.start(install_dir=install)
    with pyghidra.open_project(project_parent, project_name, create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        rows = []
        for function in program.getFunctionManager().getFunctions(True):
            rows.append({
                "address": int(function.getEntryPoint().getOffset()),
                "address_hex": str(function.getEntryPoint()),
                "name": str(function.getName()),
                "namespace": str(function.getParentNamespace().getName()),
            })
    output.write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
    print(f"Exported {len(rows)} functions: {output}")
    return output


def export_text_claims() -> list[dict]:
    """Return Ghidra-defined function and data ranges within the .text block."""

    exe = _exe_path()
    pyghidra = _require_pyghidra()
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    install = resolve(spec["install_dir"])
    project_parent = resolve(spec["project_dir"])
    project_name = spec["project_name"]

    pyghidra.start(install_dir=install)
    with pyghidra.open_project(project_parent, project_name, create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        block = program.getMemory().getBlock(".text")
        if block is None:
            raise SystemExit("Ghidra program has no .text memory block")
        text_start = int(block.getStart().getOffset())
        text_end = int(block.getEnd().getOffset()) + 1
        claims = []
        listing = program.getListing()
        for function in program.getFunctionManager().getFunctions(True):
            body = function.getBody()
            for address_range in body.getAddressRanges():
                start = max(int(address_range.getMinAddress().getOffset()), text_start)
                end = min(int(address_range.getMaxAddress().getOffset()) + 1, text_end)
                if start < end:
                    first_instruction = listing.getInstructionAt(address_range.getMinAddress())
                    last_instruction = listing.getInstructionContaining(address_range.getMaxAddress())
                    boundary_safe = (
                        first_instruction is not None
                        and last_instruction is not None
                        and int(last_instruction.getMaxAddress().getOffset()) + 1 == end
                    )
                    claims.append(
                        {
                            "start_va": start,
                            "end_va": end,
                            "kind": "function",
                            "name": str(function.getName()),
                            "instruction_boundary_safe": boundary_safe,
                        }
                    )
        data_claims = []
        for data in listing.getDefinedData(True):
            start = max(int(data.getMinAddress().getOffset()), text_start)
            end = min(int(data.getMaxAddress().getOffset()) + 1, text_end)
            if start >= end:
                continue
            data_type = str(data.getDataType().getDisplayName())
            lowered = data_type.lower()
            points_into_text = any(
                text_start <= int(reference.getToAddress().getOffset()) < text_end
                for reference in data.getValueReferences()
            )
            data_claims.append(
                {
                    "start_va": start,
                    "end_va": end,
                    "kind": "defined_data",
                    "name": data_type,
                    "pointer_candidate": "pointer" in lowered and points_into_text,
                }
            )
        candidates = [claim for claim in data_claims if claim.pop("pointer_candidate")]
        candidates.sort(key=lambda claim: int(claim["start_va"]))
        run: list[dict] = []
        for candidate in candidates:
            if run and int(candidate["start_va"]) != int(run[-1]["end_va"]):
                if len(run) >= 2 or sum(int(item["end_va"]) - int(item["start_va"]) for item in run) >= 8:
                    for item in run:
                        item["kind"] = "jump_table"
                run = []
            run.append(candidate)
        if len(run) >= 2 or sum(int(item["end_va"]) - int(item["start_va"]) for item in run) >= 8:
            for item in run:
                item["kind"] = "jump_table"
        claims.extend(data_claims)
    return claims


def decompile_function(address: int, output: Path | None = None) -> Path | None:
    """Decompile one function from the deterministic local Ghidra project."""

    exe = _exe_path()
    pyghidra = _require_pyghidra()
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    install = resolve(spec["install_dir"])
    project_parent = resolve(spec["project_dir"])
    project_name = spec["project_name"]

    pyghidra.start(install_dir=install)
    from ghidra.app.decompiler import DecompInterface
    from ghidra.util.task import ConsoleTaskMonitor

    with pyghidra.open_project(project_parent, project_name, create=False) as project, pyghidra.program_context(
        project, f"/{exe.name}"
    ) as program:
        address_space = program.getAddressFactory().getDefaultAddressSpace()
        function = program.getFunctionManager().getFunctionAt(address_space.getAddress(address))
        if function is None:
            raise SystemExit(f"no function starts at 0x{address:08x}")
        decompiler = DecompInterface()
        decompiler.openProgram(program)
        result = decompiler.decompileFunction(function, 60, ConsoleTaskMonitor())
        if not result.decompileCompleted():
            raise SystemExit(f"could not decompile 0x{address:08x}: {result.getErrorMessage()}")
        text = result.getDecompiledFunction().getC()

    if output is None:
        print(text)
        return None
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")
    print(f"Decompiled 0x{address:08x}: {output}")
    return output
