"""Offline decoding for the tiny Windows in-process capture recorder.

The injected component writes a bounded, fixed-layout ``.otcap`` file.  This
module is the only place that translates that transport into the established
JSONL ``.otrec`` contract; retail and native replay do not need to know which
recorder produced a recording.
"""

from __future__ import annotations

import json
import math
import struct
import subprocess
import tempfile
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from .common import ROOT, headless_wine_command, headless_wine_env, load_yaml, resolve
from .display import HeadlessDisplay, terminate_process
from .nocd import nocd_executable

OTCAP_MAGIC = b"OTCAP\0\0\1"
OTCAP_VERSION = 1
OTCAP_MAPPING_SIZE = 64 * 1024 * 1024
OTCAP_PLAYER_BLOB_SIZE = 0x3210
OTCAP_MAX_ACTION_INTERVALS = 128
OTCAP_MAX_TIMER_SAMPLES = 8
OTCAP_MAX_CAUSAL_EVENTS = 16
SUPPORTED_BUILD_SHA256 = "f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c"

STATUS_INITIALIZING = 0
STATUS_READY = 1
STATUS_CAPTURING = 2
STATUS_COMPLETE = 3
STATUS_FAILED = 4
STATUS_OVERFLOW = 5

ACTION_MASKS = {
    "jump": 0x0010,
    "grind": 0x0080,
    "grab": 0x0020,
    "kick": 0x0040,
    "spinleft": 0x0004,
    "nollie": 0x0001,
    "spinright": 0x0008,
    "switch": 0x0002,
    "left": 0x8000,
    "right": 0x2000,
    "up": 0x1000,
    "down": 0x4000,
}

HEADER_STRUCT = struct.Struct("<8s17I32s")
CONFIG_PREFIX_STRUCT = struct.Struct("<6I32s")
ACTION_STRUCT = struct.Struct("<3I")
INITIAL_STATE_STRUCT = struct.Struct("<6I2Q4I")
TIMING_STRUCT = struct.Struct("<6I")
TIMER_SAMPLE_STRUCT = struct.Struct("<8I2Q")
EVENT_STRUCT = struct.Struct("<4I64s")
FRAME_HEADER_STRUCT = struct.Struct("<9I")
FRAME_STRUCT = struct.Struct(
    f"<9I{OTCAP_PLAYER_BLOB_SIZE}s{OTCAP_PLAYER_BLOB_SIZE}s"
    "6I6I"
    f"{('8I2Q' * OTCAP_MAX_TIMER_SAMPLES)}"
    f"{('4I64s' * OTCAP_MAX_CAUSAL_EVENTS)}"
)


class CaptureDecodeError(ValueError):
    """The bounded capture is malformed, incomplete, or from an unknown build."""


def _signed32(value: int) -> int:
    return value - 0x100000000 if value & 0x80000000 else value


def _word(value: int) -> dict[str, int]:
    return {"raw": value & 0xFFFFFFFF, "signed": _signed32(value & 0xFFFFFFFF)}


def _vec(blob: bytes, offset: int) -> dict[str, list[int]]:
    raw = list(struct.unpack_from("<3I", blob, offset))
    return {"raw": raw, "signed": [_signed32(value) for value in raw]}


def _short_vec(blob: bytes, offset: int) -> dict[str, list[int]]:
    raw = list(struct.unpack_from("<3H", blob, offset))
    return {
        "raw": raw,
        "signed": [value - 0x10000 if value & 0x8000 else value for value in raw],
    }


def _timing(values: tuple[int, ...]) -> dict[str, dict[str, int]]:
    names = (
        "animation_clock",
        "animation_time_scale",
        "animation_time_scale_square",
        "animation_clock_accumulator",
        "simulation_time",
        "timing_delta_q11",
    )
    return {name: _word(value) for name, value in zip(names, values)}


