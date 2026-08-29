"""Versioned binary OpenTony recording reader and writer.

The injected recorder has a deliberately simple fixed transport (``.otcap``),
but replay should consume one archival recording format.  OTREC2 keeps the
normalized snapshots alongside the original player/timer observations and
uses a length-delimited event stream so adding a causal seam does not change
the frame layout.
"""

from __future__ import annotations

import base64
import json
import os
import struct
import tempfile
from pathlib import Path
from typing import Any

from .recording import Recording, RecordingEvent, RecordingFrame

MAGIC = b"OTREC2\0\0"
VERSION = 2
# Public descriptive aliases used by tooling/tests.
OTREC2_MAGIC = MAGIC
OTREC2_VERSION = VERSION
PLAYER_BLOB_SIZE = 0x3210
HEADER_STRUCT = struct.Struct("<8s17I32s")
FRAME_INDEX_STRUCT = struct.Struct("<16I")
SNAPSHOT_PREFIX_STRUCT = struct.Struct("<II")
EVENT_HEADER_STRUCT = struct.Struct("<HHIII")

# Stable IDs for events already emitted by the GDB/in-process decoders.  An
# unknown event remains round-trippable through the JSON payload with ID 0.
EVENT_TYPES = {
    "timer_callback_delivery": 1,
    "timer_boundary_sample": 2,
    "simulation_time_accumulator_store": 3,
    "timing_producer_sample": 4,
    "ollie_random_input": 5,
    "shared_random_call": 6,
    "deterministic_random_call": 7,
}
EVENT_NAMES = {value: key for key, value in EVENT_TYPES.items()}


class BinaryRecordingError(ValueError):
    """An OTREC2 file is malformed or cannot be represented by this reader."""


