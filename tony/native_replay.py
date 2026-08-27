"""Run a retail recording through the native gameplay session."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

from .common import resolve
from .recording import validate_recording


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
    return {
        "position": _vector(snapshot, "position"),
        "previous_position": _vector(snapshot, "position_history"),
        "collision_response": _vector(snapshot, "response_velocity"),
        "motion_correction": _vector(snapshot, "correction"),
        "air_motion": _vector(snapshot, "air_motion"),
        "physics_state": _signed(physics_state, 32),
        "turn_accumulator": _signed(accumulator, 32),
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
    return "init " + " ".join(str(value) for value in values)


def _frame_wire(frame: dict[str, Any]) -> str:
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
    return f"frame {frame_index} {action_mask} {horizontal} {vertical}"


def _wire_input(initial: dict[str, Any], frames: list[dict[str, Any]]) -> str:
    lines = ["version 1", _initial_wire(initial)]
    lines.extend(_frame_wire(frame) for frame in frames)
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
            input=_wire_input(initial_records[0]["state"], frames),
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
