from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import urllib.request
from pathlib import Path

from .common import ROOT, load_yaml, resolve, sha256

_CHUNK_SIZE = 1024 * 1024
_PROGRESS_INTERVAL = 16 * _CHUNK_SIZE
_DEFAULT_COMPILE_FLAGS = ["/O2", "/GX-", "/GR-"]


def _spec() -> dict:
    return load_yaml("re/config/vc6.yml")["vc6"]


def _download_verified(media: dict) -> Path:
    target = resolve(media["path"])
    expected_size = int(media["size"])
    expected_sha256 = media["sha256"]
    if expected_sha256 == "pending":
        raise SystemExit(f"VC6 media SHA-256 has not been recorded: {target.name}")

    if target.is_file():
        if target.stat().st_size == expected_size and sha256(target) == expected_sha256:
            print(f"VC6 media already downloaded: {target}")
            return target
        raise SystemExit(f"refusing to replace VC6 media with unexpected contents: {target}")
    if target.exists():
        raise SystemExit(f"VC6 media destination is not a regular file: {target}")

    target.parent.mkdir(parents=True, exist_ok=True)
    partial: Path | None = None
    digest = hashlib.sha256()
    size = 0
    print(f"Downloading {media['url']}\n       -> {target}")
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=target.parent, prefix=f".{target.name}.", suffix=".part", delete=False
        ) as output:
            partial = Path(output.name)
            with urllib.request.urlopen(media["url"], timeout=60) as response:
                next_progress = _PROGRESS_INTERVAL
                while chunk := response.read(_CHUNK_SIZE):
                    output.write(chunk)
                    digest.update(chunk)
                    size += len(chunk)
                    if size >= next_progress:
                        print(f"Downloaded {size / _CHUNK_SIZE:.1f} MiB", flush=True)
                        next_progress += _PROGRESS_INTERVAL
        if size != expected_size:
            raise SystemExit(
                f"download size mismatch for {target.name}: expected {expected_size}, got {size}"
            )
        actual_sha256 = digest.hexdigest()
        if actual_sha256 != expected_sha256:
            raise SystemExit(
                f"download SHA-256 mismatch for {target.name}: expected {expected_sha256}, got {actual_sha256}"
            )
        os.replace(partial, target)
        partial = None
    except BaseException:
        if partial is not None:
            partial.unlink(missing_ok=True)
        raise
    print(f"Downloaded and verified: {target}")
    return target


def _wine_environment(prefix: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["WINEPREFIX"] = str(prefix)
    env.setdefault("WINEDEBUG", "-all")
    env.setdefault("WINEDLLOVERRIDES", "mscoree,mshtml=")
    return env


def _initialize_prefix(prefix: Path) -> None:
    if (prefix / "system.reg").is_file():
        return
    prefix.parent.mkdir(parents=True, exist_ok=True)
    command = ["wineboot", "-i"]
    if shutil.which("xvfb-run"):
        command = ["xvfb-run", "-a", *command]
    subprocess.run(command, cwd=ROOT, env=_wine_environment(prefix), check=True)
    subprocess.run(["wineserver", "-k"], cwd=ROOT, env=_wine_environment(prefix), check=False)
    try:
        subprocess.run(["wineserver", "-w"], cwd=ROOT, env=_wine_environment(prefix), timeout=10, check=False)
    except subprocess.TimeoutExpired:
        pass


def _overlay_case_insensitive(source: Path, destination: Path) -> None:
    """Overlay a Windows tree while preserving the base ISO's path casing."""

    for source_path in source.rglob("*"):
        relative = source_path.relative_to(source)
        target = destination
        for component in relative.parts:
            match = next(
                (child for child in target.iterdir() if child.name.casefold() == component.casefold()),
                None,
            )
            target = match if match is not None else target / component
        if source_path.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source_path, target)


