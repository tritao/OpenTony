"""Format-independent recording model and host-side recording controls."""

from __future__ import annotations

import json
import os
import tempfile
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable

from . import sessions
from .common import resolve

RECORDING_FORMAT = "opentony-retail-recording-v1"


@dataclass(frozen=True)
class RecordingEvent:
    """One semantic event attached to a recording frame.

    ``payload`` deliberately remains a plain mapping.  The capture decoder
    can therefore preserve newly discovered event fields without having to
    rev this host-side model, while callers still get a small typed envelope
    for the fields used by replay and comparison.
    """

    type: str
    frame: int | None = None
    phase: str | None = None
    payload: dict[str, Any] = field(default_factory=dict)
    raw: bytes = b""

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "RecordingEvent":
        payload = dict(value)
        event_type = str(payload.pop("type", "unknown"))
        frame = payload.pop("frame", None)
        phase = payload.pop("phase", None)
        if phase is None:
            phase = payload.get("timer_boundary_phase")
        raw_value = payload.pop("_raw_bytes", b"")
        return cls(
            type=event_type,
            frame=int(frame) if isinstance(frame, int) else None,
            phase=str(phase) if isinstance(phase, str) else None,
            payload=payload,
            raw=bytes(raw_value) if isinstance(raw_value, (bytes, bytearray)) else b"",
        )

    def to_dict(self, *, include_raw: bool = False) -> dict[str, Any]:
        value = dict(self.payload)
        value["type"] = self.type
        if self.frame is not None:
            value["frame"] = self.frame
        if self.phase is not None and "timer_boundary_phase" not in value:
            value["phase"] = self.phase
        if include_raw and self.raw:
            value["_raw_bytes"] = self.raw
        return value


@dataclass(frozen=True)
class RecordingFrame:
    """Normalized frame plus optional raw observations from in-process capture."""

    frame: int
    input: dict[str, Any]
    before: dict[str, Any]
    after: dict[str, Any]
    events: tuple[RecordingEvent, ...] = ()
    raw_player_before: bytes | None = None
    raw_player_after: bytes | None = None
    timing_before: tuple[int, ...] = ()
    timing_after: tuple[int, ...] = ()
    timer_samples: tuple[tuple[int, ...], ...] = ()

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "RecordingFrame":
        raw = value.get("raw")
        if not isinstance(raw, dict):
            raw = {}
        before_blob = raw.get("player_before")
        after_blob = raw.get("player_after")
        timer_samples = raw.get("timer_samples", ())
        input_value = value.get("input")
        before_value = value.get("before")
        after_value = value.get("after")
        return cls(
            frame=int(value.get("frame", 0)),
            input=dict(input_value) if isinstance(input_value, dict) else {},
            before=dict(before_value) if isinstance(before_value, dict) else {},
            after=dict(after_value) if isinstance(after_value, dict) else {},
            events=tuple(
                RecordingEvent.from_dict(event)
                for event in value.get("events", ())
                if isinstance(event, dict)
            ),
            raw_player_before=bytes(before_blob) if isinstance(before_blob, (bytes, bytearray)) else None,
            raw_player_after=bytes(after_blob) if isinstance(after_blob, (bytes, bytearray)) else None,
            timing_before=tuple(int(item) for item in raw.get("timing_before", ()) if isinstance(item, int)),
            timing_after=tuple(int(item) for item in raw.get("timing_after", ()) if isinstance(item, int)),
            timer_samples=tuple(
                tuple(int(item) for item in sample if isinstance(item, int))
                for sample in timer_samples
                if isinstance(sample, (list, tuple))
            ),
        )

    def to_dict(self, *, include_raw: bool = False) -> dict[str, Any]:
        value = {
            "frame": self.frame,
            "input": dict(self.input),
            "before": self.before,
            "after": self.after,
            "events": [event.to_dict(include_raw=include_raw) for event in self.events],
        }
        if include_raw and (
            self.raw_player_before is not None
            or self.raw_player_after is not None
            or self.timing_before
            or self.timing_after
            or self.timer_samples
        ):
            value["raw"] = {
                "player_before": self.raw_player_before,
                "player_after": self.raw_player_after,
                "timing_before": list(self.timing_before),
                "timing_after": list(self.timing_after),
                "timer_samples": [list(sample) for sample in self.timer_samples],
            }
        return value