def _snapshot(blob: bytes, timing: tuple[int, ...], player_address: int, input_flags: int) -> dict[str, Any]:
    if len(blob) != OTCAP_PLAYER_BLOB_SIZE:
        raise CaptureDecodeError(f"player snapshot has {len(blob)} bytes; expected 0x{OTCAP_PLAYER_BLOB_SIZE:x}")
    physics_state = struct.unpack_from("<I", blob, 0x30B8)[0]
    return {
        "player_address": f"0x{player_address:08x}",
        "timing": _timing(timing),
        "raw_physics_words": list(struct.unpack_from(f"<{0x490 // 4}I", blob, 0x2D80)),
        "physics_state": physics_state,
        "physics": {
            "state_raw": physics_state,
            "previous_state_raw": struct.unpack_from("<I", blob, 0x30C0)[0],
            "auxiliary_state_raw": struct.unpack_from("<I", blob, 0x30C4)[0],
            "air_control_enabled": bool(input_flags & 1),
        },
        "position": _vec(blob, 0x08),
        "position_history": _vec(blob, 0xBC),
        "response_velocity": _vec(blob, 0x4C),
        "correction": _vec(blob, 0x58),
        "air_motion": _vec(blob, 0x310C),
        "turn": {
            "accumulator_raw": struct.unpack_from("<I", blob, 0x3144)[0],
            "mirror_raw": struct.unpack_from("<I", blob, 0x3148)[0],
        },
        "basis": {
            "forward_raw": _vec(blob, 0x30F4),
            "up_raw": _vec(blob, 0x3100),
            "air_raw": _vec(blob, 0x310C),
        },
        "orientation": {
            "row_0": _short_vec(blob, 0x2E58),
            "row_1": _short_vec(blob, 0x2E5E),
            "row_2": _short_vec(blob, 0x2E64),
        },
        "animation": {
            "id_raw": struct.unpack_from("<H", blob, 0xF6)[0],
            "frame_raw": struct.unpack_from("<h", blob, 0xF4)[0],
            "fraction_raw": struct.unpack_from("<H", blob, 0x104)[0],
            "rate_raw": struct.unpack_from("<I", blob, 0x108)[0],
            "mode_raw": blob[0xF8],
            "direction_raw": struct.unpack_from("<b", blob, 0x100)[0],
            "endpoint_raw": struct.unpack_from("<b", blob, 0x101)[0],
            "alternate_endpoint_raw": struct.unpack_from("<b", blob, 0x102)[0],
            "finished_raw": blob[0x107],
        },
    }


def _float_value(raw: int) -> float | dict[str, str]:
    value = struct.unpack("<d", struct.pack("<Q", raw))[0]
    if math.isfinite(value):
        return value
    return {"non_finite_float": value.hex()}


def _initial_state(values: tuple[int, ...]) -> dict[str, Any]:
    (
        _size,
        timer_handle,
        interval_ms,
        opaque_08,
        accumulated_ms,
        opaque_10,
        public_accumulator_raw,
        simulation_accumulator_raw,
        public_tick,
        simulation_time,
        pause_a,
        pause_b,
    ) = values
    return {
        "timer_handle": timer_handle,
        "interval_ms": interval_ms,
        "opaque_08": opaque_08,
        "accumulated_ms": accumulated_ms,
        "opaque_10": opaque_10,
        "public_accumulator": {"raw": public_accumulator_raw, "value": _float_value(public_accumulator_raw)},
        "simulation_accumulator": {
            "raw": simulation_accumulator_raw,
            "value": _float_value(simulation_accumulator_raw),
        },
        "public_tick": public_tick,
        "simulation_time": simulation_time,
        "simulation_pause_gate_a": bool(pause_a),
        "simulation_pause_gate_b": bool(pause_b),
    }


def _phase_name(phase: int) -> str:
    return {
        1: "physics_entry",
        2: "timer_update",
        3: "clock_read",
        4: "post_physics",
    }.get(phase, f"phase_{phase}")


def _decode_timer(values: tuple[int, ...]) -> dict[str, Any]:
    (
        phase,
        frame_index,
        interval_ms,
        accumulated_ms,
        public_tick,
        simulation_time,
        pause_a,
        pause_b,
        public_accumulator_raw,
        simulation_accumulator_raw,
    ) = values
    return {
        "type": "timer_boundary_sample",
        "timer_boundary_phase": _phase_name(phase),
        "frame": frame_index,
        "interval_ms": interval_ms,
        "accumulated_ms": accumulated_ms,
        "public_tick": public_tick,
        "simulation_time": simulation_time,
        "simulation_pause_gate_a": bool(pause_a),
        "simulation_pause_gate_b": bool(pause_b),
        "public_accumulator": {"raw": public_accumulator_raw, "value": _float_value(public_accumulator_raw)},
        "simulation_accumulator": {
            "raw": simulation_accumulator_raw,
            "value": _float_value(simulation_accumulator_raw),
        },
    }


