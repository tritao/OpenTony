from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

from .common import ROOT, load_yaml, resolve


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def _java_major() -> int | None:
    try:
        result = subprocess.run(["java", "-version"], capture_output=True, text=True, check=False)
    except FileNotFoundError:
        return None
    text = result.stderr + result.stdout
    match = re.search(r'version "([0-9]+)', text)
    return int(match.group(1)) if match else None


def _safe_extract(archive: Path, destination: Path) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    root = destination.resolve()
    with zipfile.ZipFile(archive) as zipped:
        for member in zipped.infolist():
            target = (destination / member.filename).resolve()
            if target != root and root not in target.parents:
                raise RuntimeError(f"unsafe archive member: {member.filename}")
        zipped.extractall(destination)


def _ensure_executable_bits(install: Path) -> None:
    for rel in (
        "ghidraRun",
        "support/analyzeHeadless",
        "support/pyghidraRun",
        "support/launch.sh",
        "Ghidra/Features/Decompiler/os/linux_x86_64/sleigh",
        "Ghidra/Features/Decompiler/os/linux_x86_64/decompile",
    ):
        path = install / rel
        if path.is_file():
            path.chmod(path.stat().st_mode | 0o111)


def install_ghidra() -> Path:
    config = load_yaml("re/config/ghidra.yml")
    spec = config["ghidra"]
    required_java = int(config["java"]["major"])
    detected_java = _java_major()
    if detected_java is None or detected_java < required_java:
        raise SystemExit(f"JDK {required_java}+ is required before provisioning Ghidra; detected {detected_java!r}")

    install = resolve(spec["install_dir"])
    properties = install / "Ghidra/application.properties"
    if properties.is_file() and f"application.version={spec['version']}" in properties.read_text(errors="ignore"):
        _ensure_executable_bits(install)
        _install_pyghidra(install)
        print(f"Ghidra already installed: {install}")
        return install

    downloads = ROOT / ".tools/downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    archive = downloads / spec["release_asset"]
    url = f"https://github.com/NationalSecurityAgency/ghidra/releases/download/{spec['release_tag']}/{spec['release_asset']}"

    if not archive.is_file() or _sha256(archive) != spec["sha256"]:
        partial = archive.with_suffix(archive.suffix + ".part")
        partial.unlink(missing_ok=True)
        print(f"Downloading {url}")
        urllib.request.urlretrieve(url, partial)
        actual = _sha256(partial)
        if actual != spec["sha256"]:
            partial.unlink(missing_ok=True)
            raise SystemExit(f"Ghidra SHA-256 mismatch: expected {spec['sha256']}, got {actual}")
        partial.replace(archive)

    staging = ROOT / ".tools/ghidra-extract"
    shutil.rmtree(staging, ignore_errors=True)
    _safe_extract(archive, staging)
    dirs = [item for item in staging.iterdir() if item.is_dir()]
    if len(dirs) != 1:
        raise SystemExit(f"could not identify extracted Ghidra directory in {archive}")
    shutil.rmtree(install, ignore_errors=True)
    dirs[0].rename(install)
    shutil.rmtree(staging, ignore_errors=True)
    _ensure_executable_bits(install)
    _install_pyghidra(install)
    print(f"Installed Ghidra {spec['version']}: {install}")
    return install


def _install_pyghidra(install: Path) -> None:
    # Bootstrap script installs OpenTony into .tools/venv. If setup is invoked
    # elsewhere, use the current interpreter so PyGhidra and `tony` coexist.
    python = Path(sys.executable)
    dist = install / "Ghidra/Features/PyGhidra/pypkg/dist"
    subprocess.run(
        [str(python), "-m", "pip", "install", "--disable-pip-version-check", "--no-index", "-f", str(dist), "pyghidra"],
        check=True,
    )
    stubs = install / "docs/ghidra_stubs"
    if stubs.is_dir():
        subprocess.run(
            [str(python), "-m", "pip", "install", "--disable-pip-version-check", "--no-index", "-f", str(stubs), "ghidra-stubs"],
            check=False,
        )
