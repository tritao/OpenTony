from __future__ import annotations

import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

import pefile

from .common import ROOT, capture, load_yaml, relative_to_root, resolve, save_yaml, sha256, which, wine_env
from .ghidra_setup import install_ghidra
from . import ghidra_ops


def _version_major(text: str) -> int | None:
    match = re.search(r"(\d+)(?:\.\d+)?", text)
    return int(match.group(1)) if match else None


def doctor(_args) -> int:
    checks: list[tuple[str, bool, str]] = []

    checks.append(("python", sys.version_info >= (3, 10), sys.version.split()[0]))

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
    except Exception as exc:  # pragma: no cover - environment dependent
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


def media_identify(args) -> int:
    config_path = "re/config/binaries.yml"
    config = load_yaml(config_path)
    default = config["media"]["thps2_pc_disc"]["path"]
    path = resolve(args.path or default)
    if not path.is_file():
        raise SystemExit(f"media not found: {path}")
    status, description = capture(["file", "-b", str(path)])
    record = {
        "path": relative_to_root(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "file_description": description if status == 0 else None,
    }
    print(json.dumps(record, indent=2))
    if args.record:
        config["media"]["thps2_pc_disc"].update(record)
        save_yaml(config_path, config)
        print(f"Recorded identity in {config_path}")
    return 0


def media_list(args) -> int:
    config = load_yaml("re/config/binaries.yml")
    path = resolve(args.path or config["media"]["thps2_pc_disc"]["path"])
    if not path.is_file():
        raise SystemExit(f"media not found: {path}")
    return subprocess.run(["7z", "l", str(path)], cwd=ROOT, check=False).returncode


def media_extract(args) -> int:
    config = load_yaml("re/config/binaries.yml")
    path = resolve(args.path or config["media"]["thps2_pc_disc"]["path"])
    output = resolve(args.output)
    if output.resolve() == path.parent.resolve():
        raise SystemExit("refusing to extract beside/over canonical media; use build/")
    output.mkdir(parents=True, exist_ok=True)
    return subprocess.run(["7z", "x", str(path), f"-o{output}", "-y"], cwd=ROOT, check=False).returncode


def exe_identify(args) -> int:
    path = resolve(args.path)
    if not path.is_file():
        raise SystemExit(f"executable not found: {path}")
    pe = pefile.PE(str(path), fast_load=True)
    image_base = int(pe.OPTIONAL_HEADER.ImageBase)
    ep_rva = int(pe.OPTIONAL_HEADER.AddressOfEntryPoint)
    machine = f"0x{int(pe.FILE_HEADER.Machine):04x}"
    record = {
        "path": relative_to_root(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "machine": machine,
        "pe_timestamp": int(pe.FILE_HEADER.TimeDateStamp),
        "image_base": image_base,
        "entry_point_rva": ep_rva,
        "entry_point_va": image_base + ep_rva,
    }
    print(json.dumps(record, indent=2))
    if args.record:
        config_path = "re/config/binaries.yml"
        config = load_yaml(config_path)
        config["executables"]["thps2_pc"].update(record)
        save_yaml(config_path, config)
        print(f"Recorded identity in {config_path}")
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


def wine_init(_args) -> int:
    env = wine_env()
    prefix = Path(env["WINEPREFIX"])
    prefix.mkdir(parents=True, exist_ok=True)
    print(f"Initializing Wine prefix: {prefix}")
    return subprocess.run(["wineboot", "-u"], cwd=ROOT, env=env, check=False).returncode


def _recorded_exe() -> Path:
    path = load_yaml("re/config/binaries.yml")["executables"]["thps2_pc"].get("path")
    if not path:
        raise SystemExit("No executable recorded. Run: tony exe identify <path> --record")
    exe = resolve(path)
    if not exe.is_file():
        raise SystemExit(f"recorded executable missing: {exe}")
    return exe


def run_game(args) -> int:
    exe = _recorded_exe()
    env = wine_env()
    command = ["wine", str(exe), *args.game_args]
    print(" ".join(command))
    return subprocess.run(command, cwd=exe.parent, env=env, check=False).returncode


def _wait_port(port: int, timeout: float = 10.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with socket.socket() as sock:
            sock.settimeout(0.2)
            if sock.connect_ex(("127.0.0.1", port)) == 0:
                return
        time.sleep(0.1)
    raise RuntimeError(f"WineDbg GDB proxy did not open port {port}")


def debug_game(args) -> int:
    exe = _recorded_exe()
    cfg = load_yaml("re/config/wine.yml")["wine"]
    port = int(args.port or cfg["debug_port"])
    env = wine_env()
    proxy = subprocess.Popen(
        ["winedbg", "--gdb", "--no-start", "--port", str(port), str(exe), *args.game_args],
        cwd=exe.parent,
        env=env,
    )
    try:
        _wait_port(port)
        gdb_cmd = [
            "gdb", "-q", "-nx",
            "-ex", f"target remote localhost:{port}",
            "-x", str(ROOT / "re/gdb/bootstrap.gdb"),
        ]
        return subprocess.run(gdb_cmd, cwd=ROOT, env=os.environ.copy(), check=False).returncode
    finally:
        proxy.terminate()
        try:
            proxy.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proxy.kill()


def ghidra_rebuild(_args) -> int:
    ghidra_ops.rebuild()
    return 0


def ghidra_export_functions(args) -> int:
    output = resolve(args.output) if args.output else None
    ghidra_ops.export_functions(output)
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

    def lines(path: Path):
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
