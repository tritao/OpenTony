from __future__ import annotations

import shutil
from pathlib import Path

from .common import ROOT, capture, load_yaml, resolve
from .ghidra_setup import _install_pyghidra


def _primary_worktree() -> Path:
    returncode, output = capture(["git", "worktree", "list", "--porcelain"])
    if returncode:
        raise SystemExit("could not list Git worktrees")
    entries: list[tuple[Path, str | None]] = []
    path: Path | None = None
    branch: str | None = None
    for line in [*output.splitlines(), ""]:
        if line.startswith("worktree "):
            if path is not None:
                entries.append((path, branch))
            path = Path(line.removeprefix("worktree ")).resolve()
            branch = None
        elif line.startswith("branch "):
            branch = line.removeprefix("branch ")
        elif not line and path is not None:
            entries.append((path, branch))
            path = None
            branch = None
    for candidate, candidate_branch in entries:
        if candidate_branch == "refs/heads/main":
            return candidate
    if entries:
        return entries[0][0]
    raise SystemExit("no Git worktree found")


def _shared_paths() -> list[Path]:
    ghidra = Path(load_yaml("re/config/ghidra.yml")["ghidra"]["install_dir"])
    return [ghidra, Path("game/THPS2.img"), Path("build/disc"), Path("match/original")]


def _path_ready(root: Path, relative: Path) -> bool:
    config = load_yaml("re/config/ghidra.yml")["ghidra"]
    binaries = load_yaml("re/config/binaries.yml")
    if relative == Path(config["install_dir"]):
        return (root / relative / "Ghidra/application.properties").is_file()
    if relative == Path("game/THPS2.img"):
        target = root / relative
        return target.is_file() and target.stat().st_size == binaries["media"]["thps2_pc_disc"]["size"]
    if relative == Path("build/disc"):
        exe = root / binaries["executables"]["thps2_pc"]["path"]
        return exe.is_file() and exe.stat().st_size == binaries["executables"]["thps2_pc"]["size"]
    if relative == Path("match/original"):
        expected = len(load_yaml("match/manifest.yml").get("modules", []))
        return expected > 0 and len(list((root / relative / "modules").glob("*.bin"))) == expected
    return (root / relative).exists()


def _link_shared(source_root: Path, relative: Path) -> None:
    source = source_root / relative
    target = ROOT / relative
    if _path_ready(ROOT, relative):
        print(f"READY {relative}")
        return
    if target.exists() or target.is_symlink():
        raise SystemExit(f"worktree prerequisite exists but is incomplete: {target}")
    if not _path_ready(source_root, relative):
        raise SystemExit(f"primary worktree prerequisite is missing: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.symlink_to(source, target_is_directory=source.is_dir())
    print(f"LINK  {relative} -> {source}")


def _seed_ghidra_project(source_root: Path) -> None:
    relative = Path(load_yaml("re/config/ghidra.yml")["ghidra"]["project_dir"])
    source = source_root / relative
    target = ROOT / relative
    if target.exists():
        print(f"READY {relative}")
        return
    if not (source / "recovered-types.json").is_file():
        raise SystemExit(f"primary Ghidra project is not ready: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, target, copy_function=_copy_reflink)
    print(f"COPY  {relative} (private worktree project)")


def _copy_reflink(source: str, target: str) -> str:
    returncode, _output = capture(["cp", "--reflink=auto", "--preserve=mode,timestamps", source, target])
    if returncode:
        return shutil.copy2(source, target)
    return target


def _readiness() -> list[tuple[str, bool]]:
    config = load_yaml("re/config/ghidra.yml")["ghidra"]
    checks = [(str(path), _path_ready(ROOT, path)) for path in _shared_paths()]
    project = resolve(config["project_dir"])
    project_ready = (
        (project / "recovered-types.json").is_file()
        and (project / f"{config['project_name']}.gpr").is_file()
        and (project / f"{config['project_name']}.rep/project.prp").is_file()
    )
    checks.append((f"{config['project_dir']}/{config['project_name']}", project_ready))
    try:
        import pyghidra  # noqa: F401

        pyghidra_ready = True
    except ImportError:
        pyghidra_ready = False
    checks.append(("pyghidra", pyghidra_ready))
    return checks


def worktree_verify(_args) -> int:
    failed = False
    for name, ready in _readiness():
        print(f"{'READY' if ready else 'MISSING':7} {name}")
        failed |= not ready
    if failed:
        print("Run `tony worktree prepare` from this worktree; do not download or rebuild prerequisites.")
        return 1
    print("worktree: READY")
    return 0


def worktree_prepare(args) -> int:
    source_root = Path(args.source).resolve() if args.source else _primary_worktree()
    if source_root == ROOT.resolve():
        print("Already in the primary worktree")
        return worktree_verify(args)
    for relative in _shared_paths():
        _link_shared(source_root, relative)
    _seed_ghidra_project(source_root)
    install = resolve(load_yaml("re/config/ghidra.yml")["ghidra"]["install_dir"])
    if not any(name == "pyghidra" and ready for name, ready in _readiness()):
        _install_pyghidra(install)
    return worktree_verify(args)
