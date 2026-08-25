from __future__ import annotations

import json
import os
import re
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import pefile

from . import __version__, ghidra_ops
from .common import ROOT, capture, load_yaml, relative_to_root, resolve, save_yaml, sha256, wine_env
from .ghidra_setup import install_ghidra

_RAW_CD_SECTOR_SIZE = 2352
_ISO_SECTOR_SIZE = 2048
_RAW_CD_SYNC = b"\x00" + b"\xff" * 10 + b"\x00"
_ISO_PVD_SIGNATURE = b"\x01CD001"


def _read_at(path: Path, offset: int, size: int) -> bytes:
    with path.open("rb") as stream:
        stream.seek(offset)
        return stream.read(size)


def _pvd_data_sectors(pvd: bytes) -> int | None:
    if len(pvd) < 88 or pvd[:6] != _ISO_PVD_SIGNATURE:
        return None
    little = int.from_bytes(pvd[80:84], "little")
    big = int.from_bytes(pvd[84:88], "big")
    return little if little == big and little > 0 else None


def _detect_media_format(path: Path) -> dict[str, int | str | None]:
    """Identify an ISO, raw CD image, or opaque archive/container."""

    standard_pvd_offset = 16 * _ISO_SECTOR_SIZE
    standard_pvd = _read_at(path, standard_pvd_offset, _ISO_SECTOR_SIZE)
    if standard_pvd[:len(_ISO_PVD_SIGNATURE)] == _ISO_PVD_SIGNATURE:
        return {
            "format": "iso9660",
            "sector_size": _ISO_SECTOR_SIZE,
            "user_data_offset": 0,
            "user_data_size": _ISO_SECTOR_SIZE,
            "pvd_sector": 16,
            "data_sectors": _pvd_data_sectors(standard_pvd),
        }

    if path.stat().st_size % _RAW_CD_SECTOR_SIZE == 0:
        raw_sector = _read_at(path, 16 * _RAW_CD_SECTOR_SIZE, _RAW_CD_SECTOR_SIZE)
        if raw_sector[:12] == _RAW_CD_SYNC:
            mode = raw_sector[15]
            if mode == 1 and raw_sector[16:22] == _ISO_PVD_SIGNATURE:
                pvd = raw_sector[16:16 + _ISO_SECTOR_SIZE]
                return {
                    "format": "raw-cd-mode1",
                    "sector_size": _RAW_CD_SECTOR_SIZE,
                    "user_data_offset": 16,
                    "user_data_size": _ISO_SECTOR_SIZE,
                    "pvd_sector": 16,
                    "data_sectors": _pvd_data_sectors(pvd),
                }
            if mode == 2 and raw_sector[24:30] == _ISO_PVD_SIGNATURE:
                pvd = raw_sector[24:24 + _ISO_SECTOR_SIZE]
                return {
                    "format": "raw-cd-mode2-form1",
                    "sector_size": _RAW_CD_SECTOR_SIZE,
                    "user_data_offset": 24,
                    "user_data_size": _ISO_SECTOR_SIZE,
                    "pvd_sector": 16,
                    "data_sectors": _pvd_data_sectors(pvd),
                }

    return {
        "format": "container-or-archive",
        "sector_size": None,
        "user_data_offset": None,
        "user_data_size": None,
        "pvd_sector": None,
        "data_sectors": None,
    }


def _convert_raw_cd(source: Path, destination: Path, media_format: dict[str, int | str | None]) -> None:
    sector_size = int(media_format["sector_size"])
    user_data_offset = int(media_format["user_data_offset"])
    user_data_size = int(media_format["user_data_size"])
    total_sector_count, remainder = divmod(source.stat().st_size, sector_size)
    if remainder:
        raise RuntimeError(f"raw image size is not divisible by {sector_size}: {source}")
    sector_count = int(media_format.get("data_sectors") or total_sector_count)
    if sector_count > total_sector_count:
        raise RuntimeError(f"ISO volume exceeds raw image: {sector_count} > {total_sector_count} sectors")
    expected_mode = 1 if media_format["format"] == "raw-cd-mode1" else 2

    destination.parent.mkdir(parents=True, exist_ok=True)
    with source.open("rb") as input_stream, destination.open("wb") as output_stream:
        for index in range(sector_count):
            sector = input_stream.read(sector_size)
            if len(sector) != sector_size:
                raise RuntimeError(f"short raw sector {index} in {source}")
            if sector[:12] != _RAW_CD_SYNC:
                raise RuntimeError(f"invalid raw CD sync header at sector {index}")
            if sector[15] != expected_mode:
                raise RuntimeError(f"unexpected raw CD mode at sector {index}: {sector[15]}")
            payload = sector[user_data_offset:user_data_offset + user_data_size]
            if len(payload) != user_data_size:
                raise RuntimeError(f"short user-data payload at sector {index}")
            output_stream.write(payload)

    pvd_offset = 16 * _ISO_SECTOR_SIZE
    if _read_at(destination, pvd_offset, len(_ISO_PVD_SIGNATURE)) != _ISO_PVD_SIGNATURE:
        raise RuntimeError(f"converted image has no valid ISO-9660 volume descriptor: {destination}")


