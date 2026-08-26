from __future__ import annotations

import json
import re
from pathlib import Path

from .common import ROOT, load_yaml, resolve
from .identity import recorded_executable
from .native_progress import load_native_progress
from .slices import load_slices, slice_for_address


def _require_pyghidra():
    try:
        import pyghidra
    except ImportError as exc:
        raise SystemExit("PyGhidra is not installed. Run: tony setup ghidra") from exc
    return pyghidra


def _exe_path() -> Path:
    return recorded_executable()


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
            "slice": slice_for_address(address),
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


def gaps(output: Path | None = None, limit: int = 50, slice_id: str | None = None) -> list[dict]:
    exe = _exe_path()
    pyghidra = _require_pyghidra()
    spec = load_yaml("re/config/ghidra.yml")["ghidra"]
    tracked = load_yaml("re/symbols/functions.yml").get("functions", [])
    if slice_id is not None:
        selected = load_slices().get(slice_id)
        if selected is None:
            raise SystemExit(f"unknown reconstruction slice: {slice_id}")
        selected_addresses = {int(value) for value in selected.get("scope", {}).get("functions", [])}
        tracked = [item for item in tracked if int(item["address"]) in selected_addresses]
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