def _normalize_timer_samples(samples: list[tuple[int, ...]]) -> list[tuple[int, ...]]:
    """Assign deliveries between timer-update and physics entry to update.

    The in-process timer seam observes the callback counter immediately before
    the retail update calls.  A callback may then complete before physics
    entry, so the counter delta is first visible in that entry sample.  Replay
    must publish those deliveries at the timer-update boundary or the retail
    loop can wait forever for the callback-owned counter to advance.
    """

    normalized = [list(sample) for sample in samples]
    for index in range(1, len(normalized)):
        previous = normalized[index - 1]
        current = normalized[index]
        if (
            current[0] == 1
            and previous[0] == 2
            and current[1] == previous[1]
            and current[3] != previous[3]
        ):
            current[0] = 2
    return [tuple(sample) for sample in normalized]


def _timer_float(boundary: dict[str, Any], name: str) -> float | None:
    value = boundary.get(name)
    if isinstance(value, dict):
        value = value.get("value")
    try:
        value = float(value)
    except (TypeError, ValueError):
        return None
    return value if math.isfinite(value) else None


def _timer_delivery_count(
    previous: dict[str, Any], current: dict[str, Any], interval: int | None = None
) -> int | None:
    """Infer callback deliveries from the sampled 32-bit timer counter."""

    if interval is None:
        interval = int(previous.get("interval_ms", 0)) & 0xFFFFFFFF
    else:
        interval = int(interval) & 0xFFFFFFFF
    if interval <= 0:
        return None
    delta = (
        int(current.get("accumulated_ms", 0))
        - int(previous.get("accumulated_ms", 0))
    ) & 0xFFFFFFFF
    return delta // interval if delta % interval == 0 else None


def _decode_timer_events(
    samples: list[dict[str, Any]], state: dict[str, Any]
) -> list[dict[str, Any]]:
    """Convert raw timer samples to the replay service's causal event form.

    The callback increments the integer counter once per delivery.  Keeping
    this inference offline means the injected DLL only performs fixed-size
    reads, while replay still receives the same boundary metadata as GDB.
    """

    previous = dict(state["previous"])
    pending_deliveries = int(state["pending_deliveries"])
    logical_accumulated = int(state["logical_accumulated"])
    decoded: list[dict[str, Any]] = []
    for sample in samples:
        if not state["seen"]:
            # The GDB sampler establishes its baseline on the first boundary
            # and emits no replay event for that baseline observation.
            previous = dict(sample)
            logical_accumulated = int(sample.get("accumulated_ms", logical_accumulated)) & 0xFFFFFFFF
            state["seen"] = True
            continue
        interval = int(sample.get("interval_ms", 0)) & 0xFFFFFFFF
        previous_interval = int(previous.get("interval_ms", 0)) & 0xFFFFFFFF
        counter_deliveries = _timer_delivery_count(previous, sample, previous_interval)
        if counter_deliveries is None:
            # Match the GDB sampler's interval-change fallback.  A malformed
            # non-divisible delta remains conservative (zero deliveries).
            counter_deliveries = _timer_delivery_count(previous, sample, interval) or 0
        completed = counter_deliveries
        pending = 0
        # A callback can be sampled after its integer counter increment but
        # before its floating stores.  When both pause gates are open, use the
        # simulation accumulator as the completion signal and defer the torn
        # delivery exactly as the GDB sampler does.
        previous_simulation = _timer_float(previous, "simulation_accumulator")
        current_simulation = _timer_float(sample, "simulation_accumulator")
        delta_per_delivery = float(previous_interval) * 0.001 * 60.0
        gates_open = not any(
            bool(previous.get(name)) or bool(sample.get(name))
            for name in ("simulation_pause_gate_a", "simulation_pause_gate_b")
        )
        if (
            previous_interval == interval
            and gates_open
            and previous_simulation is not None
            and current_simulation is not None
            and delta_per_delivery > 0.0
        ):
            candidate = pending_deliveries + counter_deliveries
            observed = current_simulation - previous_simulation
            candidate_completed = round(observed / delta_per_delivery)
            tolerance = 1.0e-6 * max(1.0, abs(current_simulation))
            if (
                0 <= candidate_completed <= candidate
                and abs(observed - candidate_completed * delta_per_delivery) <= tolerance
            ):
                completed = candidate_completed
                pending = candidate - completed
        logical_before = logical_accumulated
        logical_after = (logical_before + completed * previous_interval) & 0xFFFFFFFF
        metadata = {
            "timer_boundary_before": {
                "interval_ms": previous_interval,
                "accumulated_ms": logical_before,
            },
            "timer_boundary_after": {
                "interval_ms": previous_interval,
                "accumulated_ms": logical_after,
            },
            "timer_boundary_sampled_accumulated_ms": int(sample.get("accumulated_ms", 0)),
            "timer_boundary_pending_delivery_count": pending,
            "timer_boundary_delivery_count": completed,
        }
        if completed:
            for ordinal in range(completed):
                decoded.append({
                    **metadata,
                    "type": "timer_callback_delivery",
                    "frame": sample.get("frame", 0),
                    "callback_ordinal": ordinal,
                    "interval_ms": previous_interval,
                    "callback_arg0": previous_interval,
                    "callback_arg1": 0,
                    "delivery_source": "timer_state_boundary_delta",
                    "timer_boundary_phase": sample.get("timer_boundary_phase", "physics_entry"),
                })
        else:
            decoded.append({
                **sample,
                **metadata,
                "delivery_source": "timer_state_boundary_delta",
            })
        pending_deliveries = pending
        logical_accumulated = logical_after
        previous = dict(sample)
    state["previous"] = previous
    state["pending_deliveries"] = pending_deliveries
    state["logical_accumulated"] = logical_accumulated
    return decoded


