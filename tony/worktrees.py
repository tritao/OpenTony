from __future__ import annotations

import importlib
import shutil
import sys
from pathlib import Path

from .common import ROOT, capture, load_yaml, resolve
from .ghidra_setup import _install_pyghidra


def _git_common_dir() -> Path:
    returncode, output = capture(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"])
    if returncode:
        raise SystemExit("could not locate the Git common directory")
    return Path(output).resolve()


def _shared_pyghidra_site() -> Path:
    version = load_yaml("re/config/ghidra.yml")["ghidra"]["version"]
    return _git_common_dir() / "opentony/tools" / f"pyghidra-{version}"


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
    if target.exists() and _project_ready(ROOT):
        print(f"READY {relative}")
        return
    if not (source / "recovered-types.json").is_file():
        print(f"DEFER {relative} (canonical project is not ready: {source})")
        return
    if target.exists() or target.is_symlink():
        backup = target.with_name(f"{target.name}.incomplete")
        if backup.exists() or backup.is_symlink():
            raise SystemExit(f"cannot preserve incomplete Ghidra project; backup already exists: {backup}")
        target.rename(backup)
        print(f"MOVE  {relative} -> {backup.relative_to(ROOT)}")
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


def _capabilities() -> list[tuple[str, str, str]]:
    ready = dict(_readiness())
    config = load_yaml("re/config/ghidra.yml")["ghidra"]
    project = f"{config['project_dir']}/{config['project_name']}"
    dynamic = ready["game/THPS2.img"] and ready["build/disc"]
    live = ready[str(Path(config["install_dir"]))] and ready[project] and ready["pyghidra"]
    matching = ready["match/original"]
    return [
        ("static-evidence", "READY", "checked-in symbols, evidence, and disassembly"),
        ("dynamic-tracing", "READY" if dynamic else "MISSING", "disc image and extracted executable"),
        ("live-pyghidra", "READY" if live else "MISSING", "PyGhidra and a private Ghidra project"),
        ("binary-matching", "READY" if matching else "DEFERRED", "original split modules"),
    ]


def worktree_verify(_args) -> int:
    capabilities = _capabilities()
    for name, status, detail in capabilities:
        print(f"{status:8} {name:17} {detail}")
    missing = [name for name, status, _detail in capabilities if status == "MISSING"]
    if missing:
        print("Run `tony worktree prepare`; bootstrap the primary worktree first if live-pyghidra remains missing.")
    print("worktree: READY" if not missing else f"worktree: PARTIAL ({', '.join(missing)} unavailable)")
    return 0


def worktree_prepare(args) -> int:
    source_root = Path(args.source).resolve() if args.source else _primary_worktree()
    if source_root == ROOT.resolve():
        print("Primary worktree selected; run `tony prerequisites bootstrap` to provision shared tools and its canonical project.")
        return worktree_verify(args)
    for relative in _shared_paths():
        if relative == Path("match/original") and not _path_ready(source_root, relative):
            print("DEFER match/original (binary matching unavailable)")
            continue
        _link_shared(source_root, relative)
    _seed_ghidra_project(source_root)
    return worktree_verify(args)


def _project_ready(root: Path) -> bool:
    config = load_yaml("re/config/ghidra.yml")["ghidra"]
    project = root / config["project_dir"]
    return (
        (project / "recovered-types.json").is_file()
        and (project / f"{config['project_name']}.gpr").is_file()
        and (project / f"{config['project_name']}.rep/project.prp").is_file()
    )


def prerequisites_bootstrap(args) -> int:
    primary = _primary_worktree()
    if ROOT.resolve() != primary:
        raise SystemExit(f"run prerequisite bootstrap from the primary worktree: {primary}")
    config = load_yaml("re/config/ghidra.yml")["ghidra"]
    install = resolve(config["install_dir"])
    if not _path_ready(ROOT, Path(config["install_dir"])):
        raise SystemExit("Ghidra is not installed; provision the pinned installation explicitly with `tony setup ghidra`")
    if not _path_ready(ROOT, Path("build/disc")):
        raise SystemExit("the extracted retail executable is missing; bootstrap will not download game inputs")
    site = _shared_pyghidra_site()
    if args.repair and site.exists():
        shutil.rmtree(site)
    if not (site / "pyghidra/__init__.py").is_file():
        _install_pyghidra(install, target=site, force=args.repair)
    else:
        print(f"Shared PyGhidra already installed: {site}")
    if str(site) not in sys.path:
        sys.path.insert(0, str(site))
    importlib.invalidate_caches()
    if args.repair or not _project_ready(ROOT):
        from .ghidra_ops import rebuild
        from .ghidra_lock import ghidra_project_lock

        with ghidra_project_lock():
            rebuild(args.profile)
    else:
        print(f"Canonical Ghidra project already ready: {resolve(config['project_dir'])}")
    print(f"Shared PyGhidra ready: {site}")
    return worktree_verify(args)
