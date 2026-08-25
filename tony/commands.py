from __future__ import annotations

import json
import sys

from . import ghidra_ops
from .assets import (
    assets_extract_hed,  # noqa: F401 - command handlers are consumed by cli.py
    assets_extract_pkr,  # noqa: F401 - command handlers are consumed by cli.py
    assets_extract_pre,  # noqa: F401 - command handlers are consumed by cli.py
    assets_extract_psx,  # noqa: F401 - command handlers are consumed by cli.py
    assets_inspect_hed,  # noqa: F401 - command handlers are consumed by cli.py
    assets_inspect_pkr,  # noqa: F401 - command handlers are consumed by cli.py
    assets_inspect_pre,  # noqa: F401 - command handlers are consumed by cli.py
    assets_inspect_psx,  # noqa: F401 - command handlers are consumed by cli.py
    assets_inspect_trg,  # noqa: F401 - command handlers are consumed by cli.py
    assets_inventory,  # noqa: F401 - command handlers are consumed by cli.py
)
from .common import capture, load_yaml, resolve, sha256
from .debug import debug_game  # noqa: F401 - command handlers are consumed by cli.py
from .explorer import assets_explore  # noqa: F401 - command handlers are consumed by cli.py
from .gdb_knowledge import generate as generate_gdb_knowledge
from .ghidra_setup import install_ghidra
from .media import (
    _convert_raw_cd,  # noqa: F401 - retained as a compatibility import for tooling/tests
    _detect_media_format,  # noqa: F401 - retained as a compatibility import for tooling/tests
    media_extract,  # noqa: F401 - command handlers are consumed by cli.py
    media_identify,  # noqa: F401 - command handlers are consumed by cli.py
    media_list,  # noqa: F401 - command handlers are consumed by cli.py
    media_tracks,  # noqa: F401 - command handlers are consumed by cli.py
)
from .nocd import patch_nocd_executable
from .pe import exe_identify  # noqa: F401 - command handlers are consumed by cli.py
from .sessions import (  # noqa: F401 - command handlers are consumed by cli.py
    sessions_clean,
    sessions_list,
    sessions_stop,
)
from .wine import (  # noqa: F401 - public command compatibility
    _recorded_exe,
    run_game,
    wine_init,
    wine_mount_disc,
    wine_unmount_disc,
)


def play_game(args) -> int:
    """Mount the generated disc when needed, then launch the recorded game."""

    _recorded_exe()
    mount_status = wine_mount_disc(args)
    if mount_status:
        return mount_status
    return run_game(args)


def exe_patch_nocd(args) -> int:
    output = resolve(args.output) if args.output else None
    patch_nocd_executable(output)
    return 0


def doctor(_args) -> int:
    checks: list[tuple[str, bool, str]] = []

    checks.append(("python", sys.version_info >= (3, 12), sys.version.split()[0]))

    for name, command in (
        ("git", ["git", "--version"]),
        ("java", ["java", "-version"]),
        ("wine", ["wine", "--version"]),
        ("winedbg", ["winedbg", "--help"]),
        ("gdb", ["gdb", "--version"]),
        ("7z", ["7z"]),
        ("file", ["file", "--version"]),
        ("xorriso", ["xorriso", "-version"]),
        ("jq", ["jq", "--version"]),
        ("rg", ["rg", "--version"]),
        ("cmake", ["cmake", "--version"]),
        ("ninja", ["ninja", "--version"]),
        ("clang", ["clang", "--version"]),
    ):
        status, output = capture(command)
        checks.append((name, status == 0, output.splitlines()[0] if output else "not found"))

    gdb_status, gdb_python = capture(["gdb", "-q", "-nx", "-batch", "-ex", "python import sys; print(sys.version)"])
    checks.append(("gdb-python", gdb_status == 0 and bool(gdb_python), gdb_python.splitlines()[-1] if gdb_python else "unavailable"))

    ghidra_cfg = load_yaml("re/config/ghidra.yml")
    ghidra = resolve(ghidra_cfg["ghidra"]["install_dir"])
    checks.append(("ghidra", (ghidra / "Ghidra/application.properties").is_file(), str(ghidra)))

    try:
        import pyghidra  # noqa: F401
        pyghidra_ok = True
        pyghidra_desc = "import OK"
    except Exception as exc:  # noqa: BLE001  # pragma: no cover - environment dependent
        pyghidra_ok = False
        pyghidra_desc = str(exc)
    checks.append(("pyghidra", pyghidra_ok, pyghidra_desc))

    media = resolve(load_yaml("re/config/binaries.yml")["media"]["thps2_pc_disc"]["path"])
    checks.append(("THPS2.img", media.is_file(), str(media)))

    width = max(len(name) for name, _, _ in checks)
    failed_required = False
    optional = {"jq", "rg", "cmake", "ninja", "clang", "xorriso"}
    for name, ok, detail in checks:
        marker = "OK" if ok else ("WARN" if name in optional else "FAIL")
        print(f"{marker:4} {name:<{width}}  {detail}")
        if not ok and name not in optional:
            failed_required = True
    return 1 if failed_required else 0