def decode_capture(path: str | Path) -> dict[str, Any]:
    """Decode and validate one complete fixed-layout ``.otcap`` file."""

    source = resolve(path)
    try:
        data = source.read_bytes()
    except OSError as exc:
        raise CaptureDecodeError(f"could not read capture {source}: {exc}") from exc
    if len(data) < HEADER_STRUCT.size:
        raise CaptureDecodeError("capture is shorter than its header")
    fields = HEADER_STRUCT.unpack_from(data)
    (
        magic,
        version,
        header_size,
        config_offset,
        config_size,
        initial_offset,
        initial_size,
        data_offset,
        mapping_size,
        bytes_used,
        frame_count,
        frame_limit,
        status,
        error_code,
        level_index,
        image_base,
        player_blob_size,
        process_id,
        build_sha256,
    ) = fields
    if magic != OTCAP_MAGIC or version != OTCAP_VERSION:
        raise CaptureDecodeError("unsupported .otcap magic or version")
    if header_size != HEADER_STRUCT.size or config_size != CONFIG_PREFIX_STRUCT.size + ACTION_STRUCT.size * OTCAP_MAX_ACTION_INTERVALS:
        raise CaptureDecodeError("capture layout version does not match this decoder")
    if mapping_size != OTCAP_MAPPING_SIZE or player_blob_size != OTCAP_PLAYER_BLOB_SIZE:
        raise CaptureDecodeError("capture mapping has unsupported bounds")
    if status in {STATUS_FAILED, STATUS_OVERFLOW}:
        raise CaptureDecodeError(f"capture failed in process (status={status}, error={error_code})")
    if status != STATUS_COMPLETE:
        raise CaptureDecodeError(f"capture is incomplete (status={status})")
    if frame_count != frame_limit:
        raise CaptureDecodeError(
            f"capture stopped at {frame_count} frames; expected {frame_limit}"
        )
    if bytes_used > len(data) or bytes_used < data_offset:
        raise CaptureDecodeError("capture bytes_used is outside the file")
    if frame_count > frame_limit or frame_count > (bytes_used - data_offset) // FRAME_STRUCT.size:
        raise CaptureDecodeError("capture frame count exceeds its bounded mapping")
    if config_offset + config_size > bytes_used or initial_offset + initial_size > bytes_used:
        raise CaptureDecodeError("capture metadata extends past bytes_used")
    if initial_size != INITIAL_STATE_STRUCT.size:
        raise CaptureDecodeError("capture initial-state size is invalid")
    if config_size != CONFIG_PREFIX_STRUCT.size + ACTION_STRUCT.size * OTCAP_MAX_ACTION_INTERVALS:
        raise CaptureDecodeError("capture config size is invalid")
    config_values = CONFIG_PREFIX_STRUCT.unpack_from(data, config_offset)
    config_version, _config_size, config_frames, action_count, _config_level, flags, config_sha = config_values
    if config_version != OTCAP_VERSION or config_frames != frame_limit or action_count > OTCAP_MAX_ACTION_INTERVALS:
        raise CaptureDecodeError("capture config is invalid")
    if config_sha != build_sha256:
        raise CaptureDecodeError("capture header/config build identities differ")
    if build_sha256.hex() != SUPPORTED_BUILD_SHA256:
        raise CaptureDecodeError(f"unsupported capture build identity: {build_sha256.hex()}")
    actions = [
        ACTION_STRUCT.unpack_from(data, config_offset + CONFIG_PREFIX_STRUCT.size + index * ACTION_STRUCT.size)
        for index in range(action_count)
    ]
    initial_values = INITIAL_STATE_STRUCT.unpack_from(data, initial_offset)
    if initial_values[0] != initial_size:
        raise CaptureDecodeError("capture initial-state size is invalid")
    initial_timer = _initial_state(initial_values)
    timer_decode_state: dict[str, Any] = {
        "previous": initial_timer,
        "pending_deliveries": 0,
        "logical_accumulated": int(initial_timer.get("accumulated_ms", 0)) & 0xFFFFFFFF,
        "seen": False,
    }

    frames = []
    for index in range(frame_count):
        offset = data_offset + index * FRAME_STRUCT.size
        if offset + FRAME_STRUCT.size > bytes_used:
            raise CaptureDecodeError(f"capture frame {index} is truncated")
        values = FRAME_STRUCT.unpack_from(data, offset)
        header_values = values[:9]
        (
            frame_index,
            input_mask,
            input_flags,
            player_address,
            timer_count,
            event_count,
            before_size,
            after_size,
            _frame_flags,
        ) = header_values
        if frame_index != index or before_size != OTCAP_PLAYER_BLOB_SIZE or after_size != OTCAP_PLAYER_BLOB_SIZE:
            raise CaptureDecodeError(f"capture frame {index} has invalid header")
        if timer_count > OTCAP_MAX_TIMER_SAMPLES or event_count > OTCAP_MAX_CAUSAL_EVENTS:
            raise CaptureDecodeError(f"capture frame {index} exceeds fixed event bounds")
        before_start = 9
        after_start = before_start + 1
        before = values[before_start]
        after = values[after_start]
        timing_before = values[after_start + 1 : after_start + 7]
        timing_after = values[after_start + 7 : after_start + 13]
        timer_start = after_start + 13
        timer_values = values[timer_start : timer_start + OTCAP_MAX_TIMER_SAMPLES * 10]
        event_start = timer_start + OTCAP_MAX_TIMER_SAMPLES * 10
        event_values = values[event_start:]
        timer_samples = _normalize_timer_samples([
            tuple(timer_values[item * 10 : item * 10 + 10])
            for item in range(timer_count)
        ])
        timers = [_decode_timer(sample) for sample in timer_samples]
        events = [
            {
                "type": "inproc_causal_event",
                "event_code": event_values[item * 5],
                "phase": event_values[item * 5 + 1],
                "frame": event_values[item * 5 + 2],
                "payload_size": event_values[item * 5 + 3],
                "payload": event_values[item * 5 + 4].hex(),
            }
            for item in range(event_count)
        ]
        frames.append(
            {
                "frame": index,
                "input": {"action_mask": input_mask},
                "before": _snapshot(before, tuple(timing_before), player_address, input_flags),
                "after": _snapshot(after, tuple(timing_after), player_address, input_flags),
                "events": [*_decode_timer_events(timers, timer_decode_state), *events],
            }
        )
    return {
        "build_sha256": build_sha256.hex(),
        "image_base": image_base,
        "process_id": process_id,
        "level_index": level_index,
        "flags": flags,
        "frame_limit": frame_limit,
        "actions": actions,
        "initial_timer_state": initial_timer,
        "frames": frames,
        "status": status,
    }


