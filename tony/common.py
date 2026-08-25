from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
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


def wine_env() -> dict[str, str]:
    config = load_yaml("re/config/wine.yml")["wine"]
    env = os.environ.copy()
    env["WINEPREFIX"] = str(resolve(config["prefix"]))
    if config.get("debug_quiet", True):
        env.setdefault("WINEDEBUG", "-all")
    return env
