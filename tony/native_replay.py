"""Run a retail recording through the native gameplay session."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

from .common import resolve
from .recording import validate_recording

REPLAY_MODES = ("assisted", "strict")
NATIVE_REPLAY_WIRE_VERSION = 3
_DERIVED_NATIVE_CHANNELS = (
    "motion_correction",
    "response_correction",
)


def _signed(value: int, bits: int) -> int:
    mask = (1 << bits) - 1
    value &= mask
    sign = 1 << (bits - 1)
    return value - (1 << bits) if value & sign else value


def _vector(snapshot: dict[str, Any], name: str) -> list[int]:
    value = snapshot.get(name, {})
    raw = value.get("raw") if isinstance(value, dict) else None
    if not isinstance(raw, list) or len(raw) != 3:
        raise ValueError(f"snapshot field {name!r} is not a three-component raw vector")
    return [_signed(int(component), 32) for component in raw]


def _orientation(snapshot: dict[str, Any]) -> list[int]:
    value = snapshot.get("orientation")
    if not isinstance(value, dict):
        raise TypeError("snapshot has no orientation")
    result: list[int] = []
    for row_name in ("row_0", "row_1", "row_2"):
        row = value.get(row_name)
        raw = row.get("raw") if isinstance(row, dict) else None
        if not isinstance(raw, list) or len(raw) != 3:
            raise ValueError(f"snapshot orientation {row_name!r} is incomplete")
        result.extend(_signed(int(component), 16) for component in raw)
    return result


def _snapshot_fields(snapshot: dict[str, Any]) -> dict[str, Any]:
    turn = snapshot.get("turn")
    accumulator = turn.get("accumulator_raw") if isinstance(turn, dict) else None
    if not isinstance(accumulator, int):
        raise TypeError("snapshot has no turn accumulator")
    physics_state = snapshot.get("physics_state")
    if not isinstance(physics_state, int):
        raise TypeError("snapshot has no physics state")
    animation = snapshot.get("animation")
    if not isinstance(animation, dict):
        animation = {}
    animation_values = (
        animation.get("id_raw", 0),
        animation.get("frame_raw", 0),
        animation.get("fraction_raw", 0),
        animation.get("rate_raw", 0x10000),
        animation.get("mode_raw", 0),
        animation.get("direction_raw", 0),
        animation.get("endpoint_raw", 0),
        animation.get("alternate_endpoint_raw", -1),
        animation.get("finished_raw", 1),
    )
    if not all(isinstance(value, int) for value in animation_values):
        raise TypeError("snapshot has invalid animation state")
    return {
        "position": _vector(snapshot, "position"),
        "previous_position": _vector(snapshot, "position_history"),
        "collision_response": _vector(snapshot, "response_velocity"),
        "motion_correction": _vector(snapshot, "correction"),
        "air_motion": _vector(snapshot, "air_motion"),
        "physics_state": _signed(physics_state, 32),
        "turn_accumulator": _signed(accumulator, 32),
        "animation": animation_values,
        "orientation": _orientation(snapshot),
    }


def _initial_wire(snapshot: dict[str, Any]) -> str:
    fields = _snapshot_fields(snapshot)
    values: list[int] = []
    for name in (
        "position",
        "previous_position",
        "collision_response",
        "motion_correction",
        "air_motion",
    ):
        values.extend(fields[name])
    values.extend((fields["physics_state"], 0, 0, fields["turn_accumulator"]))
    values.extend(fields["orientation"])
    values.extend(fields["animation"])
    return "init " + " ".join(str(value) for value in values)


def _frame_wire(frame: dict[str, Any], *, mode: str = "assisted") -> str:
    if mode not in REPLAY_MODES:
        raise ValueError(
            f"unsupported native replay mode {mode!r}; "
            f"choose one of {', '.join(REPLAY_MODES)}"
        )
    input_record = frame.get("input")
    if not isinstance(input_record, dict):
        raise TypeError(f"frame {frame.get('frame')} has no input record")
    axes = input_record.get("normalized_axes")
    if not isinstance(axes, dict):
        raw_axes = input_record.get("raw_axes")
        axes = raw_axes.get("normalized") if isinstance(raw_axes, dict) else None
    if not isinstance(axes, dict):
        axes = {}
    frame_index = frame.get("frame")
    if not isinstance(frame_index, int):
        raise TypeError("frame has no integer index")
    action_mask = int(input_record.get("action_mask", 0)) & 0xFFFF
    horizontal = _signed(int(axes.get("horizontal", 0)), 8)
    vertical = _signed(int(axes.get("vertical", 0)), 8)
    timing = frame.get("before")
    timing = timing.get("timing") if isinstance(timing, dict) else None
    scale_record = timing.get("animation_time_scale") \
        if isinstance(timing, dict) else None
    scale_raw = scale_record.get("raw") if isinstance(scale_record, dict) else None
    frame_scale_q8 = _signed(int(scale_raw), 32) if isinstance(scale_raw, int) else 0x100
    random_by_purpose: dict[str, int] = {}
    events = frame.get("events", [])
    if mode == "strict":
        # Strict replay still consumes causal random draws. Only captured
        # derived state is excluded; the native producers must recompute it.
        events = [
            event for event in events
            if not isinstance(event, dict)
            or event.get("type") not in {
                "motion_correction_input",
                "response_correction_input",
            }
        ] if isinstance(events, list) else []
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, dict) or event.get("type") != "ollie_random_input":
                continue
            purpose = event.get("purpose")
            raw_roll = event.get("raw_roll")
            if not isinstance(purpose, str) or not isinstance(raw_roll, int):
                raise TypeError(f"frame {frame_index} has an invalid ollie random event")
            if purpose in random_by_purpose:
                raise ValueError(f"frame {frame_index} has duplicate ollie random purpose {purpose!r}")
            random_by_purpose[purpose] = _signed(raw_roll, 32)

    random_purposes = (
        "charge_cap.first",
        "charge_cap.second",
        "charge_refresh.first",
        "charge_refresh.second",
        "impulse.first",
        "impulse.second",
        "impulse.third",
        "impulse.fourth",
        "impulse.fifth",
        "early_release.first",
        "early_release.second",
    )
    random_values = [random_by_purpose.get(purpose, 0) for purpose in random_purposes]
    random_available = int(bool(random_by_purpose))
    metric_event = next(
        (
            event
            for event in events
            if isinstance(event, dict) and event.get("type") == "ollie_random_input"
        ),
        None,
    ) if isinstance(events, list) else None
    metrics = [
        _signed(int(metric_event.get(name, 0)), 32)
        if isinstance(metric_event, dict) and isinstance(metric_event.get(name, 0), int)
        else 0
        for name in ("slope_metric", "horizontal_speed_metric", "height_delta_metric")
    ]
    damping_by_purpose: dict[str, int] = {}
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, dict) or event.get("type") != "velocity_damping_random_input":
                continue
            purpose = event.get("purpose")
            raw_roll = event.get("raw_roll")
            if not isinstance(purpose, str) or not isinstance(raw_roll, int):
                raise TypeError(
                    f"frame {frame_index} has an invalid velocity damping random event"
                )
            if purpose in damping_by_purpose:
                raise ValueError(
                    f"frame {frame_index} has duplicate velocity damping purpose {purpose!r}"
                )
            damping_by_purpose[purpose] = _signed(raw_roll, 32)
    damping_values = [
        damping_by_purpose.get(purpose, 0)
        for purpose in (
            "rescale_threshold",
            "rescale_x",
            "rescale_y",
            "rescale_z",
            "decay_threshold",
        )
    ]
    damping_available = int(bool(damping_by_purpose))
    motion_events = [
        event
        for event in events
        if isinstance(event, dict) and event.get("type") == "motion_correction_input"
    ] if isinstance(events, list) else []
    if len(motion_events) > 1:
        raise ValueError(f"frame {frame_index} has duplicate motion correction events")
    motion_event = motion_events[0] if motion_events else None
    motion_values = (
        motion_event.get("correction_s32")
        if isinstance(motion_event, dict)
        else None
    )
    if not isinstance(motion_values, list) or len(motion_values) != 3:
        raw = motion_event.get("correction_raw") if isinstance(motion_event, dict) else None
        motion_values = (
            [_signed(int(value), 32) for value in raw]
            if isinstance(raw, list) and len(raw) == 3
            else [0, 0, 0]
        )
    motion_available = int(motion_event is not None and mode == "assisted")
    response_events = [
        event
        for event in events
        if isinstance(event, dict) and event.get("type") == "response_correction_input"
    ] if isinstance(events, list) else []
    if len(response_events) > 1:
        raise ValueError(f"frame {frame_index} has duplicate response correction events")
    response_event = response_events[0] if response_events else None
    response_values = (
        response_event.get("operand_s32")
        if isinstance(response_event, dict)
        else None
    )
    if not isinstance(response_values, list) or len(response_values) != 3:
        raw = response_event.get("operand_raw") if isinstance(response_event, dict) else None
        response_values = (
            [_signed(int(value), 32) for value in raw]
            if isinstance(raw, list) and len(raw) == 3
            else [0, 0, 0]
        )
    response_available = int(response_event is not None and mode == "assisted")
    before_snapshot = frame.get("before")
    physics_snapshot = (
        before_snapshot.get("physics")
        if isinstance(before_snapshot, dict)
        else None
    )
    raw_physics_words = (
        before_snapshot.get("raw_physics_words")
        if isinstance(before_snapshot, dict)
        else None
    )
    gravity_acceleration = 0
    air_control_available = 0
    if isinstance(raw_physics_words, list) and len(raw_physics_words) > 11:
        gravity_acceleration = _signed(int(raw_physics_words[11]), 32)
        if isinstance(physics_snapshot, dict) and isinstance(
            physics_snapshot.get("air_control_enabled"), bool
        ):
            air_control_available = 1
    air_control_enabled = int(
        isinstance(physics_snapshot, dict)
        and bool(physics_snapshot.get("air_control_enabled", False))
    )
    return "frame " + " ".join(
        str(value)
        for value in (
            frame_index,
            action_mask,
            horizontal,
            vertical,
            frame_scale_q8,
            random_available,
            *random_values,
            *metrics,
            damping_available,
            *damping_values,
            motion_available,
            *motion_values,
            response_available,
            *response_values,
            air_control_available,
            gravity_acceleration,
            air_control_enabled,
        )
    )


def _wire_input(
    initial: dict[str, Any],
    frames: list[dict[str, Any]],
    *,
    mode: str = "assisted",
) -> str:
    if mode not in REPLAY_MODES:
        raise ValueError(
            f"unsupported native replay mode {mode!r}; "
            f"choose one of {', '.join(REPLAY_MODES)}"
        )
    lines = [f"version {NATIVE_REPLAY_WIRE_VERSION}", _initial_wire(initial)]
    lines.extend(_frame_wire(frame, mode=mode) for frame in frames)
    lines.append("end")
    return "\n".join(lines) + "\n"


def _native_frame_fields(values: list[int]) -> dict[str, Any]:
    if len(values) != 28:
        raise ValueError(f"native replay frame has {len(values)} values; expected 28")
    cursor = 0

    def take(count: int) -> list[int]:
        nonlocal cursor
        result = values[cursor : cursor + count]
        cursor += count
        return result

    fields = {
        "position": take(3),
        "previous_position": take(3),
        "collision_response": take(3),
        "motion_correction": take(3),
        "air_motion": take(3),
        "physics_state": values[cursor],
        "ground_update_state": values[cursor + 1],
        "ground_physics_mode": values[cursor + 2],
        "turn_accumulator": values[cursor + 3],
    }
    cursor += 4
    fields["orientation"] = take(9)
    if cursor != len(values):
        raise ValueError("native replay frame has trailing values")
    return fields


def _parse_native_output(output: str) -> list[tuple[int, dict[str, Any]]]:
    lines = [line for line in output.splitlines() if line.strip()]
    if not lines or lines[0].strip() != "native-replay-v1":
        raise ValueError("native replay emitted no recognized trace header")
    frames: list[tuple[int, dict[str, Any]]] = []
    for line in lines[1:]:
        values = line.split()
        if not values or values[0] != "frame":
            raise ValueError(f"native replay emitted an unknown trace record: {line}")
        if len(values) < 2:
            raise ValueError("native replay frame has no index")
        index = int(values[1])
        frames.append((index, _native_frame_fields([int(value) for value in values[2:]])))
    return frames


def _trace_record(index: int, fields: dict[str, Any]) -> dict[str, Any]:
    return {"type": "frame", "frame": index, "native": fields}


def _compare_frame(
    retail: dict[str, Any],
    native: dict[str, Any],
) -> tuple[str, Any, Any] | None:
    expected = _snapshot_fields(retail)
    for name in (
        "position",
        "previous_position",
        "collision_response",
        "motion_correction",
        "air_motion",
        "physics_state",
        "turn_accumulator",
        "orientation",
    ):
        if expected[name] != native[name]:
            return name, expected[name], native[name]
    return None


def _level_paths(args, header: dict[str, Any]) -> tuple[Path, Path, Path]:
    asset_root = resolve(args.asset_root) if args.asset_root else resolve("build/assets/all-pkr/files/data")
    trg = resolve(args.trg) if args.trg else asset_root / "SKWARE_T.TRG"
    psx = resolve(args.psx) if args.psx else asset_root / "SKWARE.PSX"
    level = header.get("level")
    level_name = level.get("name") if isinstance(level, dict) else None
    if level_name != "warehouse":
        raise SystemExit(f"native replay currently supports only Warehouse, not {level_name!r}")
    return trg, psx, asset_root


def replay_native(args) -> int:
    mode = getattr(args, "mode", "assisted")
    if mode not in REPLAY_MODES:
        raise SystemExit(
            f"unsupported native replay mode {mode!r}; "
            f"choose one of {', '.join(REPLAY_MODES)}"
        )
    path = resolve(args.path)
    summary, errors = validate_recording(path)
    if errors:
        print(json.dumps({"summary": summary, "errors": errors}, indent=2, sort_keys=True))
        return 1

    records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    header = records[0]
    initial_records = [record for record in records if record.get("type") == "initial_state"]
    frames = [record for record in records if record.get("type") == "frame"]
    if len(initial_records) != 1:
        raise SystemExit("native replay requires exactly one initial_state record")
    trg, psx, asset_root = _level_paths(args, header)
    binary = resolve(args.native_binary) if args.native_binary else resolve("build/native/opentony_native_replay")
    if not binary.is_file():
        raise SystemExit(
            f"native replay executable not found: {binary}\n"
            "build it with: cmake --build build/native --target opentony_native_replay"
        )

    try:
        result = subprocess.run(
            [str(binary), str(trg), str(psx), str(asset_root)],
            input=_wire_input(initial_records[0]["state"], frames, mode=mode),
            text=True,
            capture_output=True,
            check=False,
            cwd=Path.cwd(),
        )
    except OSError as exc:
        raise SystemExit(f"could not run native replay: {exc}") from exc
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip()
        raise SystemExit(f"native replay failed ({result.returncode}): {detail}")

    try:
        native_frames = _parse_native_output(result.stdout)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    if len(native_frames) != len(frames):
        raise SystemExit(
            f"native replay emitted {len(native_frames)} frames; recording contains {len(frames)}"
        )

    trace_records: list[dict[str, Any]] = [
        {
            "type": "header",
            "format": "opentony-native-replay-v1",
            "recording": str(path),
            "recording_id": header.get("recording_id"),
            "mode": mode,
        }
    ]
    difference: tuple[int, str, Any, Any] | None = None
    for recording_frame, (native_index, native_fields) in zip(frames, native_frames):
        if native_index != recording_frame.get("frame"):
            raise SystemExit(
                f"native replay frame index {native_index} does not match recording frame "
                f"{recording_frame.get('frame')}"
            )
        trace_records.append(_trace_record(native_index, native_fields))
        if difference is None:
            frame_difference = _compare_frame(recording_frame["after"], native_fields)
            if frame_difference is not None:
                name, expected, actual = frame_difference
                difference = (native_index, name, expected, actual)
    trace_records.append(
        {
            "type": "end",
            "frames": len(native_frames),
            "result": "divergent" if difference else "matching",
        }
    )

    output = resolve(args.output) if args.output else resolve(
        f"build/parity/{path.stem}.native.jsonl"
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "".join(json.dumps(record, sort_keys=True) + "\n" for record in trace_records),
        encoding="utf-8",
    )

    print(f"native trace: {output}")
    print(f"frames: {len(native_frames)}")
    print(f"mode: {mode}")
    print(
        "injected derived channels: "
        + (", ".join(_DERIVED_NATIVE_CHANNELS) if mode == "assisted" else "none")
    )
    if mode == "strict":
        print(
            "note: native strict mode disables captured derived seams; "
            "their native producers are not yet complete"
        )
    if difference is None:
        print(f"matching: {len(native_frames)}")
        print("result: matching")
        return 0
    frame, name, expected, actual = difference
    print(f"first divergence: frame {frame}")
    print(f"field: {name}")
    print(f"retail  = {expected!r}")
    print(f"native  = {actual!r}")
    print(f"matching: {frame}")
    print("result: divergent")
    return 1
