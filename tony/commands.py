from __future__ import annotations

import json
import sys
from types import SimpleNamespace

from . import ghidra_inspect as ghidra_inspection
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
from .media_setup import install_media
from .native_progress import native_verify
from .nocd import patch_nocd_executable
from .pe import exe_identify  # noqa: F401 - command handlers are consumed by cli.py
from .recording import (
    record_start,  # noqa: F401 - command handlers are consumed by cli.py
    record_status,  # noqa: F401 - command handlers are consumed by cli.py
    record_stop,  # noqa: F401 - command handlers are consumed by cli.py
    record_toggle,  # noqa: F401 - command handlers are consumed by cli.py
    record_validate,  # noqa: F401 - command handlers are consumed by cli.py
)
from .recovered_types import types_verify
from .sessions import (  # noqa: F401 - command handlers are consumed by cli.py
    sessions_clean,
    sessions_list,
    sessions_prune,
    sessions_stop,
)
from .slices import (  # noqa: F401 - command handlers are consumed by cli.py
    slice_claim,
    slice_list,
    slice_prompt,
    slice_release,
    slice_show,
    slice_status,
    slice_verify,
)
from .split import (  # noqa: F401 - command handlers are consumed by cli.py
    split_accept_proposal,
    split_accept_proposals,
    split_build,
    split_compare,
    split_coverage,
    split_extract,
    split_init,
    split_module,
    split_propose_modules,
    split_rebuild,
    split_symbols,
    split_verify,
)
from .vc6 import (  # noqa: F401 - command handlers are consumed by cli.py
    setup_vc6,
    vc6_compare,
    vc6_compile,
    vc6_verify,
)
from .wine import (  # noqa: F401 - public command compatibility
    _recorded_exe,
    run_game,
    wine_init,
    wine_mount_disc,
    wine_unmount_disc,
)
from .worktrees import (  # noqa: F401 - command handlers are consumed by cli.py
    prerequisites_bootstrap,
    worktree_prepare,
    worktree_verify,
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
        ("nasm", ["nasm", "-v"]),
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


def setup_media(_args) -> int:
    install_media()
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
    if types_verify(_args):
        failed = True
    if native_verify(_args):
        failed = True
    if slice_verify(_args):
        failed = True
    if getattr(_args, "all", False):
        split_args = SimpleNamespace(
            no_build=False,
            output="match/generated/THawk2.rebuilt.exe",
        )
        if split_rebuild(split_args) or split_verify(split_args):
            failed = True
        if not ghidra_ops.verify():
            failed = True
    return 1 if failed else 0


def ghidra_rebuild(args) -> int:
    ghidra_ops.rebuild(args.profile)
    return 0


def ghidra_sync(args) -> int:
    ghidra_ops.sync(args.function, args.force)
    return 0


def ghidra_verify(_args) -> int:
    return 0 if ghidra_ops.verify() else 1


def ghidra_inspect(args) -> int:
    ghidra_inspection.inspect_function(args.address, resolve(args.output) if args.output else None)
    return 0


def ghidra_gaps(args) -> int:
    ghidra_inspection.gaps(resolve(args.output) if args.output else None, args.limit, args.slice_id)
    return 0


def ghidra_export_functions(args) -> int:
    output = resolve(args.output) if args.output else None
    ghidra_inspection.export_functions(output)
    return 0


def ghidra_decompile(args) -> int:
    output = resolve(args.output) if args.output else None
    ghidra_inspection.decompile_function(args.address, output)
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
