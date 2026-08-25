from __future__ import annotations

import subprocess
from pathlib import Path

from .common import ROOT, wine_env
from .identity import recorded_executable


def wine_init(_args) -> int:
    env = wine_env()
    prefix = Path(env["WINEPREFIX"])
    prefix.mkdir(parents=True, exist_ok=True)
    print(f"Initializing Wine prefix: {prefix}")
    return subprocess.run(["wineboot", "-u"], cwd=ROOT, env=env, check=False).returncode


def _recorded_exe():
    return recorded_executable()


def run_game(args) -> int:
    exe = recorded_executable()
    env = wine_env()
    command = ["wine", str(exe), *args.game_args]
    print(" ".join(command))
    return subprocess.run(command, cwd=exe.parent, env=env, check=False).returncode