def convert_capture(source: str | Path, output: str | Path, *, force: bool = False) -> dict[str, Any]:
    """Convert a complete ``.otcap`` into the existing V1 JSONL recording."""

    capture = decode_capture(source)
    target = resolve(output)
    if target.exists() and not force:
        raise CaptureDecodeError(f"refusing to overwrite {target}; use --force if intended")
    level_name = {0: "hangar", 12: "warehouse"}.get(capture["level_index"])
    metadata = {
        "recording_id": f"inproc-{target.stem}",
        "recording_timestamp": datetime.now(UTC).isoformat(timespec="milliseconds"),
        "binary_sha256": capture["build_sha256"],
        "retail_executable_sha256": capture["build_sha256"],
        "instrumentation_version": "inproc-capture-v1",
        "capture_backend": "inproc",
        "capture_layout_version": OTCAP_VERSION,
        "image_base": capture["image_base"],
        "frame_boundary": "Skater_PhysicsFrame",
        "input_boundary": "Game_GameplayUpdate",
        "level": {"index": capture["level_index"], "name": level_name},
        "player_identity": {"slot": 0},
        "initial_timer_state": capture["initial_timer_state"],
    }
    frame_records = [
        {"type": "frame", **frame}
        for frame in capture["frames"]
    ]
    records = [{
        "type": "header",
        "format": "opentony-retail-recording-v1",
        "format_version": 1,
        "capture_schema_version": 2,
        **metadata,
    }, {
        "type": "initial_state",
        "frame": 0,
        "state": capture["frames"][0]["before"] if capture["frames"] else {},
    }, *frame_records, {
        "type": "end",
        "format": "opentony-retail-recording-v1",
        "recording_id": metadata["recording_id"],
        "frames": len(capture["frames"]),
        "complete": True,
    }]
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=target.parent, prefix=f".{target.name}.", delete=False) as stream:
            temporary_name = stream.name
            for record in records:
                stream.write(json.dumps(record, sort_keys=True, allow_nan=False) + "\n")
        Path(temporary_name).replace(target)
    finally:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
    return {"path": str(target), "frames": len(capture["frames"]), "format": metadata["instrumentation_version"]}


