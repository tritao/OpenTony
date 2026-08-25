from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
from collections.abc import Sequence
from pathlib import Path
from typing import Any

import yaml

ROOT = Path(__file__).resolve().parents[1]


def resolve(path: str | Path) -> Path:
    path = Path(path)
    return path if path.is_absolute() else ROOT / path


def relative_to_root(path: Path) -> str:
    path = path.resolve()
    try:
        return str(path.relative_to(ROOT.resolve()))
    except ValueError as exc:
        raise ValueError(f"path must be inside the OpenTony repository: {path}") from exc


def load_yaml(path: str | Path) -> dict[str, Any]:
    with resolve(path).open("r", encoding="utf-8") as stream:
        return yaml.safe_load(stream) or {}


def save_yaml(path: str | Path, value: dict[str, Any]) -> None:
    target = resolve(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(yaml.safe_dump(value, sort_keys=False), encoding="utf-8")


def sha256(path: str | Path) -> str:
    digest = hashlib.sha256()
    with resolve(path).open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def which(name: str) -> str | None:
    return shutil.which(name)


def capture(command: list[str], *, env: dict[str, str] | None = None, cwd: Path | None = None) -> tuple[int, str]:
    try:
        result = subprocess.run(
            command,
            cwd=cwd or ROOT,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except FileNotFoundError:
        return 127, f"{command[0]}: not found"
    return result.returncode, result.stdout.strip()


def wine_env(prefix: str | Path | None = None) -> dict[str, str]:
    config = load_yaml("re/config/wine.yml")["wine"]
    env = os.environ.copy()
    env["WINEPREFIX"] = str(resolve(prefix if prefix is not None else config["prefix"]))
    if config.get("debug_quiet", True):
        env.setdefault("WINEDEBUG", "-all")
    return env


def headless_wine_env(prefix: str | Path | None = None) -> dict[str, str]:
    """Return an isolated Wine environment for Xvfb launches.

    Wine's server keeps display state per prefix. Reusing the visible prefix
    can route a supposedly headless process back to the host X display.
    """

    config = load_yaml("re/config/wine.yml")["wine"]
    prefix = resolve(prefix if prefix is not None else config.get("headless_prefix", ".tools/wineprefix-headless"))
    disc = resolve(config.get("headless_disc", "build/disc/files"))
    if not disc.is_dir():
        raise SystemExit(f"headless Wine disc tree not found: {disc}")

    drive = prefix / "dosdevices" / "d:"
    if drive.exists() and not drive.is_symlink():
        raise SystemExit(f"refusing to replace non-symlink headless Wine D: mapping: {drive}")
    env = wine_env(prefix)
    env["TONY_HEADLESS_DISC"] = str(disc)
    return env


def headless_wine_command(command: Sequence[str | Path]) -> list[str | Path]:
    """Initialize the isolated prefix and run a Wine command on one Xvfb display.

    New per-session prefixes are empty. Wine's ``-u`` update mode can wait
    indefinitely on this Wine build; ``-i`` is the one-shot initialization
    mode needed for a newly-created prefix.
    """

    return [
        "sh",
        "-c",
        (
            'wineserver -k 2>/dev/null || true; timeout 5s wineserver -w 2>/dev/null || true; '
            'timeout 30s env WINEDLLOVERRIDES=mscoree,mshtml= wineboot -i && '
            'wine reg add "HKCU\\Software\\Wine\\Direct3D" /v renderer /d gl /f >/dev/null 2>&1 && '
            'mkdir -p "$WINEPREFIX/dosdevices" && '
            'if [ -L "$WINEPREFIX/dosdevices/d:" ] && '
            '[ "$(readlink -f "$WINEPREFIX/dosdevices/d:")" != "$(readlink -f "$TONY_HEADLESS_DISC")" ]; then '
            'unlink "$WINEPREFIX/dosdevices/d:"; fi && '
            'if [ ! -e "$WINEPREFIX/dosdevices/d:" ] && [ ! -L "$WINEPREFIX/dosdevices/d:" ]; then '
            'ln -s "$TONY_HEADLESS_DISC" "$WINEPREFIX/dosdevices/d:"; fi && exec "$@"'
        ),
        "opentony-headless-wine",
        *command,
    ]
