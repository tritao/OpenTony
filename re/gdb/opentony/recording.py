"""Retail gameplay recording lifecycle and JSONL evidence writer.

The state machine in this module deliberately does not know about GDB.  The
GDB adapter supplies input/frame snapshots and forwards probe events through
``event``.  Keeping the lifecycle independent makes the start/stop semantics
testable without a running Wine process and gives the native replay reader a
stable file contract later.
"""

from __future__ import annotations

import json
import math
import os
import tempfile
import uuid
from collections.abc import Callable
from datetime import UTC, datetime
from enum import Enum
from pathlib import Path


class RecordingState(str, Enum):
    IDLE = "Idle"
    START_PENDING = "StartPending"
    RECORDING = "Recording"
    STOP_PENDING = "StopPending"


class RecordingError(RuntimeError):
    """A recording request cannot be applied in the current state."""


def _timestamp() -> str:
    return datetime.now(UTC).isoformat(timespec="milliseconds")


def _json_safe(value):
    """Return a JSON-compatible copy while retaining non-finite evidence.

    Probe records intentionally include heuristic float interpretations beside
    their raw words.  A raw word can decode as an IEEE NaN or infinity even
    when the game's actual value is integer/fixed-point data.  Strict JSON
    cannot represent those values, so keep the distinction explicit instead
    of emitting non-standard JSON or dropping the field.
    """

    if isinstance(value, float):
        if math.isfinite(value):
            return value
        return {"non_finite_float": value.hex()}
    if isinstance(value, dict):
        return {key: _json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(item) for item in value]
    return value