def _recording_records(path: str | Path) -> list[dict[str, Any]]:
    source = resolve(path)
    try:
        records = [json.loads(line) for line in source.read_text(encoding="utf-8").splitlines()]
    except (OSError, json.JSONDecodeError) as exc:
        raise CaptureDecodeError(f"could not read recording {source}: {exc}") from exc
    if not all(isinstance(record, dict) for record in records):
        raise CaptureDecodeError(f"recording {source} contains a non-object record")
    return records


def _first_difference(expected: Any, actual: Any, path: tuple[Any, ...] = ()) -> tuple[tuple[Any, ...], Any, Any] | None:
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            if key not in expected:
                return path + (key,), "<missing>", actual[key]
            if key not in actual:
                return path + (key,), expected[key], "<missing>"
            difference = _first_difference(expected[key], actual[key], path + (key,))
            if difference is not None:
                return difference
        return None
    if isinstance(expected, list) and isinstance(actual, list):
        if len(expected) != len(actual):
            return path + ("length",), len(expected), len(actual)
        for index, (left, right) in enumerate(zip(expected, actual)):
            difference = _first_difference(left, right, path + (index,))
            if difference is not None:
                return difference
        return None
    return None if expected == actual else (path, expected, actual)


def _comparison_frame(record: dict[str, Any], scope: str) -> dict[str, Any]:
    if scope == "all":
        return record
    if scope != "snapshots":
        raise CaptureDecodeError(f"unknown recording comparison scope: {scope}")
    input_record = record.get("input")
    action_mask = input_record.get("action_mask") if isinstance(input_record, dict) else None
    return {
        "type": "frame",
        "frame": record.get("frame"),
        "input": {"action_mask": action_mask},
        "before": record.get("before"),
        "after": record.get("after"),
    }