def _json_safe(value: Any) -> Any:
    if isinstance(value, bytes):
        return {"__bytes__": base64.b64encode(value).decode("ascii")}
    if isinstance(value, float):
        if value == value and abs(value) != float("inf"):
            return value
        return {"non_finite_float": value.hex()}
    if isinstance(value, dict):
        return {str(key): _json_safe(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_safe(item) for item in value]
    return value


def _json_bytes(value: Any) -> bytes:
    try:
        return json.dumps(_json_safe(value), sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")
    except (TypeError, ValueError) as exc:
        raise BinaryRecordingError(f"recording contains non-serializable metadata: {exc}") from exc


def _read_json(data: bytes, label: str) -> Any:
    try:
        return json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise BinaryRecordingError(f"invalid {label} JSON: {exc}") from exc


def _section(data: bytes, offset: int, size: int, label: str) -> bytes:
    if offset < 0 or size < 0 or offset > len(data) or size > len(data) - offset:
        raise BinaryRecordingError(f"{label} section is outside the file")
    return data[offset : offset + size]


def _timing_from_snapshot(snapshot: dict[str, Any]) -> tuple[int, ...]:
    timing = snapshot.get("timing")
    if not isinstance(timing, dict):
        return ()
    names = (
        "animation_clock",
        "animation_time_scale",
        "animation_time_scale_square",
        "animation_clock_accumulator",
        "simulation_time",
        "timing_delta_q11",
    )
    values: list[int] = []
    for name in names:
        value = timing.get(name)
        raw = value.get("raw") if isinstance(value, dict) else None
        if not isinstance(raw, int):
            return ()
        values.append(raw & 0xFFFFFFFF)
    return tuple(values)


def _snapshot_bytes(snapshot: dict[str, Any], raw_blob: bytes | None, timing: tuple[int, ...]) -> bytes:
    semantic = _json_bytes(snapshot)
    blob = bytes(raw_blob or b"")
    if blob and len(blob) != PLAYER_BLOB_SIZE:
        raise BinaryRecordingError(
            f"raw player snapshot has {len(blob)} bytes; expected 0x{PLAYER_BLOB_SIZE:x}"
        )
    values = tuple(int(item) & 0xFFFFFFFF for item in timing)
    if len(values) not in (0, 6):
        raise BinaryRecordingError("snapshot timing must contain six words")
    if not values:
        values = (0,) * 6
    return SNAPSHOT_PREFIX_STRUCT.pack(len(semantic), len(blob)) + semantic + blob + struct.pack("<6I", *values)


def _read_snapshot(data: bytes, offset: int, size: int, label: str) -> tuple[dict[str, Any], bytes | None, tuple[int, ...]]:
    payload = _section(data, offset, size, label)
    if len(payload) < SNAPSHOT_PREFIX_STRUCT.size + 24:
        raise BinaryRecordingError(f"{label} snapshot is truncated")
    semantic_size, blob_size = SNAPSHOT_PREFIX_STRUCT.unpack_from(payload)
    cursor = SNAPSHOT_PREFIX_STRUCT.size
    if semantic_size > len(payload) - cursor:
        raise BinaryRecordingError(f"{label} semantic snapshot is truncated")
    semantic = _read_json(payload[cursor : cursor + semantic_size], f"{label} snapshot")
    cursor += semantic_size
    if not isinstance(semantic, dict):
        raise BinaryRecordingError(f"{label} snapshot is not an object")
    if blob_size > len(payload) - cursor - 24:
        raise BinaryRecordingError(f"{label} raw snapshot is truncated")
    if blob_size not in (0, PLAYER_BLOB_SIZE):
        raise BinaryRecordingError(f"{label} raw snapshot has unsupported size {blob_size}")
    blob = payload[cursor : cursor + blob_size]
    cursor += blob_size
    if len(payload) - cursor != 24:
        raise BinaryRecordingError(f"{label} snapshot has trailing bytes")
    timing = struct.unpack_from("<6I", payload, cursor)
    return semantic, (bytes(blob) if blob else None), tuple(timing)


def _event_bytes(events: tuple[RecordingEvent, ...], frame: int) -> bytes:
    result = bytearray()
    for event in events:
        payload = _json_bytes(event.to_dict(include_raw=True))
        event_type = EVENT_TYPES.get(event.type, 0)
        phase = 0
        if event.phase:
            # Preserve phase text in the payload; this field is an index only
            # for cheap filtering by consumers that know the phase enum.
            phase = {
                "physics_entry": 1,
                "timer_update": 2,
                "simulation_clock_read": 3,
                "post_physics": 4,
            }.get(event.phase, 0)
        result.extend(EVENT_HEADER_STRUCT.pack(event_type, 1, len(payload), event.frame if event.frame is not None else frame, phase))
        result.extend(payload)
    return bytes(result)


def _read_events(data: bytes, offset: int, size: int, frame: int) -> tuple[RecordingEvent, ...]:
    payload = _section(data, offset, size, "event")
    cursor = 0
    events: list[RecordingEvent] = []
    while cursor < len(payload):
        if len(payload) - cursor < EVENT_HEADER_STRUCT.size:
            raise BinaryRecordingError(f"frame {frame} event header is truncated")
        event_type, event_version, event_size, event_frame, phase_id = EVENT_HEADER_STRUCT.unpack_from(payload, cursor)
        cursor += EVENT_HEADER_STRUCT.size
        if event_version != 1:
            raise BinaryRecordingError(f"frame {frame} has unsupported event version {event_version}")
        if event_size > len(payload) - cursor:
            raise BinaryRecordingError(f"frame {frame} event payload is truncated")
        value = _read_json(payload[cursor : cursor + event_size], f"frame {frame} event")
        cursor += event_size
        if not isinstance(value, dict):
            raise BinaryRecordingError(f"frame {frame} event payload is not an object")
        event_name = EVENT_NAMES.get(event_type)
        if event_name is not None and "type" not in value:
            value["type"] = event_name
        if "frame" not in value:
            value["frame"] = event_frame
        if phase_id and "phase" not in value and "timer_boundary_phase" not in value:
            value["phase"] = {
                1: "physics_entry",
                2: "timer_update",
                3: "simulation_clock_read",
                4: "post_physics",
            }.get(phase_id, f"phase_{phase_id}")
        raw_value = value.get("_raw_bytes")
        if isinstance(raw_value, dict) and "__bytes__" in raw_value:
            try:
                value["_raw_bytes"] = base64.b64decode(raw_value["__bytes__"], validate=True)
            except (ValueError, TypeError) as exc:
                raise BinaryRecordingError(f"frame {frame} event raw payload is invalid") from exc
        events.append(RecordingEvent.from_dict(value))
    return tuple(events)


def _timer_bytes(samples: tuple[tuple[int, ...], ...]) -> bytes:
    result = bytearray(struct.pack("<I", len(samples)))
    for sample in samples:
        if len(sample) != 10:
            raise BinaryRecordingError("raw timer sample must contain ten words")
        result.extend(struct.pack("<8I2Q", *(int(item) & 0xFFFFFFFFFFFFFFFF for item in sample)))
    return bytes(result)


def _read_timers(data: bytes, offset: int, size: int, frame: int) -> tuple[tuple[int, ...], ...]:
    payload = _section(data, offset, size, f"frame {frame} timer samples")
    if len(payload) < 4:
        raise BinaryRecordingError(f"frame {frame} timer samples are truncated")
    count = struct.unpack_from("<I", payload)[0]
    expected = 4 + count * 48
    if expected != len(payload):
        raise BinaryRecordingError(f"frame {frame} timer sample size is invalid")
    return tuple(
        tuple(struct.unpack_from("<8I2Q", payload, 4 + index * 48))
        for index in range(count)
    )


def write_binary_recording(recording: Recording, path: str | Path, *, force: bool = False) -> dict[str, Any]:
    """Write one format-independent recording as OTREC2."""

    target = Path(path).expanduser()
    if target.exists() and not force:
        raise BinaryRecordingError(f"refusing to overwrite {target}; use --force if intended")
    target.parent.mkdir(parents=True, exist_ok=True)
    metadata = dict(recording.metadata)
    metadata["format"] = "opentony-retail-recording-v2"
    metadata["format_version"] = VERSION
    metadata["frame_count"] = len(recording.frames)
    metadata["actions"] = [list(action) for action in recording.actions]
    metadata_bytes = _json_bytes(metadata)
    initial_bytes = _json_bytes(recording.initial_state)

    frame_table = bytearray()
    snapshots = bytearray()
    events = bytearray()
    for frame in recording.frames:
        before = _snapshot_bytes(frame.before, frame.raw_player_before, frame.timing_before or _timing_from_snapshot(frame.before))
        after = _snapshot_bytes(frame.after, frame.raw_player_after, frame.timing_after or _timing_from_snapshot(frame.after))
        before_offset = len(snapshots)
        snapshots.extend(before)
        after_offset = len(snapshots)
        snapshots.extend(after)
        input_bytes = _json_bytes(frame.input)
        input_offset = len(snapshots)
        snapshots.extend(struct.pack("<I", len(input_bytes)))
        snapshots.extend(input_bytes)
        event_bytes = _event_bytes(frame.events, frame.frame)
        event_offset = len(events)
        events.extend(event_bytes)
        timer_values = _timer_bytes(frame.timer_samples)
        timer_offset = len(events)
        events.extend(timer_values)
        player_address = 0
        address = frame.before.get("player_address")
        if isinstance(address, str):
            try:
                player_address = int(address, 0) & 0xFFFFFFFF
            except ValueError:
                pass
        action_mask = int(frame.input.get("action_mask", 0)) & 0xFFFFFFFF
        frame_table.extend(FRAME_INDEX_STRUCT.pack(
            frame.frame & 0xFFFFFFFF,
            action_mask,
            0,
            player_address,
            before_offset,
            len(before),
            after_offset,
            len(after),
            input_offset,
            4 + len(input_bytes),
            event_offset,
            len(event_bytes),
            timer_offset,
            len(timer_values),
            0,
            0,
        ))

    header_size = HEADER_STRUCT.size
    initial_offset = header_size
    frame_table_offset = initial_offset + len(initial_bytes)
    snapshot_offset = frame_table_offset + len(frame_table)
    event_offset = snapshot_offset + len(snapshots)
    metadata_offset = event_offset + len(events)
    build_sha = metadata.get("retail_executable_sha256") or metadata.get("binary_sha256") or ""
    try:
        build_sha_bytes = bytes.fromhex(str(build_sha))
    except ValueError:
        build_sha_bytes = b""
    build_sha_bytes = (build_sha_bytes + b"\0" * 32)[:32]
    level = metadata.get("level")
    level_index = int(level.get("index", 0)) if isinstance(level, dict) else 0
    image_base = int(metadata.get("image_base", 0)) & 0xFFFFFFFF
    header = HEADER_STRUCT.pack(
        MAGIC,
        VERSION,
        header_size,
        0,
        len(recording.frames),
        level_index & 0xFFFFFFFF,
        image_base,
        PLAYER_BLOB_SIZE,
        initial_offset,
        len(initial_bytes),
        frame_table_offset,
        len(frame_table),
        snapshot_offset,
        len(snapshots),
        event_offset,
        len(events),
        metadata_offset,
        len(metadata_bytes),
        build_sha_bytes,
    )
    payload = header + initial_bytes + frame_table + snapshots + events + metadata_bytes
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile("wb", dir=target.parent, prefix=f".{target.name}.", delete=False) as stream:
            temporary_name = stream.name
            stream.write(payload)
        os.replace(temporary_name, target)
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return {"path": str(target), "frames": len(recording.frames), "format": "opentony-retail-recording-v2"}


def read_binary_recording(path: str | Path) -> Recording:
    source = Path(path).expanduser()
    try:
        data = source.read_bytes()
    except OSError as exc:
        raise BinaryRecordingError(f"could not read recording {source}: {exc}") from exc
    if len(data) < HEADER_STRUCT.size:
        raise BinaryRecordingError("recording is shorter than its OTREC2 header")
    values = HEADER_STRUCT.unpack_from(data)
    (
        magic, version, header_size, _flags, frame_count, level_index, image_base,
        player_blob_size, initial_offset, initial_size, frame_table_offset,
        frame_table_size, snapshot_offset, snapshot_size, event_offset,
        event_size, metadata_offset, metadata_size, build_sha,
    ) = values
    if magic != MAGIC or version != VERSION or header_size != HEADER_STRUCT.size:
        raise BinaryRecordingError("unsupported OTREC2 magic or version")
    if player_blob_size != PLAYER_BLOB_SIZE:
        raise BinaryRecordingError("unsupported OTREC2 player snapshot size")
    if frame_table_size != frame_count * FRAME_INDEX_STRUCT.size:
        raise BinaryRecordingError("OTREC2 frame table size does not match frame count")
    initial = _read_json(_section(data, initial_offset, initial_size, "initial-state"), "initial-state")
    metadata = _read_json(_section(data, metadata_offset, metadata_size, "metadata"), "metadata")
    if not isinstance(initial, dict) or not isinstance(metadata, dict):
        raise BinaryRecordingError("OTREC2 metadata and initial state must be objects")
    metadata_frame_count = metadata.get("frame_count")
    if metadata_frame_count is not None and metadata_frame_count != frame_count:
        raise BinaryRecordingError("OTREC2 metadata frame count does not match header")
    metadata.setdefault("format", "opentony-retail-recording-v2")
    metadata.setdefault("format_version", VERSION)
    metadata.setdefault("level", {"index": level_index})
    metadata.setdefault("image_base", image_base)
    metadata.setdefault("retail_executable_sha256", build_sha.hex())
    snapshots_data = _section(data, snapshot_offset, snapshot_size, "snapshot")
    frame_data = _section(data, frame_table_offset, frame_table_size, "frame table")
    frames: list[RecordingFrame] = []
    for index in range(frame_count):
        values = FRAME_INDEX_STRUCT.unpack_from(frame_data, index * FRAME_INDEX_STRUCT.size)
        (
            frame_number, action_mask, _input_flags, _player_address,
            before_offset, before_size, after_offset, after_size,
            input_offset, input_size, event_rel_offset, event_size_item,
            timer_rel_offset, timer_size_item, _frame_flags, _reserved,
        ) = values
        if frame_number != index:
            raise BinaryRecordingError(f"frame table is not contiguous at frame {index}")
        if before_offset > snapshot_size or before_size > snapshot_size - before_offset:
            raise BinaryRecordingError(f"frame {index} before snapshot is outside the snapshot section")
        if after_offset > snapshot_size or after_size > snapshot_size - after_offset:
            raise BinaryRecordingError(f"frame {index} after snapshot is outside the snapshot section")
        if input_offset > snapshot_size or input_size > snapshot_size - input_offset:
            raise BinaryRecordingError(f"frame {index} input is outside the snapshot section")
        if event_rel_offset > event_size or event_size_item > event_size - event_rel_offset:
            raise BinaryRecordingError(f"frame {index} events are outside the event section")
        if timer_rel_offset > event_size or timer_size_item > event_size - timer_rel_offset:
            raise BinaryRecordingError(f"frame {index} timer samples are outside the event section")
        before, before_blob, before_timing = _read_snapshot(
            data, snapshot_offset + before_offset, before_size, f"frame {index} before"
        )
        after, after_blob, after_timing = _read_snapshot(
            data, snapshot_offset + after_offset, after_size, f"frame {index} after"
        )
        input_payload = _section(data, snapshot_offset + input_offset, input_size, f"frame {index} input")
        if len(input_payload) < 4:
            raise BinaryRecordingError(f"frame {index} input is truncated")
        input_size_value = struct.unpack_from("<I", input_payload)[0]
        if input_size_value != len(input_payload) - 4:
            raise BinaryRecordingError(f"frame {index} input size is invalid")
        input_value = _read_json(input_payload[4:], f"frame {index} input")
        if not isinstance(input_value, dict):
            raise BinaryRecordingError(f"frame {index} input is not an object")
        input_value.setdefault("action_mask", action_mask)
        event_values = _read_events(data, event_offset + event_rel_offset, event_size_item, index)
        timer_values = _read_timers(data, event_offset + timer_rel_offset, timer_size_item, index)
        frames.append(RecordingFrame(
            frame=index,
            input=input_value,
            before=before,
            after=after,
            events=event_values,
            raw_player_before=before_blob,
            raw_player_after=after_blob,
            timing_before=before_timing,
            timing_after=after_timing,
            timer_samples=timer_values,
        ))
    actions_value = metadata.get("actions", ())
    actions = tuple(
        tuple(int(item) for item in action)
        for action in actions_value
        if isinstance(action, (list, tuple))
    )
    return Recording(
        metadata=metadata,
        frames=tuple(frames),
        initial_state=initial,
        actions=actions,
        source_format="otrec2",
    )
