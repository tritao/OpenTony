from __future__ import annotations

import json
import shutil
from pathlib import Path

from .common import ROOT, load_yaml, resolve
from .identity import recorded_executable


def _require_pyghidra():
    try:
        import pyghidra
    except ImportError as exc:
        raise SystemExit("PyGhidra is not installed. Run: tony setup ghidra") from exc
    return pyghidra


def _exe_path() -> Path:
    return recorded_executable()


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


def rebuild() -> None:
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
            print(f"Analyzing {exe.name} ...")
            pyghidra.analyze(program, pyghidra.task_monitor())
            _apply_symbols(program, pyghidra)
            program.save("OpenTony deterministic rebuild", pyghidra.task_monitor())

    print(f"Ghidra project rebuilt: {project_parent} / {project_name}")


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
        for function in program.getFunctionManager().getFunctions(True):
            body = function.getBody()
            for address_range in body.getAddressRanges():
                start = max(int(address_range.getMinAddress().getOffset()), text_start)
                end = min(int(address_range.getMaxAddress().getOffset()) + 1, text_end)
                if start < end:
                    claims.append(
                        {
                            "start_va": start,
                            "end_va": end,
                            "kind": "function",
                            "name": str(function.getName()),
                        }
                    )
        for data in program.getListing().getDefinedData(True):
            start = max(int(data.getMinAddress().getOffset()), text_start)
            end = min(int(data.getMaxAddress().getOffset()) + 1, text_end)
            if start >= end:
                continue
            data_type = str(data.getDataType().getDisplayName())
            lowered = data_type.lower()
            kind = "jump_table" if "jump" in lowered or "switch" in lowered else "defined_data"
            claims.append(
                {
                    "start_va": start,
                    "end_va": end,
                    "kind": kind,
                    "name": data_type,
                }
            )
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