def compare_recordings(
    left: str | Path,
    right: str | Path,
    *,
    scope: str = "all",
) -> dict[str, Any]:
    """Compare two JSONL recordings at canonical frame/event boundaries.

    Recording IDs and timestamps are intentionally excluded; all input,
    timer/service events, and player snapshots are compared exactly by default.
    ``scope="snapshots"`` is the M3 qualification mode: it compares the
    overlapping input/action-mask and before/after player snapshots while
    ignoring recorder-specific diagnostic event arrays until those hooks are
    migrated.
    """

    if scope not in {"all", "snapshots"}:
        raise CaptureDecodeError(f"unknown recording comparison scope: {scope}")

    left_records = _recording_records(left)
    right_records = _recording_records(right)
    left_frames = [
        _comparison_frame(record, scope)
        for record in left_records
        if record.get("type") == "frame"
    ]
    right_frames = [
        _comparison_frame(record, scope)
        for record in right_records
        if record.get("type") == "frame"
    ]
    left_header = next((record for record in left_records if record.get("type") == "header"), {})
    right_header = next((record for record in right_records if record.get("type") == "header"), {})
    for key in ("binary_sha256", "retail_executable_sha256", "level", "frame_boundary", "input_boundary"):
        if key not in left_header or key not in right_header:
            continue
        difference = _first_difference(left_header[key], right_header[key], ("header", key))
        if difference is not None:
            path, expected, actual = difference
            return {
                "equal": False,
                "scope": scope,
                "frames": {"left": len(left_frames), "right": len(right_frames)},
                "difference": {"path": list(path), "left": expected, "right": actual},
            }
    if len(left_frames) != len(right_frames):
        return {
            "equal": False,
            "scope": scope,
            "frames": {"left": len(left_frames), "right": len(right_frames)},
            "difference": {"path": ["frames", "length"], "left": len(left_frames), "right": len(right_frames)},
        }
    for index, (left_frame, right_frame) in enumerate(zip(left_frames, right_frames)):
        difference = _first_difference(left_frame, right_frame, ("frames", index))
        if difference is not None:
            path, expected, actual = difference
            return {
                "equal": False,
                "scope": scope,
                "frames": len(left_frames),
                "difference": {"path": list(path), "left": expected, "right": actual},
            }
    return {"equal": True, "scope": scope, "frames": len(left_frames), "difference": None}


def _capture_binary(explicit: str | Path | None, names: tuple[str, ...]) -> Path:
    if explicit:
        candidate = resolve(explicit)
        if candidate.is_file():
            return candidate
        raise CaptureDecodeError(f"capture binary not found: {candidate}")
    for name in names:
        candidate = ROOT / name
        if candidate.is_file():
            return candidate
    joined = ", ".join(str(ROOT / name) for name in names)
    raise CaptureDecodeError(f"capture binary not found; build the Windows component (looked for {joined})")