class RecordingWriter:
    """Write one immutable retail recording as flushed JSONL records."""

    FORMAT = "opentony-retail-recording-v1"
    FORMAT_VERSION = 1
    SCHEMA_VERSION = 2

    def __init__(
        self,
        path: str | Path,
        metadata: dict,
        *,
        overwrite: bool = False,
        event_handler: Callable[[dict], None] | None = None,
    ):
        self.path = Path(path).expanduser()
        self.metadata = dict(metadata)
        self.overwrite = overwrite
        self.event_handler = event_handler
        self._stream = None
        self._closed = False

    def _marker_path(self) -> Path | None:
        session_dir = os.environ.get("TONY_SESSION_DIR")
        return Path(session_dir) / "recording.active" if session_dir else None

    def _mark_active(self) -> None:
        marker = self._marker_path()
        if marker is None:
            return
        marker.parent.mkdir(parents=True, exist_ok=True)
        marker.write_text(
            json.dumps(
                {
                    "path": str(self.path.resolve()),
                    "recording_id": self.metadata.get("recording_id"),
                    "format": self.FORMAT,
                },
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )

    def _clear_marker(self) -> None:
        marker = self._marker_path()
        if marker is not None:
            marker.unlink(missing_ok=True)

    def open(self) -> None:
        if self._stream is not None:
            return
        if self.path.exists() and not self.overwrite:
            raise OSError(
                f"refusing to overwrite {self.path}; use --force if intended"
            )
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._stream = self.path.open("w", encoding="utf-8")
        self._mark_active()
        self.write(
            {
                "type": "header",
                "format": self.FORMAT,
                "format_version": self.FORMAT_VERSION,
                "capture_schema_version": self.SCHEMA_VERSION,
                **self.metadata,
            }
        )

    def write(self, record: dict) -> None:
        if self._closed:
            raise OSError(f"recording writer is closed: {self.path}")
        if self._stream is None:
            self.open()
        self._stream.write(
            json.dumps(_json_safe(record), sort_keys=True, allow_nan=False) + "\n"
        )
        self._stream.flush()

    def event(self, record: dict) -> None:
        """Forward a probe event to the active frame accumulator."""

        if self.event_handler is None:
            return
        self.event_handler(dict(record))

    def close(
        self,
        *,
        frames: int,
        complete: bool = True,
        reason: str | None = None,
    ) -> None:
        if self._closed:
            return
        if self._stream is None:
            self.open()
        footer = {
            "type": "end",
            "format": self.FORMAT,
            "recording_id": self.metadata.get("recording_id"),
            "frames": frames,
            "complete": complete,
        }
        if reason is not None:
            footer["reason"] = reason
        self.write(footer)
        self._stream.close()
        self._closed = True
        self._clear_marker()


class RecordingController:
    """Own recording lifecycle, frame numbering, and active-frame events."""

    DEFAULT_HOTKEY_SCAN_CODE = 0x58  # DIK_F12; not used by TH2_OPT.CFG.
    DEFAULT_DIRECTORY = Path("build/recordings/retail")
    PENDING_ASYNC_EVENT_TYPES = frozenset({"simulation_time_accumulator_store"})

    def __init__(
        self,
        *,
        hotkey_scan_code: int = DEFAULT_HOTKEY_SCAN_CODE,
        session_dir: str | Path | None = None,
        clock: Callable[[], str] = _timestamp,
        writer_factory=RecordingWriter,
    ):
        if not 0 <= hotkey_scan_code < 0x100:
            raise ValueError("hotkey scan code must be between 0 and 255")
        self.hotkey_scan_code = hotkey_scan_code
        self.session_dir = Path(session_dir) if session_dir else None
        self.clock = clock
        self.writer_factory = writer_factory
        self.state = RecordingState.IDLE
        self.recording_id: str | None = None
        self.current_frame_index = 0
        self._writer: RecordingWriter | None = None
        self._path: Path | None = None
        self._overwrite = False
        self._active_frame: dict | None = None
        self._pending_external_events: list[dict] = []
        self._latest_input: dict | None = None
        self._hotkey_down = False
        self._stop_after_frame = False
        self._last_error: str | None = None
        self._pending_metadata: dict = {}
        self._write_status()

    @property
    def writer(self) -> RecordingWriter | None:
        return self._writer

    @property
    def path(self) -> Path | None:
        return self._path

    @property
    def latest_input(self) -> dict | None:
        return None if self._latest_input is None else dict(self._latest_input)

    @property
    def active_frame(self) -> int | None:
        if self._active_frame is None:
            return None
        return int(self._active_frame["frame"])

    def _default_path(self, recording_id: str) -> Path:
        return Path.cwd() / self.DEFAULT_DIRECTORY / f"{recording_id}.otrec"

    def _status(self) -> dict:
        return {
            "format": RecordingWriter.FORMAT,
            "state": self.state.value,
            "recording_id": self.recording_id,
            "path": str(self._path) if self._path is not None else None,
            "frames": self.current_frame_index,
            "active_frame": self.active_frame,
            "hotkey_scan_code": self.hotkey_scan_code,
            "last_error": self._last_error,
        }

    def _write_status(self) -> None:
        if self.session_dir is None:
            return
        target = self.session_dir / "recording.status.json"
        try:
            target.parent.mkdir(parents=True, exist_ok=True)
            fd, temporary_name = tempfile.mkstemp(
                prefix="recording-status-",
                suffix=".json",
                dir=target.parent,
            )
            temporary = Path(temporary_name)
            try:
                with os.fdopen(fd, "w", encoding="utf-8") as stream:
                    json.dump(self._status(), stream, indent=2, sort_keys=True)
                    stream.write("\n")
                os.replace(temporary, target)
            finally:
                temporary.unlink(missing_ok=True)
        except OSError as exc:
            self._last_error = f"could not write recording status: {exc}"

    def set_error(self, message: str) -> None:
        """Publish an instrumentation error without exposing status internals."""

        self._last_error = str(message)
        self._write_status()

    def _new_id(self) -> str:
        return f"recording-{datetime.now(UTC).strftime('%Y%m%d-%H%M%S')}-{uuid.uuid4().hex[:8]}"

    def request_start(
        self,
        path: str | Path | None = None,
        *,
        overwrite: bool = False,
        metadata: dict | None = None,
    ) -> str:
        if self.state is not RecordingState.IDLE:
            raise RecordingError(
                f"cannot start recording while state is {self.state.value}"
            )
        recording_id = self._new_id()
        target = (
            Path(path).expanduser()
            if path is not None
            else self._default_path(recording_id)
        )
        if target.exists() and not overwrite:
            raise RecordingError(
                f"refusing to overwrite {target}; use --force if intended"
            )
        self.recording_id = recording_id
        self._path = target
        self._overwrite = overwrite
        self._writer = None
        self._active_frame = None
        self._pending_external_events = []
        self._latest_input = None
        self.current_frame_index = 0
        self._stop_after_frame = False
        self._last_error = None
        self._pending_metadata = dict(metadata or {})
        self.state = RecordingState.START_PENDING
        self._write_status()
        return recording_id

    def request_stop(self) -> bool:
        if self.state is RecordingState.IDLE:
            return False
        self.state = RecordingState.STOP_PENDING
        self._stop_after_frame = True
        self._write_status()
        return True

    def request_toggle(
        self,
        path: str | Path | None = None,
        *,
        overwrite: bool = False,
        metadata: dict | None = None,
    ) -> str | None:
        if self.state is RecordingState.IDLE:
            return self.request_start(path, overwrite=overwrite, metadata=metadata)
        if self.state is RecordingState.START_PENDING:
            self.request_stop()
            return self.recording_id
        if self.state is RecordingState.STOP_PENDING:
            self.state = RecordingState.RECORDING
            self._stop_after_frame = False
            self._write_status()
            return self.recording_id
        self.request_stop()
        return self.recording_id

    def poll_control(self) -> None:
        """Apply queued host-side commands at the post-input boundary."""

        if self.session_dir is None:
            return
        directory = self.session_dir / "recording.commands"
        if not directory.is_dir():
            return
        for command_path in sorted(directory.glob("*.json")):
            try:
                command = json.loads(command_path.read_text(encoding="utf-8"))
                command_path.unlink()
                action = command.get("action")
                if action == "start":
                    self.request_start(
                        command.get("output"),
                        overwrite=bool(command.get("overwrite", False)),
                    )
                elif action == "stop":
                    if not self.request_stop():
                        raise RecordingError("no active recording")
                elif action == "toggle":
                    self.request_toggle(
                        command.get("output"),
                        overwrite=bool(command.get("overwrite", False)),
                    )
                else:
                    raise RecordingError(f"unknown recording command: {action!r}")
            except (OSError, TypeError, ValueError, RecordingError) as exc:
                command_path.unlink(missing_ok=True)
                self._last_error = str(exc)
                self._write_status()

    def on_input(self, input_record: dict, *, hotkey_down: bool = False) -> bool:
        """Latch post-poll input and apply a rising-edge toggle."""

        self._latest_input = dict(input_record)
        rising = hotkey_down and not self._hotkey_down
        self._hotkey_down = hotkey_down
        if rising:
            self.request_toggle()
        return rising

    def begin_frame(
        self,
        before: dict,
        *,
        input_record: dict | None = None,
        metadata: dict | None = None,
    ) -> int | None:
        if self.state not in (
            RecordingState.START_PENDING,
            RecordingState.RECORDING,
            RecordingState.STOP_PENDING,
        ):
            return None
        if self._active_frame is not None:
            raise RecordingError("a recording frame is already active")
        if self._writer is None:
            if self._path is None or self.recording_id is None:
                raise RecordingError("recording has no output path or ID")
            header = {
                "recording_id": self.recording_id,
                "recording_timestamp": self.clock(),
                "hotkey_scan_code": self.hotkey_scan_code,
                "frame_boundary": "Skater_PhysicsFrame",
                "input_boundary": "Game_GameplayUpdate",
                **self._pending_metadata,
                **(metadata or {}),
            }
            self._writer = self.writer_factory(
                self._path,
                header,
                overwrite=self._overwrite,
                event_handler=self.event,
            )
            try:
                self._writer.open()
                self._writer.write(
                    {
                        "type": "initial_state",
                        "frame": self.current_frame_index,
                        "state": before,
                    }
                )
            except (OSError, TypeError, ValueError) as exc:
                self._last_error = str(exc)
                self._writer = None
                self.state = RecordingState.IDLE
                self._write_status()
                raise RecordingError(str(exc)) from exc
        if self.state is RecordingState.START_PENDING:
            self.state = RecordingState.RECORDING
        pending_external_events = self._pending_external_events
        self._pending_external_events = []
        self._active_frame = {
            "type": "frame",
            "frame": self.current_frame_index,
            "input": dict(input_record or self._latest_input or {}),
            "before": before,
            "events": [dict(event) for event in pending_external_events],
        }
        self._write_status()
        return self.current_frame_index

    def event(self, record: dict) -> None:
        if self._active_frame is None:
            if (
                self.state is not RecordingState.IDLE
                and record.get("type") in self.PENDING_ASYNC_EVENT_TYPES
            ):
                self._pending_external_events.append(dict(record))
            return
        self._active_frame["events"].append(dict(record))

    def end_frame(self, after: dict) -> int | None:
        if self._active_frame is None:
            return None
        if self._writer is None:
            raise RecordingError("active frame has no writer")
        frame = dict(self._active_frame)
        frame["after"] = after
        try:
            self._writer.write(frame)
        except (OSError, TypeError, ValueError) as exc:
            frame_index = int(frame["frame"])
            self.close_incomplete(f"frame-{frame_index}-write-failed: {exc}")
            raise RecordingError(str(exc)) from exc
        frame_index = int(frame["frame"])
        self.current_frame_index += 1
        self._active_frame = None
        if self._stop_after_frame:
            self._writer.close(frames=self.current_frame_index)
            self._writer = None
            self.state = RecordingState.IDLE
            self._stop_after_frame = False
        self._write_status()
        return frame_index

    def close_incomplete(self, reason: str) -> None:
        if self._writer is None:
            return
        self._writer.close(
            frames=self.current_frame_index,
            complete=False,
            reason=reason,
        )
        self._writer = None
        self._active_frame = None
        self.state = RecordingState.IDLE
        self._last_error = reason
        self._write_status()

    def status(self) -> dict:
        return self._status()