@dataclass(frozen=True)
class Recording:
    """Format-independent recording consumed by replay and comparison."""

    metadata: dict[str, Any]
    frames: tuple[RecordingFrame, ...]
    initial_state: dict[str, Any] = field(default_factory=dict)
    actions: tuple[tuple[int, ...], ...] = ()
    source_format: str = "legacy-jsonl"

    @property
    def header(self) -> dict[str, Any]:
        return self.metadata

    def frame_dicts(self, *, include_raw: bool = False) -> list[dict[str, Any]]:
        return [frame.to_dict(include_raw=include_raw) for frame in self.frames]

    def legacy_records(self) -> list[dict[str, Any]]:
        """Return the established JSONL record view for compatibility tools."""

        header = dict(self.metadata)
        # The view is consumed by the unchanged GDB adapter, whose wire
        # contract is deliberately frozen at V1 even when the source artifact
        # is OTREC2.
        header["format"] = RECORDING_FORMAT
        header["format_version"] = 1
        header.setdefault("capture_schema_version", 2)
        records = [{"type": "header", **header}]
        records.append({"type": "initial_state", "frame": 0, "state": self.initial_state})
        records.extend({"type": "frame", **frame.to_dict()} for frame in self.frames)
        records.append({
            "type": "end",
            "format": header.get("format", RECORDING_FORMAT),
            "recording_id": header.get("recording_id"),
            "frames": len(self.frames),
            "complete": True,
        })
        return records


def recording_from_legacy_records(records: Iterable[dict[str, Any]], *, source_format: str = "legacy-jsonl") -> Recording:
    """Build a :class:`Recording` from V1 JSONL records.

    This intentionally accepts sparse frame-only traces used by comparison
    diagnostics.  Strict format validation remains the job of
    :func:`validate_recording` below.
    """

    values = list(records)
    header = next((dict(value) for value in values if value.get("type") == "header"), {})
    header.pop("type", None)
    initial = next((value for value in values if value.get("type") == "initial_state"), {})
    state = initial.get("state") if isinstance(initial, dict) else {}
    if not isinstance(state, dict):
        state = {}
    frames = tuple(
        RecordingFrame.from_dict(value)
        for value in values
        if isinstance(value, dict) and value.get("type") == "frame"
    )
    return Recording(
        metadata=header,
        frames=frames,
        initial_state=state,
        source_format=source_format,
    )


def _read_jsonl_recording(path: Path) -> Recording:
    try:
        records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"could not read recording {path}: {exc}") from exc
    if not all(isinstance(record, dict) for record in records):
        raise ValueError(f"recording {path} contains a non-object record")
    return recording_from_legacy_records(records)


def load_recording(path: str | Path) -> Recording:
    """Load JSONL, ``.otcap`` or future binary ``.otrec`` recordings.

    Dispatching lives here so replay code never needs to know which recorder
    produced an artifact.  Binary V2 support is intentionally delegated to
    ``tony.binary_recording`` when that format is encountered; keeping that
    dependency lazy avoids a cycle with the existing capture decoder.
    """

    source = Path(path).expanduser()
    try:
        prefix = source.read_bytes()[:8]
    except OSError as exc:
        raise ValueError(f"could not read recording {source}: {exc}") from exc
    if prefix in {b"OTCAP\0\0\1", b"OTCAP\0\0\2", b"OTCAP\0\0\3"}:
        from .capture import decode_capture  # local import avoids capture → recording cycle

        capture = decode_capture(source, include_raw=True)
        return recording_from_capture(capture)
    if prefix.startswith(b"OTREC"):
        from .binary_recording import read_binary_recording

        return read_binary_recording(source)
    return _read_jsonl_recording(source)


