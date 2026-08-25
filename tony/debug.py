from __future__ import annotations

import os
import socket
import subprocess
import time

from .common import ROOT, load_yaml, wine_env
from .identity import recorded_executable


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
    exe = recorded_executable()
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
