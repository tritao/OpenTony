from __future__ import annotations

import json
import shutil
from pathlib import Path

from .common import ROOT, load_yaml, resolve


def _require_pyghidra():
    try:
        import pyghidra
    except ImportError as exc:
        raise SystemExit("PyGhidra is not installed. Run: tony setup ghidra") from exc
    return pyghidra


def _exe_path() -> Path:
    config = load_yaml("re/config/binaries.yml")
    path = config["executables"]["thps2_pc"].get("path")
    if not path:
        raise SystemExit("No THPS2 executable recorded. Run: tony exe identify <path> --record")
    exe = resolve(path)
    if not exe.is_file():
        raise SystemExit(f"Recorded executable does not exist: {exe}")
    return exe


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
    pyghidra = _require_pyghidra()
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    install = resolve(spec["install_dir"])
    exe = _exe_path()
    project_parent = resolve(spec["project_dir"])
    project_name = spec["project_name"]

    if not install.is_dir():
        raise SystemExit("Ghidra is not provisioned. Run: tony setup ghidra")

    # Rebuild means generated state is intentionally disposable.
    shutil.rmtree(project_parent, ignore_errors=True)
    project_parent.mkdir(parents=True, exist_ok=True)

    pyghidra.start(install_dir=install)
    with pyghidra.open_project(project_parent, project_name, create=True) as project:
        loader = pyghidra.program_loader().project(project).source(exe)
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
    pyghidra = _require_pyghidra()
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    install = resolve(spec["install_dir"])
    exe = _exe_path()
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
