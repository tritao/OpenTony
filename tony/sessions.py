from __future__ import annotations

import json
import os
import re
import shutil
import signal
import socket
import subprocess
import tempfile
import time
import uuid
from collections.abc import Iterator
from contextlib import contextmanager
from datetime import UTC, datetime
from fcntl import LOCK_EX, flock
from pathlib import Path

from .audio import cleanup_muted_audio
from .common import ROOT, load_yaml, resolve

_SESSION_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
_ACTIVE_STATUSES = {"starting", "proxy-started", "running"}


def _timestamp() -> str:
    return datetime.now(UTC).isoformat(timespec="seconds")


def _config() -> dict:
    return load_yaml("re/config/wine.yml")["wine"]


def _shared_resolve(path: str | Path) -> Path:
    path = Path(path)
    if path.is_absolute():
        return path
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--path-format=absolute", "--git-common-dir"],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        common_dir = Path(result.stdout.strip())
        if result.returncode == 0 and common_dir.name == ".git":
            return common_dir.parent / path
    except OSError:
        pass
    return resolve(path)


def _sessions_root() -> Path:
    return _shared_resolve(_config().get("sessions_dir", "build/debug/sessions"))


def _prefix_root() -> Path:
    return _shared_resolve(_config().get("session_prefix_dir", ".tools/debug-sessions"))


def _validate_session_id(value: str) -> str:
    if not _SESSION_ID_RE.fullmatch(value):
        raise SystemExit(
            "invalid session id; use 1-64 letters, numbers, '.', '_' or '-' and start with a letter or number"
        )
    return value


def _new_session_id(root: Path) -> str:
    while True:
        value = f"debug-{datetime.now(UTC).strftime('%Y%m%d-%H%M%S')}-{uuid.uuid4().hex[:8]}"
        if not (root / value).exists():
            return value


def _port_range() -> range:
    config = _config()
    configured = config.get("debug_port_range")
    if configured is None:
        first = int(config.get("debug_port", 31337))
        return range(first, first + 64)
    if not isinstance(configured, list) or len(configured) != 2:
        raise SystemExit("wine.debug_port_range must contain [first, last]")
    first, last = (int(value) for value in configured)
    if not 1 <= first <= last <= 65535:
        raise SystemExit("wine.debug_port_range must be a valid inclusive TCP port range")
    return range(first, last + 1)


@contextmanager
def _port_lock(root: Path) -> Iterator[None]:
    root.mkdir(parents=True, exist_ok=True)
    lock_path = root / ".ports.lock"
    with lock_path.open("a+", encoding="utf-8") as stream:
        flock(stream.fileno(), LOCK_EX)
        yield


def _port_is_free(port: int) -> bool:
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 0)
            sock.bind(("127.0.0.1", port))
    except OSError:
        return False
    return True