def _build_output(path: Path) -> Path:
    output = path.resolve()
    build_root = (ROOT / "build").resolve()
    try:
        output.relative_to(build_root)
    except ValueError as exc:
        raise SystemExit(f"media extraction output must be under build/: {output}") from exc
    return output


def _extract_with_7z(source: Path, output: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    return subprocess.run(["7z", "x", str(source), f"-o{output}", "-y"], cwd=ROOT, check=False).returncode


def _extract_with_xorriso(source: Path, output: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    return subprocess.run(
        ["xorriso", "-osirrox", "on", "-indev", str(source), "-extract", "/", str(output)],
        cwd=ROOT,
        check=False,
    ).returncode


def _write_media_manifest(output: Path, manifest: dict) -> None:
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def _remove_generated_tree(path: Path) -> None:
    def make_writable(function, target, _error):
        target_path = Path(target)
        for candidate in (target_path, target_path.parent):
            mode = os.stat(candidate).st_mode | stat.S_IWUSR
            if candidate.is_dir():
                mode |= stat.S_IXUSR
            os.chmod(candidate, mode)
        function(target)

    shutil.rmtree(path, onexc=make_writable)


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


def media_identify(args) -> int:
    config_path = "re/config/binaries.yml"
    config = load_yaml(config_path)
    default = config["media"]["thps2_pc_disc"]["path"]
    path = resolve(args.path or default)
    if not path.is_file():
        raise SystemExit(f"media not found: {path}")
    status, description = capture(["file", "-b", str(path)])
    media_format = _detect_media_format(path)
    record = {
        "path": relative_to_root(path),
        "size": path.stat().st_size,
        "sha256": sha256(path),
        "file_description": description if status == 0 else None,
        **media_format,
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
    media_format = _detect_media_format(path)
    if str(media_format["format"]).startswith("raw-cd-"):
        with tempfile.TemporaryDirectory(prefix="opentony-media-") as temporary:
            normalized = Path(temporary) / "image.iso"
            _convert_raw_cd(path, normalized, media_format)
            return subprocess.run(["7z", "l", str(normalized)], cwd=ROOT, check=False).returncode
    return subprocess.run(["7z", "l", str(path)], cwd=ROOT, check=False).returncode


def media_extract(args) -> int:
    config = load_yaml("re/config/binaries.yml")
    spec = config["media"]["thps2_pc_disc"]
    path = resolve(args.path or spec["path"])
    if not path.is_file():
        raise SystemExit(f"media not found: {path}")
    expected_sha256 = spec.get("sha256")
    if not expected_sha256:
        raise SystemExit("media identity is not recorded; run: tony media identify --record")
    actual_sha256 = sha256(path)
    if actual_sha256 != expected_sha256:
        raise SystemExit(
            f"canonical media SHA-256 mismatch:\n  expected {expected_sha256}\n  actual   {actual_sha256}"
        )

    output = _build_output(resolve(args.output))
    if output == ROOT.resolve() or output == (ROOT / "game").resolve():
        raise SystemExit("refusing to extract into the repository or game directory; use build/")
    if output.exists() and any(output.iterdir()):
        if not args.force:
            raise SystemExit(f"output already exists; use --force to replace generated output: {output}")
        _remove_generated_tree(output)
    output.mkdir(parents=True, exist_ok=True)

    media_format = _detect_media_format(path)
    files_output = output / "files"
    normalized_iso = None
    if str(media_format["format"]).startswith("raw-cd-"):
        normalized_iso = output / f"{path.stem}.iso"
        _convert_raw_cd(path, normalized_iso, media_format)
        status = _extract_with_xorriso(normalized_iso, files_output)
    elif media_format["format"] == "iso9660":
        normalized_iso = output / f"{path.stem}.iso"
        shutil.copyfile(path, normalized_iso)
        status = _extract_with_xorriso(normalized_iso, files_output)
    else:
        status = _extract_with_7z(path, files_output)

    if status != 0:
        return status

    extractor = "xorriso" if normalized_iso is not None else "7z"
    _, version_output = capture([extractor, "--version"] if extractor == "xorriso" else [extractor])
    manifest = {
        "version": 1,
        "tony_version": __version__,
        "source": {
            "path": relative_to_root(path),
            "size": path.stat().st_size,
            "sha256": actual_sha256,
        },
        "format": media_format,
        "normalized_iso": (
            {
                "path": relative_to_root(normalized_iso),
                "size": normalized_iso.stat().st_size,
                "sha256": sha256(normalized_iso),
            }
            if normalized_iso is not None
            else None
        ),
        "extracted_path": relative_to_root(files_output),
        "extractor": {
            "name": extractor,
            "version": version_output.splitlines()[0] if version_output else None,
        },
    }
    _write_media_manifest(output, manifest)
    print(f"Extracted media into {output}")
    print(f"Manifest: {output / 'manifest.json'}")
    return 0


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
