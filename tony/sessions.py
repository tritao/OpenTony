from __future__ import annotations

import json
import os
import re
import shutil
import signal
import socket
import tempfile
import time
import uuid
from collections.abc import Iterator
from contextlib import contextmanager
from datetime import UTC, datetime
from fcntl import LOCK_EX, flock
from pathlib import Path

from .common import load_yaml, resolve

_SESSION_ID_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
_ACTIVE_STATUSES = {"starting", "proxy-started", "running"}


def _timestamp() -> str:
    return datetime.now(UTC).isoformat(timespec="seconds")


def _config() -> dict:
    return load_yaml("re/config/wine.yml")["wine"]


def _sessions_root() -> Path:
    return resolve(_config().get("sessions_dir", "build/debug/sessions"))


def _prefix_root() -> Path:
    return resolve(_config().get("session_prefix_dir", ".tools/debug-sessions"))


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
        if self.data.get("status") in _ACTIVE_STATUSES:
            return True
        return any(_pid_alive(int(self.data[key])) for key in ("proxy_pid", "gdb_pid") if self.data.get(key))

    def update(self, **values) -> None:
        self.data.update(values)
        _write_metadata(self.path / "session.json", self.data)


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
        "proxy_pid": None,
        "gdb_pid": None,
        "display": None,
        "xauthority": None,
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
    try:
        os.kill(pid, 0)
    except OSError:
        return False
    return True


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
        if value:
            _terminate_pid(int(value))
    session.update(status="stopped", stopped_at=_timestamp())
    return session


def clean_session(session_id: str) -> None:
    session = load_session(session_id)
    if session.active:
        raise SystemExit(f"debug session is still active: {session_id}; run: tony sessions stop {session_id}")
    shutil.rmtree(session.path)


def sessions_list(_args) -> int:
    sessions = list_sessions()
    if not sessions:
        print("No debug sessions.")
        return 0
    print("ID                                           STATUS       PORT  PREFIX")
    for session in sessions:
        status = "running" if session.active else session.data.get("status", "unknown")
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