def _active_ports(root: Path) -> set[int]:
    ports: set[int] = set()
    for metadata_path in root.glob("*/session.json"):
        try:
            data = json.loads(metadata_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue
        if data.get("status") in _ACTIVE_STATUSES and data.get("port") is not None:
            ports.add(int(data["port"]))
    return ports


def _choose_port(root: Path, requested: int | None) -> int:
    ports = _active_ports(root)
    if requested is not None:
        if not 1 <= requested <= 65535:
            raise SystemExit(f"invalid debug port: {requested}")
        if requested in ports or not _port_is_free(requested):
            raise SystemExit(f"debug port {requested} is already in use")
        return requested
    for port in _port_range():
        if port not in ports and _port_is_free(port):
            return port
    raise SystemExit("no free debug port in the configured wine.debug_port_range")


def _write_metadata(path: Path, data: dict) -> None:
    fd, temporary_name = tempfile.mkstemp(prefix="session-", suffix=".json", dir=path.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(data, stream, indent=2, sort_keys=True)
            stream.write("\n")
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


class DebugSession:
    def __init__(self, path: Path, data: dict):
        self.path = path
        self.data = data

    @property
    def session_id(self) -> str:
        return str(self.data["id"])

    @property
    def port(self) -> int:
        return int(self.data["port"])

    @property
    def prefix(self) -> Path | None:
        value = self.data.get("prefix")
        return Path(value) if value else None

    @property
    def active(self) -> bool:
        # A newly-created session is owned by the invoking ``tony debug``
        # process until it has published its child PIDs.  Once startup has
        # progressed, only the session's own processes keep it active.  This
        # makes interrupted/debugger-crashed sessions cleanable instead of
        # leaving a permanently "running" metadata record behind.
        if self.data.get("status") == "starting" and _pid_alive(int(self.data.get("owner_pid") or 0)):
            return True
        return any(
            _session_pid_alive(int(self.data[key]), self.session_id)
            for key in ("proxy_pid", "gdb_pid")
            if self.data.get(key)
        )

    def update(self, **values) -> None:
        self.data.update(values)
        _write_metadata(self.path / "session.json", self.data)


def cleanup_session_audio(session: DebugSession) -> bool:
    had_recorded_route = bool(session.data.get("audio_module_id"))
    result = cleanup_muted_audio(session.data)
    values = {"audio_cleanup": result.status}
    if result.ok:
        values.update(
            audio_muted=False,
            audio_backend=None,
            audio_pactl=None,
            audio_module_id=None,
            audio_sink=None,
            audio_pulse_server=None,
            audio_error=None if had_recorded_route else session.data.get("audio_error"),
            audio_cleanup_error=None,
        )
    else:
        values["audio_cleanup_error"] = result.status
    session.update(**values)
    return result.ok


def create_session(session_id: str | None, requested_port: int | None, *, isolated: bool) -> DebugSession:
    root = _sessions_root()
    root.mkdir(parents=True, exist_ok=True)
    session_id = _validate_session_id(session_id) if session_id else _new_session_id(root)
    session_path = root / session_id
    try:
        session_path.mkdir()
    except FileExistsError as exc:
        raise SystemExit(f"debug session already exists: {session_id}; stop and clean it first") from exc

    try:
        with _port_lock(root):
            port = _choose_port(root, requested_port)
    except BaseException:
        shutil.rmtree(session_path)
        raise

    prefix = _prefix_root() / session_id if isolated else None
    if prefix is not None:
        try:
            prefix.mkdir(parents=True)
        except FileExistsError as exc:
            shutil.rmtree(session_path)
            raise SystemExit(f"debug session prefix already exists: {prefix}; clean it first") from exc
    data = {
        "version": 1,
        "id": session_id,
        "status": "starting",
        "created_at": _timestamp(),
        "port": port,
        "prefix": str(prefix) if prefix else None,
        "owner_pid": os.getpid(),
        "proxy_pid": None,
        "gdb_pid": None,
        "display": None,
        "xauthority": None,
        "audio_muted": False,
        "audio_backend": None,
        "audio_pactl": None,
        "audio_module_id": None,
        "audio_sink": None,
        "audio_pulse_server": None,
        "audio_error": None,
        "audio_cleanup": None,
        "audio_cleanup_error": None,
    }
    _write_metadata(session_path / "session.json", data)
    return DebugSession(session_path, data)


def load_session(session_id: str) -> DebugSession:
    session_id = _validate_session_id(session_id)
    path = _sessions_root() / session_id
    metadata = path / "session.json"
    if not metadata.is_file():
        raise SystemExit(f"debug session not found: {session_id}")
    try:
        data = json.loads(metadata.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"invalid debug session metadata: {metadata}") from exc
    return DebugSession(path, data)


def list_sessions() -> list[DebugSession]:
    root = _sessions_root()
    if not root.is_dir():
        return []
    result = []
    for path in sorted(root.iterdir()):
        if path.is_dir() and (path / "session.json").is_file():
            try:
                result.append(load_session(path.name))
            except SystemExit:
                continue
    return result


def _pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    # A zombie still accepts signal 0, but it no longer owns a live session.
    try:
        stat = Path(f"/proc/{pid}/stat").read_text(encoding="ascii")
        state = stat.rsplit(") ", 1)[1][0]
    except (FileNotFoundError, OSError, IndexError):
        state = None
    return state != "Z"


def _session_pid_alive(pid: int, session_id: str) -> bool:
    """Return whether *pid* is alive and still belongs to this session.

    Linux exposes the inherited ``TONY_SESSION_ID`` in /proc.  Checking it
    prevents a stale PID that has since been reused by an unrelated process
    from making a session appear active or being terminated by ``sessions
    stop``.  If /proc is unavailable, the liveness check remains the safe
    portable fallback.
    """

    if not _pid_alive(pid):
        return False
    try:
        environment = Path(f"/proc/{pid}/environ").read_bytes().split(b"\0")
    except OSError:
        return True
    marker = f"TONY_SESSION_ID={session_id}".encode()
    return marker in environment


def _terminate_pid(pid: int, timeout: float = 2.0) -> None:
    if not _pid_alive(pid):
        return
    try:
        process_group = os.getpgid(pid)
    except OSError:
        return
    try:
        if process_group == pid:
            os.killpg(process_group, signal.SIGTERM)
        else:
            os.kill(pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline and _pid_alive(pid):
        time.sleep(0.05)
    if _pid_alive(pid):
        try:
            if process_group == pid:
                os.killpg(process_group, signal.SIGKILL)
            else:
                os.kill(pid, signal.SIGKILL)
        except ProcessLookupError:
            return


def stop_session(session_id: str) -> DebugSession:
    session = load_session(session_id)
    for key in ("gdb_pid", "proxy_pid"):
        value = session.data.get(key)
        if value and _session_pid_alive(int(value), session.session_id):
            _terminate_pid(int(value))
    cleanup_session_audio(session)
    session.update(status="stopped", stopped_at=_timestamp())
    return session


def clean_session(session_id: str) -> None:
    session = load_session(session_id)
    if session.active:
        raise SystemExit(f"debug session is still active: {session_id}; run: tony sessions stop {session_id}")
    if not cleanup_session_audio(session):
        detail = session.data.get("audio_cleanup_error", "unknown audio cleanup error")
        raise SystemExit(f"could not clean debug session audio: {detail}; retry sessions clean {session_id}")
    prefix = session.prefix
    if prefix is not None and prefix.is_dir():
        shutil.rmtree(prefix)
    shutil.rmtree(session.path)


def cleanup_session_prefix(session: DebugSession) -> bool:
    """Remove a stopped session's disposable Wine prefix, retaining its records."""

    prefix = session.prefix
    if prefix is None or not prefix.exists():
        return False
    if session.active or prefix.resolve() in _live_wine_prefixes():
        return False
    shutil.rmtree(prefix)
    session.update(prefix_cleaned_at=_timestamp())
    return True


def _live_wine_prefixes(proc_root: Path = Path("/proc"), *, attempts: int = 3) -> set[Path]:
    """Return a conservative union of live prefixes across repeated /proc scans."""

    prefixes: set[Path] = set()
    for attempt in range(attempts):
        try:
            processes = list(proc_root.iterdir())
        except OSError:
            return prefixes
        for process in processes:
            if not process.name.isdigit():
                continue
            try:
                environment = (process / "environ").read_bytes().split(b"\0")
            except OSError:
                continue
            for entry in environment:
                if entry.startswith(b"WINEPREFIX="):
                    value = os.fsdecode(entry.removeprefix(b"WINEPREFIX="))
                    if value:
                        prefixes.add(Path(value).resolve())
                    break
        if attempt + 1 < attempts:
            time.sleep(0.05)
    return prefixes


def _worktree_prefix_roots() -> set[Path]:
    roots = {_prefix_root().resolve()}
    try:
        result = subprocess.run(
            ["git", "worktree", "list", "--porcelain"],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except OSError:
        return roots
    if result.returncode == 0:
        for line in result.stdout.splitlines():
            if line.startswith("worktree "):
                roots.add((Path(line.removeprefix("worktree ")) / ".tools/debug-sessions").resolve())
    return roots


def prune_session_prefixes(*, dry_run: bool = False) -> tuple[list[Path], list[Path]]:
    """Remove orphaned Wine prefixes while protecting every live process prefix."""

    live = _live_wine_prefixes()
    removed: list[Path] = []
    protected: list[Path] = []
    for root in sorted(_worktree_prefix_roots()):
        if not root.is_dir():
            continue
        for prefix in sorted(root.iterdir()):
            if not prefix.is_dir():
                continue
            resolved = prefix.resolve()
            # Refresh immediately before each deletion. Wine may launch after
            # the initial inventory, and /proc entries can transiently vanish
            # while a process is execing.
            live.update(_live_wine_prefixes())
            if resolved in live:
                protected.append(prefix)
                continue
            if not dry_run:
                shutil.rmtree(prefix)
            removed.append(prefix)
    return removed, protected


def sessions_list(_args) -> int:
    sessions = list_sessions()
    if not sessions:
        print("No debug sessions.")
        return 0
    print("ID                                           STATUS       PORT  PREFIX")
    for session in sessions:
        recorded_status = session.data.get("status", "unknown")
        status = "running" if session.active else recorded_status
        if recorded_status in _ACTIVE_STATUSES and not session.active:
            status = "stale"
        prefix = session.prefix or Path("-")
        print(f"{session.session_id:<44} {status:<12} {session.port:<5} {prefix}")
    return 0


def sessions_stop(args) -> int:
    session = stop_session(args.session_id)
    print(f"Stopped debug session: {session.session_id}")
    return 0


def sessions_clean(args) -> int:
    clean_session(args.session_id)
    print(f"Cleaned debug session: {args.session_id}")
    return 0


def sessions_prune(args) -> int:
    removed, protected = prune_session_prefixes(dry_run=getattr(args, "dry_run", False))
    verb = "Would remove" if getattr(args, "dry_run", False) else "Removed"
    print(f"{verb} {len(removed)} stale Wine prefixes; protected {len(protected)} live prefixes.")
    return 0