def setup_ghidra(_args) -> int:
    install_ghidra()
    return 0


def gdb_generate(args) -> int:
    generate_gdb_knowledge(args.output)
    return 0


def verify(_args) -> int:
    config = load_yaml("re/config/binaries.yml")
    failed = False
    for category, entries in (("media", config.get("media", {})), ("executables", config.get("executables", {}))):
        for name, spec in entries.items():
            path_text = spec.get("path")
            if not path_text:
                print(f"WARN {category}.{name}: path not recorded")
                continue
            path = resolve(path_text)
            if not path.is_file():
                print(f"FAIL {category}.{name}: missing {path}")
                failed = True
                continue
            expected = spec.get("sha256")
            if not expected:
                print(f"WARN {category}.{name}: SHA-256 not recorded ({path})")
                continue
            actual = sha256(path)
            if actual != expected:
                print(f"FAIL {category}.{name}: SHA-256 mismatch\n  expected {expected}\n  actual   {actual}")
                failed = True
            else:
                print(f"OK   {category}.{name}: {actual}")
    return 1 if failed else 0


def ghidra_rebuild(_args) -> int:
    ghidra_ops.rebuild()
    return 0


def ghidra_export_functions(args) -> int:
    output = resolve(args.output) if args.output else None
    ghidra_ops.export_functions(output)
    return 0


def ghidra_decompile(args) -> int:
    output = resolve(args.output) if args.output else None
    ghidra_ops.decompile_function(args.address, output)
    return 0


def experiments_list(_args) -> int:
    data = load_yaml("re/experiments/manifest.yml")
    for experiment in data.get("experiments", []):
        print(f"{experiment['name']:<24} {experiment.get('status', 'unknown'):<10} {experiment.get('purpose', '')}")
    return 0


def compare_traces(args) -> int:
    left = resolve(args.left)
    right = resolve(args.right)
    if not left.is_file() or not right.is_file():
        raise SystemExit("both trace files must exist")

    def lines(path):
        with path.open("r", encoding="utf-8") as stream:
            for number, line in enumerate(stream, 1):
                if line.strip():
                    yield number, json.loads(line)

    sentinel = object()
    li, ri = iter(lines(left)), iter(lines(right))
    index = 0
    while True:
        a = next(li, sentinel)
        b = next(ri, sentinel)
        if a is sentinel and b is sentinel:
            print(f"MATCH: {index} JSONL records")
            return 0
        index += 1
        if a is sentinel or b is sentinel:
            print(f"DIFF record {index}: trace lengths differ")
            return 1
        if a[1] != b[1]:
            print(f"DIFF record {index}")
            print("left : " + json.dumps(a[1], sort_keys=True))
            print("right: " + json.dumps(b[1], sort_keys=True))
            return 1