def recording_from_capture(capture: dict[str, Any]) -> Recording:
    """Convert decoded in-process capture data into the common model."""

    level_index = int(capture.get("level_index", 0))
    level_name = {0: "hangar", 12: "warehouse"}.get(level_index)
    metadata = {
        "format": RECORDING_FORMAT,
        "format_version": 1,
        "capture_schema_version": 2,
        "recording_id": f"inproc-capture-{capture.get('build_sha256', '')[:8]}",
        "binary_sha256": capture.get("build_sha256"),
        "retail_executable_sha256": capture.get("build_sha256"),
        "instrumentation_version": "inproc-capture-v1",
        "capture_backend": "inproc",
        "capture_layout_version": int(capture.get("capture_layout_version", 1)),
        "image_base": capture.get("image_base", 0),
        "frame_boundary": "Skater_PhysicsFrame",
        "input_boundary": "Game_GameplayUpdate",
        "level": {"index": level_index, "name": level_name},
        "player_identity": {"slot": 0},
        "initial_timer_state": capture.get("initial_timer_state", {}),
    }
    frames = tuple(
        RecordingFrame.from_dict(frame)
        for frame in capture.get("frames", ())
        if isinstance(frame, dict)
    )
    actions = tuple(tuple(int(item) for item in action) for action in capture.get("actions", ()))
    initial_state = frames[0].before if frames else {}
    return Recording(
        metadata=metadata,
        frames=frames,
        initial_state=initial_state,
        actions=actions,
        source_format="otcap",
    )


def write_recording(recording: Recording, path: str | Path, *, force: bool = False) -> dict[str, Any]:
    """Write the canonical binary OTREC2 representation."""

    from .binary_recording import write_binary_recording

    return write_binary_recording(recording, path, force=force)


def export_json(path: str | Path, output: str | Path, *, force: bool = False) -> dict[str, Any]:
    """Export any recording as the legacy human-readable JSONL view."""

    target = Path(output).expanduser()
    if target.exists() and not force:
        raise ValueError(f"refusing to overwrite {target}; use --force if intended")
    recording = load_recording(path)
    target.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(prefix=f".{target.name}.", dir=target.parent)
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            for record in recording.legacy_records():
                stream.write(json.dumps(record, sort_keys=True, allow_nan=False) + "\n")
        os.replace(temporary, target)
    finally:
        temporary.unlink(missing_ok=True)
    return {"path": str(target), "frames": len(recording.frames), "format": RECORDING_FORMAT}


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
    frames = getattr(args, "frames", None)
    if frames is not None:
        if frames <= 0:
            raise SystemExit("--frames must be positive")
        command["frames"] = frames
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
    """Return ``(summary, errors)`` for a legacy or binary recording."""

    source = Path(path)
    try:
        prefix = source.read_bytes()[:8]
    except OSError as exc:
        return {"path": str(source), "frames": 0}, [{"line": 0, "error": str(exc)}]
    if prefix.startswith(b"OTREC") or prefix in {b"OTCAP\0\0\1", b"OTCAP\0\0\2"}:
        try:
            recording = load_recording(source)
        except (OSError, ValueError) as exc:
            return {"path": str(source), "frames": 0, "format": "binary"}, [{"line": 0, "error": str(exc)}]
        errors: list[dict] = []
        for expected, frame in enumerate(recording.frames):
            if frame.frame != expected:
                errors.append({"frame": frame.frame, "error": f"frame numbering is not contiguous; expected {expected}"})
        summary = {
            "path": str(source),
            "format": recording.metadata.get("format", "opentony-retail-recording-v2"),
            "recording_id": recording.metadata.get("recording_id"),
            "frames": len(recording.frames),
            "complete": True,
            "valid": not errors,
        }
        return summary, errors

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
    if not isinstance(header.get("initial_timer_state"), dict):
        errors.append({
            "line": 1,
            "error": "missing or invalid header field: initial_timer_state",
        })

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
        # Diagnostic probe families are deliberately optional.  Canonical
        # recordings carry no correction/RNG/collision events; forensic
        # recaptures may add whichever family is relevant to a divergence.

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