def _tool_output(tool: Path, prefix: Path) -> str:
    result = subprocess.run(
        ["wine", str(tool), "/?"],
        cwd=ROOT,
        env=_wine_environment(prefix),
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    return result.stdout


def verify_vc6() -> dict[str, str]:
    spec = _spec()
    install = resolve(spec["install_dir"])
    prefix = resolve(spec["prefix"])
    cl = install / "VC98/BIN/CL.EXE"
    link = install / "VC98/BIN/LINK.EXE"
    for tool in (cl, link):
        if not tool.is_file():
            raise SystemExit(f"VC6 toolchain is not provisioned; missing {tool}\nRun: tony setup vc6")

    compiler_output = _tool_output(cl, prefix)
    linker_output = _tool_output(link, prefix)
    compiler_version = str(spec["compiler_version"])
    linker_version = str(spec["linker_version"])
    if compiler_version not in compiler_output:
        raise SystemExit(
            f"unexpected VC6 compiler version; expected {compiler_version}\n{compiler_output.strip()}"
        )
    if linker_version not in linker_output:
        raise SystemExit(f"unexpected VC6 linker version; expected {linker_version}\n{linker_output.strip()}")
    return {"compiler": compiler_version, "linker": linker_version}


def _wine_path(path: Path, env: dict[str, str]) -> str:
    return subprocess.run(
        ["winepath", "-w", str(path.resolve())],
        env=env,
        text=True,
        capture_output=True,
        check=True,
    ).stdout.strip()


def _compiler_environment(install: Path, prefix: Path) -> dict[str, str]:
    vc98 = install / "VC98"
    env = _wine_environment(prefix)
    env["PATH"] = f"{vc98 / 'BIN'}{os.pathsep}{env.get('PATH', '')}"
    env["INCLUDE"] = _wine_path(vc98 / "INCLUDE", env)
    env["LIB"] = _wine_path(vc98 / "LIB", env)
    return env


def compile_vc6(source: Path, output: Path, flags: list[str] | None = None) -> Path:
    spec = _spec()
    install = resolve(spec["install_dir"])
    prefix = resolve(spec["prefix"])
    verify_vc6()
    if not source.is_file():
        raise SystemExit(f"VC6 source not found: {source}")
    output.parent.mkdir(parents=True, exist_ok=True)
    env = _compiler_environment(install, prefix)
    command = [
        "wine",
        str(install / "VC98/BIN/CL.EXE"),
        "/nologo",
        "/c",
        *(flags if flags is not None else _DEFAULT_COMPILE_FLAGS),
        _wine_path(source, env),
        f"/Fo{_wine_path(output, env)}",
    ]
    result = subprocess.run(
        command,
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0 or not output.is_file():
        raise SystemExit(f"VC6 compilation failed ({result.returncode}):\n{result.stdout.strip()}")
    return output


def _extract_coff_text(obj: Path, output: Path) -> Path:
    if shutil.which("objcopy") is None:
        raise SystemExit("objcopy is required to extract VC6 COFF sections")
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        ["objcopy", "-O", "binary", "--only-section=.text", str(obj), str(output)],
        cwd=ROOT,
        check=True,
    )
    if not output.is_file():
        raise SystemExit(f"objcopy did not produce a .text image: {output}")
    return output


def _matching_module(selector: str) -> dict:
    manifest = load_yaml("match/manifest.yml")
    matches = [module for module in manifest.get("modules", []) if module["id"] == selector]
    if not matches:
        try:
            address = int(selector, 0)
        except ValueError:
            address = -1
        matches = [module for module in manifest.get("modules", []) if int(module["start_va"]) == address]
    if len(matches) != 1:
        raise SystemExit(f"could not identify one split module from {selector!r}")
    return matches[0]


def compare_vc6_module(selector: str, source: Path | None = None, flags: list[str] | None = None) -> bool:
    module = _matching_module(selector)
    source_value = source or (resolve(module["cpp_source"]) if module.get("cpp_source") else None)
    if source_value is None:
        raise SystemExit(f"module has no cpp_source; pass --source: {module['id']}")
    compile_flags = flags if flags is not None else list(module.get("cpp_flags", _DEFAULT_COMPILE_FLAGS))
    output_dir = resolve("build/vc6")
    obj = compile_vc6(source_value, output_dir / f"{module['id']}.obj", compile_flags)
    generated = _extract_coff_text(obj, output_dir / f"{module['id']}.text.bin")
    original = resolve(f"match/original/modules/{module['id']}.bin")
    if not original.is_file():
        raise SystemExit(f"original module bytes not found: {original}; run: tony split extract")

    expected = original.read_bytes()
    actual = generated.read_bytes()
    if actual == expected:
        print(f"MATCH {module['id']}: {len(actual)} bytes ({' '.join(compile_flags)})")
        return True
    if len(actual) >= len(expected) and actual[: len(expected)] == expected and all(
        byte == 0x90 for byte in actual[len(expected) :]
    ):
        print(
            f"MATCH {module['id']}: {len(expected)} bytes plus {len(actual) - len(expected)} "
            f"COFF alignment NOPs ({' '.join(compile_flags)})"
        )
        return True
    common = 0
    for left, right in zip(expected, actual, strict=False):
        if left != right:
            break
        common += 1
    if common < min(len(expected), len(actual)):
        detail = f"expected 0x{expected[common]:02x}, got 0x{actual[common]:02x}"
    else:
        detail = "common prefix ends at the shorter image"
    print(
        f"MISMATCH {module['id']}: expected {len(expected)} bytes, got {len(actual)}; "
        f"first difference +0x{common:x} ({detail})"
    )
    return False


def _compile_probe(install: Path, prefix: Path) -> Path:
    probe_dir = install / "probe"
    probe_dir.mkdir(parents=True, exist_ok=True)
    source = probe_dir / "probe.c"
    source.write_text("int main(void) { return 0; }\n", encoding="ascii")
    output = probe_dir / "probe.exe"
    obj = compile_vc6(source, probe_dir / "probe.obj")
    env = _compiler_environment(install, prefix)
    result = subprocess.run(
        [
            "wine",
            str(install / "VC98/BIN/LINK.EXE"),
            "/nologo",
            "/subsystem:console",
            _wine_path(obj, env),
            f"/out:{_wine_path(output, env)}",
        ],
        cwd=probe_dir,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if result.returncode != 0 or not output.is_file():
        raise SystemExit(f"VC6 probe link failed ({result.returncode}):\n{result.stdout.strip()}")
    return output


def provision_vc6() -> Path:
    spec = _spec()
    for command in ("7z", "wine", "wineboot", "winepath", "wineserver"):
        if shutil.which(command) is None:
            raise SystemExit(f"{command} is required to provision VC6")

    install = resolve(spec["install_dir"])
    prefix = resolve(spec["prefix"])
    manifest_path = install / "manifest.json"
    if manifest_path.is_file():
        versions = verify_vc6()
        print(f"VC6 already provisioned: {install} (CL {versions['compiler']}, LINK {versions['linker']})")
        return install

    base = _download_verified(spec["base"])
    sp3 = _download_verified(spec["sp3"])
    staging = resolve(".tools/vc6-staging")
    shutil.rmtree(staging, ignore_errors=True)
    base_tree = staging / "base"
    patch_tree = staging / "sp3"
    subprocess.run(
        [
            "7z",
            "x",
            str(base),
            f"-o{base_tree}",
            "VC98/*",
            "COMMON/MSDEV98/BIN/MSOBJ10.DLL",
            "COMMON/MSDEV98/BIN/MSPDB60.DLL",
            "-y",
        ],
        cwd=ROOT,
        check=True,
    )
    subprocess.run(["7z", "x", str(sp3), f"-o{patch_tree}", "enu/vc98/*", "-y"], cwd=ROOT, check=True)

    candidate = staging / "install"
    shutil.copytree(base_tree / "VC98", candidate / "VC98")
    for runtime in ("MSOBJ10.DLL", "MSPDB60.DLL"):
        shutil.copy2(base_tree / "COMMON/MSDEV98/BIN" / runtime, candidate / "VC98/BIN" / runtime)
    _overlay_case_insensitive(patch_tree / "enu/vc98", candidate / "VC98")
    shutil.rmtree(install, ignore_errors=True)
    candidate.rename(install)
    shutil.rmtree(staging, ignore_errors=True)

    _initialize_prefix(prefix)
    versions = verify_vc6()
    probe = _compile_probe(install, prefix)
    manifest = {
        "version": 1,
        "base_sha256": sha256(base),
        "sp3_sha256": sha256(sp3),
        "compiler_version": versions["compiler"],
        "linker_version": versions["linker"],
        "probe_sha256": sha256(probe),
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Provisioned VC6 SP3 toolchain: {install}")
    print(f"Verified CL {versions['compiler']} and LINK {versions['linker']}")
    print(f"Compiled probe: {probe}")
    return install


def setup_vc6(_args) -> int:
    provision_vc6()
    return 0


def vc6_verify(_args) -> int:
    versions = verify_vc6()
    print(f"CL {versions['compiler']}")
    print(f"LINK {versions['linker']}")
    return 0


def vc6_compile(args) -> int:
    source = resolve(args.source)
    output = resolve(args.output) if args.output else resolve(f"build/vc6/{source.stem}.obj")
    compiled = compile_vc6(source, output, args.flag or None)
    print(f"Compiled VC6 object: {compiled}")
    return 0


def vc6_compare(args) -> int:
    source = resolve(args.source) if args.source else None
    return 0 if compare_vc6_module(args.module, source, args.flag or None) else 1
