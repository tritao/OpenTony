from __future__ import annotations

import json
import os
import re
import subprocess
import time
from pathlib import Path

from .audio import start_muted_audio
from .common import ROOT, headless_wine_command, headless_wine_env, load_yaml, wine_env
from .display import HeadlessDisplay, configure_visual_capture, terminate_process, xvfb_command
from .gdb_knowledge import generate as generate_gdb_knowledge
from .nocd import nocd_executable
from .sessions import _timestamp, cleanup_session_audio, create_session

_WINE_PROC_LINE = re.compile(r"^\s*=?([0-9a-fA-F]+)\s+\d+\s+(?:\\_\s+)?'([^']+)'$")


def _recover_incomplete_trace(session, reason: str) -> bool:
    """Append an incomplete footer after GDB/WineDbg dies with a trace open."""

    marker = session.path / "trace.active"
    try:
        metadata = json.loads(marker.read_text(encoding="utf-8"))
        trace_path = Path(metadata["path"])
    except (OSError, KeyError, TypeError, json.JSONDecodeError):
        return False

    if not trace_path.is_file():
        marker.unlink(missing_ok=True)
        return False

    try:
        lines = trace_path.read_text(encoding="utf-8").splitlines()
        last = json.loads(lines[-1]) if lines else {}
        if last.get("type") == "end":
            marker.unlink(missing_ok=True)
            return False
        frame = int(last.get("frame", 0))
        with trace_path.open("a", encoding="utf-8") as stream:
            stream.write(
                json.dumps(
                    {
                        "type": "end",
                        "frames": frame,
                        "complete": False,
                        "reason": reason,
                    },
                    sort_keys=True,
                )
                + "\n"
            )
    except (OSError, TypeError, ValueError, json.JSONDecodeError):
        return False
    marker.unlink(missing_ok=True)
    return True


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


def _wait_port(port: int, timeout: float = 45.0) -> None:
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
    # GDB imports this generated, dependency-free module from build/gdb.
    generate_gdb_knowledge()
    cfg = load_yaml("re/config/wine.yml")["wine"]
    pid_arg = getattr(args, "pid", None)
    headless_launch = pid_arg is None and cfg.get("debug_xvfb", True)
    session = create_session(getattr(args, "session", None), getattr(args, "port", None), isolated=headless_launch)
    port = session.port
    display = None
    proxy = None
    gdb_process = None
    exit_code = None
    env = None
    try:
        env = headless_wine_env(session.prefix) if headless_launch else wine_env()
        env["TONY_SESSION_ID"] = session.session_id
        env["TONY_SESSION_DIR"] = str(session.path)
        if pid_arg is None and not getattr(args, "unmute", False) and cfg.get("debug_mute_audio", True):
            audio_start = start_muted_audio(env, session.session_id)
            if audio_start.route is not None:
                session.update(
                    audio_muted=True,
                    audio_backend="pulse-null-sink",
                    audio_pactl=audio_start.route.pactl,
                    audio_module_id=audio_start.route.module_id,
                    audio_sink=audio_start.route.sink_name,
                    audio_pulse_server=audio_start.route.pulse_server,
                    audio_error=None,
                )
            else:
                session.update(audio_muted=False, audio_error=audio_start.error)
        if pid_arg is not None:
            target = [str(_find_game_pid(env) if pid_arg == "auto" else pid_arg)]
            cwd = ROOT
        else:
            exe = nocd_executable()
            target = [str(exe), *args.game_args]
            cwd = exe.parent
        proxy_command = ["winedbg", "--gdb", "--no-start", "--port", str(port), *target]
        if headless_launch:
            proxy_command = headless_wine_command(proxy_command)
            display = HeadlessDisplay(cfg, env)
            proxy = display.popen(proxy_command, cwd=cwd, env=env)
        else:
            if getattr(args, "screenshot", None) or getattr(args, "record", None):
                raise SystemExit("visual capture requires an isolated debug launch; omit --pid")
            proxy = subprocess.Popen(proxy_command, cwd=cwd, env=env, start_new_session=True)
        session.update(status="proxy-started", proxy_pid=proxy.pid)
        _wait_port(port)
        if display is not None:
            configure_visual_capture(display, args)
            session.update(
                display=display.info.display if display.info else None,
                xauthority=str(display.info.xauthority) if display.info else None,
            )
        gdb_cmd = [
            "gdb", "-q", "-nx",
            "-ex", f"target remote localhost:{port}",
            "-x", str(ROOT / "re/gdb/bootstrap.gdb"),
        ]
        gdb_env = os.environ.copy()
        gdb_env["TONY_SESSION_ID"] = session.session_id
        gdb_env["TONY_SESSION_DIR"] = str(session.path)
        if display is not None:
            gdb_env.update(display.environment)
        gdb_process = subprocess.Popen(gdb_cmd, cwd=ROOT, env=gdb_env, start_new_session=True)
        session.update(status="running", gdb_pid=gdb_process.pid)
        exit_code = gdb_process.wait()
        return exit_code
    except BaseException as exc:
        session.update(status="failed", error=f"{type(exc).__name__}: {exc}")
        raise
    finally:
        if display is not None:
            display.stop_recording()
        if gdb_process is not None and gdb_process.poll() is None:
            terminate_process(gdb_process)
        if gdb_process is not None or proxy is not None:
            gdb_returncode = gdb_process.returncode if gdb_process is not None else None
            proxy_returncode = proxy.poll() if proxy is not None else None
            if proxy_returncode not in (None, 0):
                reason = f"gdb-proxy-disconnected:{proxy_returncode}"
            elif gdb_returncode not in (None, 0):
                reason = f"gdb-exited:{gdb_returncode}"
            else:
                reason = "debugger-exited-with-trace-open"
            _recover_incomplete_trace(session, reason)
        if display is not None and proxy is not None:
            terminate_process(proxy)
            if env is not None:
                subprocess.run(
                    ["wineserver", "-k"],
                    cwd=ROOT,
                    env=env,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=False,
                )
        elif proxy is not None:
            proxy.terminate()
            try:
                proxy.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proxy.kill()
        cleanup_session_audio(session)
        if display is not None:
            display.close()
        session.update(status="stopped", exit_code=exit_code, stopped_at=_timestamp())
