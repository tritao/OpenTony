from __future__ import annotations

import os
import re
import subprocess
import time
from pathlib import Path

from .common import ROOT, headless_wine_command, headless_wine_env, load_yaml, wine_env
from .display import HeadlessDisplay, configure_visual_capture, xvfb_command
from .nocd import nocd_executable

_WINE_PROC_LINE = re.compile(r"^\s*=?([0-9a-fA-F]+)\s+\d+\s+(?:\\_\s+)?'([^']+)'$")


def _find_game_pid(env: dict[str, str]) -> int:
    result = subprocess.run(
        ["winedbg", "--command", "info proc"],
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    matches: list[int] = []
    for line in result.stdout.splitlines():
        match = _WINE_PROC_LINE.match(line)
        if not match:
            continue
        executable = match.group(2).replace("\\", "/").rsplit("/", 1)[-1]
        if executable.casefold() == "thawk2.nocd.exe":
            matches.append(int(match.group(1), 16))

    if result.returncode != 0:
        detail = result.stdout.strip() or "winedbg returned no details"
        raise SystemExit(f"could not list Wine processes:\n{detail}")

    if not matches:
        raise SystemExit("no running THawk2.nocd.exe Wine process found; run: tony run")
    if len(matches) > 1:
        joined = ", ".join(f"0x{pid:x}" for pid in sorted(matches))
        raise SystemExit(f"multiple THawk2.nocd.exe Wine processes found ({joined}); choose --pid <Wine PID>")
    return matches[0]


def _wait_port(port: int, timeout: float = 10.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        port_hex = f"{port:04X}"
        for proc_path in ("/proc/net/tcp", "/proc/net/tcp6"):
            try:
                lines = Path(proc_path).read_text(encoding="ascii").splitlines()[1:]
            except OSError:
                continue
            for line in lines:
                fields = line.split()
                if len(fields) >= 4 and fields[3] == "0A" and fields[1].rsplit(":", 1)[-1] == port_hex:
                    return
        time.sleep(0.1)
    raise RuntimeError(f"WineDbg GDB proxy did not open port {port}")


def _xvfb_command(cfg: dict, env: dict[str, str]) -> list[str]:
    """Compatibility wrapper for the shared headless display builder."""

    return xvfb_command(cfg, env)


def debug_game(args) -> int:
    cfg = load_yaml("re/config/wine.yml")["wine"]
    port = int(args.port or cfg["debug_port"])
    pid_arg = getattr(args, "pid", None)
    headless_launch = pid_arg is None and cfg.get("debug_xvfb", True)
    env = headless_wine_env() if headless_launch else wine_env()
    if pid_arg is not None:
        target = [str(_find_game_pid(env) if pid_arg == "auto" else pid_arg)]
        cwd = ROOT
    else:
        exe = nocd_executable()
        target = [str(exe), *args.game_args]
        cwd = exe.parent
    display = None
    proxy_command = ["winedbg", "--gdb", "--no-start", "--port", str(port), *target]
    if headless_launch:
        proxy_command = headless_wine_command(proxy_command)
        display = HeadlessDisplay(cfg, env)
        proxy = display.popen(proxy_command, cwd=cwd, env=env)
    else:
        if getattr(args, "screenshot", None) or getattr(args, "record", None):
            raise SystemExit("visual capture requires an isolated debug launch; omit --pid")
        proxy = subprocess.Popen(proxy_command, cwd=cwd, env=env)
    try:
        _wait_port(port)
        if display is not None:
            configure_visual_capture(display, args)
        gdb_cmd = [
            "gdb", "-q", "-nx",
            "-ex", f"target remote localhost:{port}",
            "-x", str(ROOT / "re/gdb/bootstrap.gdb"),
        ]
        gdb_env = os.environ.copy()
        if display is not None:
            gdb_env.update(display.environment)
        return subprocess.run(gdb_cmd, cwd=ROOT, env=gdb_env, check=False).returncode
    finally:
        if display is not None:
            display.stop_recording()
        proxy.terminate()
        try:
            proxy.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proxy.kill()
        if display is not None:
            display.close()