def _wine_path(path: Path, environment: dict[str, str]) -> str:
    """Translate a host path to a DOS path before passing it to Win32 code."""

    try:
        result = subprocess.run(
            ["winepath", "-w", str(path.resolve())],
            cwd=ROOT,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
    except OSError as exc:
        raise CaptureDecodeError(f"could not run winepath for {path}: {exc}") from exc
    if result.returncode != 0 or not result.stdout.strip():
        raise CaptureDecodeError(
            f"could not translate path for Wine: {path}\n{result.stdout.strip()}"
        )
    return result.stdout.strip()


def _capture_desktop_spec() -> str:
    """Return the Wine virtual-desktop selector used by headless runs."""

    desktop = load_yaml("re/config/wine.yml")["wine"].get("virtual_desktop", {})
    name = str(desktop.get("name", "OpenTony"))
    width = int(desktop.get("width", 1024))
    height = int(desktop.get("height", 768))
    if width < 320 or height < 200:
        raise CaptureDecodeError(f"invalid Wine virtual desktop size: {width}x{height}")
    return f"/desktop={name},{width}x{height}"


def _headless_capture_command(
    command: list[str], output: Path, desktop_spec: str
) -> list[str]:
    """Run the Windows host on a Wine desktop while keeping the caller headless.

    Wine's legacy DirectDraw path exits before frontend initialization when a
    desktop is not present.  ``explorer /desktop`` stays alive after its child
    exits, so this wrapper starts it in the background, waits for the bounded
    host to publish its output, and then tears the desktop down.
    """

    script = (
        'desktop="$1"; output="$2"; shift 2; '
        'wine explorer "$desktop" "$@" >/dev/null 2>&1 & desktop_pid=$!; '
        'ticks=0; '
        'while [ ! -s "$output" ] && kill -0 "$desktop_pid" 2>/dev/null; do '
        'sleep 0.1; ticks=$((ticks + 1)); '
        '[ "$ticks" -lt 1300 ] || { kill "$desktop_pid" 2>/dev/null || true; '
        'wait "$desktop_pid" 2>/dev/null || true; exit 124; }; '
        'done; '
        'if [ ! -s "$output" ]; then wait "$desktop_pid"; exit $?; fi; '
        'kill "$desktop_pid" 2>/dev/null || true; '
        'wait "$desktop_pid" 2>/dev/null || true; exit 0'
    )
    return ["sh", "-c", script, "opentony-capture-desktop", desktop_spec, str(output), *command]


def run_inproc_capture(
    scenario: dict[str, Any],
    output: str | Path,
    *,
    force: bool = False,
    host: str | Path | None = None,
    dll: str | Path | None = None,
) -> int:
    """Run the Windows host/injected recorder and convert its bounded output."""

    target = resolve(output)
    capture_path = target.with_suffix(".otcap")
    if target.exists() and not force:
        print(f"refusing to overwrite {target}; use --force if intended")
        return 1
    if capture_path.exists() and not force:
        print(f"refusing to overwrite {capture_path}; use --force if intended")
        return 1
    try:
        host_path = _capture_binary(host, ("build/capture/win32/opentony_capture_host.exe", "build/capture/win32/Release/opentony_capture_host.exe"))
        dll_path = _capture_binary(dll, ("build/capture/win32/opentony_capture.dll", "build/capture/win32/Release/opentony_capture.dll"))
        executable = nocd_executable()
        build_sha256 = str(load_yaml("re/config/binaries.yml")["executables"]["thps2_pc"]["sha256"])
    except (CaptureDecodeError, KeyError, OSError) as exc:
        print(str(exc))
        return 1
    environment = headless_wine_env()
    command = [
        "wine",
        str(host_path),
        "--exe",
        _wine_path(executable, environment),
        "--dll",
        _wine_path(dll_path, environment),
        "--output",
        _wine_path(capture_path, environment),
        "--build-sha256",
        build_sha256,
        "--frames",
        str(scenario["frames"]),
        "--level",
        str({"hangar": 0, "warehouse": 12}.get(scenario["level"], 0)),
    ]
    if force:
        command.append("--force")
    for action, start, hold in _scenario_action_intervals(scenario):
        command.extend(("--action", f"0x{ACTION_MASKS[action]:x}:{start}:{hold}"))
    print(" ".join(command))
    cfg = load_yaml("re/config/wine.yml")["wine"]
    display = HeadlessDisplay(cfg, environment)
    command = _headless_capture_command(command, capture_path, _capture_desktop_spec())
    process = None
    try:
        process = display.popen(
            headless_wine_command(command), cwd=executable.parent, env=environment
        )
        result_code = process.wait()
    except BaseException:
        if process is not None and process.poll() is None:
            terminate_process(process)
        raise
    finally:
        display.close()
        subprocess.run(
            ["wineserver", "-k"],
            cwd=ROOT,
            env=environment,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    if result_code:
        return result_code
    try:
        summary = convert_capture(capture_path, target, force=force)
    except CaptureDecodeError as exc:
        print(str(exc))
        return 1
    print(f"capture: {summary['path']} ({summary['frames']} frames, converted from {capture_path})")
    return 0


def _scenario_action_intervals(scenario: dict[str, Any]) -> list[tuple[str, int, int]]:
    releases = {
        event["action"]: event["frame"]
        for event in scenario["input"]
        if event["state"] == "release"
    }
    return [
        (event["action"], event["frame"], max(1, releases.get(event["action"], scenario["frames"]) - event["frame"]))
        for event in scenario["input"]
        if event["state"] == "press"
    ]
