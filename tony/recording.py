"""Host-side recording controls and V2 recording validation."""

from __future__ import annotations

import json
import os
import tempfile
import time
import uuid
from pathlib import Path

from . import sessions
from .common import resolve

RECORDING_FORMAT = "opentony-retail-recording-v1"


def _session_for(session_id: str | None, *, require_active: bool) -> sessions.DebugSession:
    if session_id:
        selected = sessions.load_session(session_id)
        if require_active and not selected.active:
            raise SystemExit(f"debug session is not active: {selected.session_id}")
        return selected
    active = [candidate for candidate in sessions.list_sessions() if candidate.active]
    if not active:
        raise SystemExit("no active debug session; run: tony debug")
    if len(active) > 1:
        names = ", ".join(candidate.session_id for candidate in active)
        raise SystemExit(f"multiple active debug sessions ({names}); use --session")
    return active[0]


def _queue_command(session: sessions.DebugSession, command: dict) -> Path:
    directory = session.path / "recording.commands"
    directory.mkdir(parents=True, exist_ok=True)
    target = directory / (
        f"{time.time_ns()}-{os.getpid()}-{uuid.uuid4().hex}.json"
    )
    fd, temporary_name = tempfile.mkstemp(
        prefix="recording-command-",
        suffix=".json",
        dir=directory,
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(command, stream, sort_keys=True)
            stream.write("\n")
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)
    return target


def _output(args) -> str | None:
    value = getattr(args, "output", None)
    return str(resolve(value)) if value else None


def record_start(args) -> int:
    session = _session_for(getattr(args, "session", None), require_active=True)
    command = {
        "action": "start",
        "output": _output(args),
        "overwrite": bool(getattr(args, "force", False)),
    }
    path = _queue_command(session, command)
    print(f"record start requested for {session.session_id}: {path}")
    return 0


def record_stop(args) -> int:
    session = _session_for(getattr(args, "session", None), require_active=True)
    path = _queue_command(session, {"action": "stop"})
    print(f"record stop requested for {session.session_id}: {path}")
    return 0


def record_toggle(args) -> int:
    session = _session_for(getattr(args, "session", None), require_active=True)
    command = {
        "action": "toggle",
        "output": _output(args),
        "overwrite": bool(getattr(args, "force", False)),
    }
    path = _queue_command(session, command)
    print(f"record toggle requested for {session.session_id}: {path}")
    return 0


def record_status(args) -> int:
    session = _session_for(getattr(args, "session", None), require_active=False)
    status_path = session.path / "recording.status.json"
    if not status_path.is_file():
        print(json.dumps({
            "session": session.session_id,
            "state": "Unavailable",
            "detail": "GDB recording controller has not published status",
        }, indent=2, sort_keys=True))
        return 0
    try:
        status = json.loads(status_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"could not read recording status: {status_path}: {exc}") from exc
    status["session"] = session.session_id
    print(json.dumps(status, indent=2, sort_keys=True))
    return 0


def validate_recording(path: str | Path) -> tuple[dict, list[dict]]:
    """Return ``(summary, errors)`` for one V1 JSONL recording."""

    source = Path(path)
    errors: list[dict] = []
    records: list[dict] = []
    try:
        lines = source.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        return {"path": str(source), "frames": 0}, [{"line": 0, "error": str(exc)}]

    for line_number, line in enumerate(lines, 1):
        try:
            value = json.loads(line)
        except json.JSONDecodeError as exc:
            errors.append({"line": line_number, "error": f"invalid JSON: {exc.msg}"})
            continue
        if not isinstance(value, dict):
            errors.append({"line": line_number, "error": "record is not an object"})
            continue
        records.append(value)

    if not records:
        errors.append({"line": 0, "error": "recording is empty"})
        return {"path": str(source), "frames": 0}, errors

    header = records[0]
    if header.get("type") != "header":
        errors.append({"line": 1, "error": "first record is not a header"})
    if header.get("format") != RECORDING_FORMAT:
        errors.append({"line": 1, "error": "unsupported recording format"})
    if header.get("capture_schema_version") != 2:
        errors.append({"line": 1, "error": "unsupported capture schema version"})
    for field in (
        "recording_id",
        "recording_timestamp",
        "retail_executable_sha256",
        "level",
        "player_identity",
        "instrumentation_version",
    ):
        if field not in header:
            errors.append({"line": 1, "error": f"missing header field: {field}"})

    initial = [record for record in records if record.get("type") == "initial_state"]
    frames = [record for record in records if record.get("type") == "frame"]
    end = records[-1] if records else {}
    if len(initial) != 1:
        errors.append({"line": 0, "error": f"expected one initial_state, found {len(initial)}"})
    if end.get("type") != "end":
        errors.append({"line": len(records), "error": "last record is not an end footer"})

    for expected, frame in enumerate(frames):
        if frame.get("frame") != expected:
            errors.append({
                "frame": frame.get("frame"),
                "error": f"frame numbering is not contiguous; expected {expected}",
            })
        for field in ("input", "before", "events", "after"):
            if field not in frame:
                errors.append({"frame": frame.get("frame"), "error": f"missing frame field: {field}"})
        if not isinstance(frame.get("input"), dict):
            errors.append({"frame": frame.get("frame"), "error": "input is not an object"})
        if not isinstance(frame.get("before"), dict):
            errors.append({"frame": frame.get("frame"), "error": "before is not an object"})
        if not isinstance(frame.get("after"), dict):
            errors.append({"frame": frame.get("frame"), "error": "after is not an object"})
        if not isinstance(frame.get("events", []), list):
            errors.append({"frame": frame.get("frame"), "error": "events is not an array"})
        else:
            for event_type in (
                "motion_correction_input",
                "response_correction_input",
            ):
                count = sum(
                    isinstance(event, dict) and event.get("type") == event_type
                    for event in frame["events"]
                )
                if count != 1:
                    errors.append({
                        "frame": frame.get("frame"),
                        "error": f"expected one {event_type} event, found {count}",
                    })

    if end.get("type") == "end" and end.get("frames") != len(frames):
        errors.append({
            "line": len(records),
            "error": f"footer frames={end.get('frames')} but found {len(frames)} frame records",
        })
    if end.get("type") == "end" and not end.get("complete", False):
        errors.append({"line": len(records), "error": "recording is incomplete"})

    summary = {
        "path": str(source),
        "format": header.get("format"),
        "recording_id": header.get("recording_id"),
        "frames": len(frames),
        "complete": bool(end.get("complete")) if end.get("type") == "end" else False,
        "valid": not errors,
    }
    return summary, errors


def record_validate(args) -> int:
    summary, errors = validate_recording(resolve(args.path))
    summary["valid"] = not errors
    print(json.dumps({"summary": summary, "errors": errors}, indent=2, sort_keys=True))
    return 0 if not errors else 1
