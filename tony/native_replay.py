"""Run a retail recording through the native gameplay session."""

from __future__ import annotations

import json
import struct
import subprocess
from pathlib import Path
from typing import Any

from .common import resolve
from .recording import load_recording, validate_recording

NATIVE_REPLAY_WIRE_VERSION = 11


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
    raw_physics_words = snapshot.get("raw_physics_words")
    ground_motion_threshold = 0x2e9b6
    if isinstance(raw_physics_words, list) and len(raw_physics_words) > 18:
        ground_motion_threshold = _signed(int(raw_physics_words[18]), 32)
    return {
        "position": _vector(snapshot, "position"),
        "previous_position": _vector(snapshot, "position_history"),
        "collision_response": _vector(snapshot, "response_velocity"),
        "motion_correction": _vector(snapshot, "correction"),
        "air_motion": _vector(snapshot, "air_motion"),
        "physics_state": _signed(physics_state, 32),
        "turn_accumulator": _signed(accumulator, 32),
        "ground_motion_threshold": ground_motion_threshold,
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
    values.extend((
        fields["physics_state"],
        0,
        0,
        fields["turn_accumulator"],
        fields["ground_motion_threshold"],
    ))
    values.extend(fields["orientation"])
    values.extend(fields["animation"])
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
    timing = frame.get("before")
    timing = timing.get("timing") if isinstance(timing, dict) else None
    scale_record = timing.get("animation_time_scale") \
        if isinstance(timing, dict) else None
    scale_raw = scale_record.get("raw") if isinstance(scale_record, dict) else None
    frame_scale_q8 = _signed(int(scale_raw), 32) if isinstance(scale_raw, int) else 0x100
    delta_record = timing.get("timing_delta_q11") \
        if isinstance(timing, dict) else None
    delta_raw = delta_record.get("raw") if isinstance(delta_record, dict) else None
    ground_surface_recovery_delta_q11 = (
        _signed(int(delta_raw), 32) if isinstance(delta_raw, int) else 0
    )
    raw = frame.get("raw")
    raw_after = raw.get("player_after") if isinstance(raw, dict) else None
    velocity_decay_divisor = 0
    if isinstance(raw_after, (bytes, bytearray)) and len(raw_after) >= 0x2C14:
        velocity_decay_divisor = struct.unpack_from("<i", raw_after, 0x2C10)[0]
    random_by_purpose: dict[str, int] = {}
    events = frame.get("events", [])
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

    # In-process captures made before the typed ollie probe was enabled still
    # retain the same causal results as generic shared-random events.  The PC
    # static callsites in FUN_0049a280 establish the order and purpose of
    # those results, so consume that existing evidence instead of replacing
    # it with zero-valued defaults.
    shared_ollie_callsite_purposes = {
        "0x0049a5bb": "charge_cap.first",
        "0x0049a5c5": "charge_cap.second",
        "0x0049a623": "charge_refresh.first",
        "0x0049a62d": "charge_refresh.second",
        "0x0049ab0b": "impulse.first",
        "0x0049ab15": "impulse.second",
        "0x0049ab38": "impulse.third",
        "0x0049ab69": "impulse.fourth",
        "0x0049abbb": "impulse.fifth",
        "0x0049ac1a": "early_release.first",
        "0x0049ac24": "early_release.second",
    }
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, dict) or event.get("type") != "shared_random_call":
                continue
            purpose = shared_ollie_callsite_purposes.get(event.get("caller"))
            raw_roll = event.get("return_value_s32")
            if purpose is None or not isinstance(raw_roll, int):
                continue
            # A typed probe and the generic service observer can coexist in
            # newer recordings.  Reject contradictory evidence while letting
            # the typed purpose retain precedence.
            value = _signed(raw_roll, 32)
            if purpose in random_by_purpose:
                if random_by_purpose[purpose] != value:
                    raise ValueError(
                        f"frame {frame_index} has conflicting ollie random "
                        f"purpose {purpose!r} values"
                    )
            else:
                random_by_purpose[purpose] = value

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
    generic_random_by_caller: dict[str, list[dict[str, Any]]] = {}
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, dict) or event.get("type") != "shared_random_call":
                continue
            caller = event.get("caller")
            result = event.get("return_value_s32")
            if isinstance(caller, str) and isinstance(result, int):
                generic_random_by_caller.setdefault(caller, []).append(event)

    def generic_random_event(caller: str, purpose: str) -> dict[str, Any] | None:
        candidates = generic_random_by_caller.get(caller, [])
        if not candidates:
            return None
        event = candidates[0]
        return {
            "type": "ground_motion_random_input",
            "purpose": purpose,
            "raw_roll": int(event["return_value_s32"]),
        }

    # Both connected orientation consumers use the same existing profile
    # service channel.  The ordinary state-1 branch returns through
    # 0x00498725, while FUN_00496360 returns through 0x004963e3 for state 2
    # (and for state 1 with +0x30c4 set).  Prefer the latter when present;
    # this keeps the already-recorded causal service result attached to the
    # correct retail call boundary instead of deriving it from snapshots.
    orientation_profile_events = generic_random_by_caller.get(
        "0x004963e3", []
    )
    if not orientation_profile_events:
        orientation_profile_events = generic_random_by_caller.get(
            "0x00498725", []
        )
    if len(orientation_profile_events) > 1:
        raise ValueError(
            f"frame {frame_index} has duplicate air orientation profile events"
        )
    orientation_profile_event = (
        orientation_profile_events[0]
        if orientation_profile_events
        else None
    )
    air_orientation_profile_available = int(
        orientation_profile_event is not None
    )
    air_orientation_profile_value = (
        _signed(int(orientation_profile_event["return_value_s32"]), 32)
        if orientation_profile_event is not None
        else 0
    )

    surface_response_events = [
        event
        for event in events
        if isinstance(event, dict)
        and event.get("type") == "shared_random_call"
        and event.get("caller") in {
            "0x0049c0ec",
            "0x0049c146",
            "0x0049c182",
        }
    ] if isinstance(events, list) else []
    surface_response_by_caller: dict[str, list[int]] = {}
    for event in surface_response_events:
        caller = event["caller"]
        raw_roll = event.get("return_value_s32")
        if not isinstance(raw_roll, int):
            raise TypeError(
                f"frame {frame_index} has an invalid ground surface response random event"
            )
        surface_response_by_caller.setdefault(caller, []).append(
            _signed(raw_roll, 32)
        )
    if any(
        len(values) > 1
        for caller, values in surface_response_by_caller.items()
        if caller != "0x0049c0ec"
    ):
        raise ValueError(
            f"frame {frame_index} has duplicate ground surface response random events"
        )
    cap_randoms = surface_response_by_caller.get("0x0049c0ec", [])
    target_randoms = surface_response_by_caller.get("0x0049c146", [])
    denominator_randoms = surface_response_by_caller.get("0x0049c182", [])
    if target_randoms and len(target_randoms) != 1:
        raise ValueError(
            f"frame {frame_index} has duplicate ground surface response target events"
        )
    if denominator_randoms and len(denominator_randoms) != 1:
        raise ValueError(
            f"frame {frame_index} has duplicate ground surface response denominator events"
        )
    if surface_response_events and (
        len(cap_randoms) < 1
        or len(target_randoms) != 1
        or len(denominator_randoms) != 1
    ):
        raise ValueError(
            f"frame {frame_index} has an incomplete ground surface response random set"
        )
    surface_response_available = int(bool(surface_response_events))
    surface_response_values = (
        surface_response_available,
        cap_randoms[0] if cap_randoms else 0,
        cap_randoms[1] if len(cap_randoms) > 1 else 0,
        target_randoms[0] if target_randoms else 0,
        denominator_randoms[0] if denominator_randoms else 0,
        int(len(cap_randoms) > 1),
    )
    normal_recovery_events = [
        event
        for event in events
        if isinstance(event, dict)
        and event.get("type") == "deterministic_random_call"
        and event.get("purpose") in {
            "normal_recovery_gate",
            "normal_recovery_x",
            "normal_recovery_z",
        }
    ] if isinstance(events, list) else []
    normal_recovery_by_purpose: dict[str, int] = {}
    for event in normal_recovery_events:
        purpose = event.get("purpose")
        raw_value = event.get("return_value_s32")
        if not isinstance(purpose, str) or not isinstance(raw_value, int):
            raise TypeError(
                f"frame {frame_index} has an invalid air normal-recovery random event"
            )
        if purpose in normal_recovery_by_purpose:
            raise ValueError(
                f"frame {frame_index} has duplicate air normal-recovery random purpose "
                f"{purpose!r}"
            )
        normal_recovery_by_purpose[purpose] = _signed(raw_value, 32)
    if normal_recovery_events and "normal_recovery_gate" not in normal_recovery_by_purpose:
        raise ValueError(
            f"frame {frame_index} has an air normal-recovery random set without its gate"
        )
    normal_recovery_gate = normal_recovery_by_purpose.get("normal_recovery_gate", 0)
    if normal_recovery_events and normal_recovery_gate == 0 and not all(
        purpose in normal_recovery_by_purpose
        for purpose in ("normal_recovery_x", "normal_recovery_z")
    ):
        raise ValueError(
            f"frame {frame_index} has an incomplete air normal-recovery random set"
        )
    normal_recovery_values = (
        int(bool(normal_recovery_events)),
        normal_recovery_gate,
        normal_recovery_by_purpose.get("normal_recovery_x", 0),
        normal_recovery_by_purpose.get("normal_recovery_z", 0),
    )
    state_two_events = [
        event
        for event in events
        if isinstance(event, dict)
        and event.get("type") == "ground_motion_random_input"
        and event.get("purpose") == "state_two_motion_seed"
    ] if isinstance(events, list) else []
    if not state_two_events:
        generic_state_two = generic_random_event(
            "0x0049e742", "state_two_motion_seed"
        )
        if generic_state_two is not None:
            state_two_events.append(generic_state_two)
    if len(state_two_events) > 1:
        raise ValueError(
            f"frame {frame_index} has duplicate state-two motion random events"
        )
    state_two_event = state_two_events[0] if state_two_events else None
    state_two_random = (
        _signed(int(state_two_event["raw_roll"]), 32)
        if isinstance(state_two_event, dict)
        and isinstance(state_two_event.get("raw_roll"), int)
        else 0
    )
    state_two_motion_values = (
        int(state_two_event is not None),
        state_two_random,
    )
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
    if metric_event is None:
        # These are the producer fields read by FUN_0049a280, not derived
        # output copied from the post-frame snapshot.  Capture V1 already
        # includes the complete +0x2d80 raw player window before physics.
        before_snapshot = frame.get("before")
        raw_words = (
            before_snapshot.get("raw_physics_words")
            if isinstance(before_snapshot, dict)
            else None
        )
        if isinstance(raw_words, list) and len(raw_words) > 228:
            slope_metric = _signed(int(raw_words[228]), 32)
            horizontal_speed_metric = _signed(int(raw_words[108]), 32)
            height_start = _signed(int(raw_words[114]), 32)
            height_end = _signed(int(raw_words[115]), 32)
            metrics = [
                slope_metric,
                horizontal_speed_metric,
                (height_start - height_end) >> 12,
            ]
    damping_by_purpose: dict[str, int] = {}
    damping_components: dict[str, int] = {}
    if isinstance(events, list):
        for event in events:
            if not isinstance(event, dict):
                continue
            event_type = event.get("type")
            if event_type == "velocity_damping_random_input":
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
            elif event_type == "velocity_damping_component_input":
                purpose = event.get("purpose")
                raw_value = event.get("raw_value")
                if not isinstance(purpose, str) or not isinstance(raw_value, int):
                    raise TypeError(
                        f"frame {frame_index} has an invalid velocity damping component event"
                    )
                if purpose in damping_components:
                    raise ValueError(
                        f"frame {frame_index} has duplicate velocity damping component purpose {purpose!r}"
                    )
                damping_components[purpose] = _signed(raw_value, 32)
            elif event_type == "shared_random_call":
                # The current recorder leaves shared RNG calls in their
                # generic form. These two return addresses are the verified
                # threshold draws inside FUN_0049d480.
                purpose_by_caller = {
                    "0x0049d4a1": "rescale_threshold",
                    "0x0049d5a0": "decay_threshold",
                }
                purpose = purpose_by_caller.get(event.get("caller"))
                raw_roll = event.get("return_value_s32")
                if purpose is None or not isinstance(raw_roll, int):
                    continue
                value = _signed(raw_roll, 32)
                if purpose in damping_by_purpose:
                    # ``service`` and ``rng`` forensic families can observe
                    # the same retail draw through different probes.  Keep
                    # the explicit purpose-specific value when both are
                    # present, but reject contradictory observations.
                    if damping_by_purpose[purpose] != value:
                        raise ValueError(
                            f"frame {frame_index} has conflicting velocity damping "
                            f"purpose {purpose!r} values"
                        )
                    continue
                damping_by_purpose[purpose] = value
    damping_values = [
        damping_by_purpose.get("rescale_threshold", 0),
        damping_components.get(
            "decay_x", damping_by_purpose.get("rescale_x", 0)
        ),
        damping_components.get(
            "decay_y", damping_by_purpose.get("rescale_y", 0)
        ),
        damping_components.get(
            "decay_z", damping_by_purpose.get("rescale_z", 0)
        ),
        damping_by_purpose.get("decay_threshold", 0),
    ]
    damping_component_available = int(
        all(purpose in damping_components for purpose in ("decay_x", "decay_y", "decay_z"))
    )
    if damping_components and not damping_component_available:
        raise ValueError(
            f"frame {frame_index} has an incomplete velocity damping component set"
        )
    damping_available = int(bool(damping_by_purpose or damping_components))
    # These fields remain in the wire layout for protocol compatibility, but
    # strict replay never restores captured derived state. The native
    # producers must create both values from causal inputs.
    motion_available = 0
    motion_values = [0, 0, 0]
    response_available = 0
    response_values = [0, 0, 0]
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
    threshold_events = [
        event
        for event in events
        if isinstance(event, dict)
        and event.get("type") == "ground_motion_random_input"
        and event.get("purpose") in {"threshold_seed_0xaa", "threshold_seed_0xdc"}
    ] if isinstance(events, list) else []
    if not threshold_events:
        for caller, purpose in (
            ("0x0049eae9", "threshold_seed_0xdc"),
            # The current shared-service observer records this call using
            # the return-site address on the special branch.  The static
            # call instruction remains 0049eae9; accept the existing alias
            # rather than adding another recorder channel.
            ("0x0049eaed", "threshold_seed_0xdc"),
            ("0x0049eb25", "threshold_seed_0xaa"),
        ):
            generic_threshold = generic_random_event(caller, purpose)
            if generic_threshold is not None:
                threshold_events.append(generic_threshold)
                break
    if len(threshold_events) > 1:
        raise ValueError(
            f"frame {frame_index} has duplicate ground-motion threshold events"
        )
    threshold_event = threshold_events[0] if threshold_events else None
    threshold_roll = (
        _signed(int(threshold_event["raw_roll"]), 32)
        if isinstance(threshold_event, dict)
        and isinstance(threshold_event.get("raw_roll"), int)
        else 0
    )
    # The post-dispatch threshold writer is observable in the raw player
    # object.  A rearm producer may also replace the threshold with a sampled
    # target during the ground-motion phase; that jump is already reproduced
    # by the causal rearm input and must not be followed by a second generic
    # threshold update.  Only expose the generic draw when the recording
    # proves the writer performed its one-unit decay.
    threshold_available = int(threshold_event is not None)
    if isinstance(raw, dict):
        raw_before = raw_physics_words
        raw_after = raw.get("player_after")
        if (isinstance(raw_before, list)
                and len(raw_before) > 18
                and isinstance(raw_after, (bytes, bytearray))
                and len(raw_after) >= 0x2dcc):
            before_threshold = _signed(int(raw_before[18]), 32)
            after_threshold = struct.unpack_from(
                "<i", raw_after, 0x2dc8)[0]
            if (isinstance(threshold_event, dict)
                    and threshold_event.get("purpose") == "threshold_seed_0xdc"):
                sampled_target = (
                    (_signed(int(threshold_event["raw_roll"]), 32) + 0xdc)
                    * 0x2d000
                    // 0x118
                )
                threshold_available = int(after_threshold == sampled_target)
            else:
                threshold_available = int(
                    threshold_event is not None
                    and after_threshold == before_threshold - 1
                )
    threshold_blocked = int(
        isinstance(threshold_event, dict)
        and threshold_event.get("purpose") == "threshold_seed_0xdc"
    )
    rearm_events = [
        event
        for event in events
        if isinstance(event, dict)
        and event.get("type") == "ground_motion_random_input"
        and event.get("purpose") in {"random_seed_0xaa", "random_seed_0xdc"}
    ] if isinstance(events, list) else []
    if not rearm_events:
        for caller, purpose in (
            ("0x0049b1bf", "random_seed_0xaa"),
            ("0x0049b411", "random_seed_0xdc"),
        ):
            generic_rearm = generic_random_event(caller, purpose)
            if generic_rearm is not None:
                rearm_events.append(generic_rearm)
                break
    if len(rearm_events) > 1:
        raise ValueError(
            f"frame {frame_index} has duplicate ground-motion rearm events"
        )
    rearm_event = rearm_events[0] if rearm_events else None
    if rearm_event is not None and not isinstance(rearm_event.get("raw_roll"), int):
        raise TypeError(
            f"frame {frame_index} has an invalid ground-motion rearm event"
        )
    rearm_values = (
        int(rearm_event is not None),
        _signed(int(rearm_event["raw_roll"]), 32) if rearm_event is not None else 0,
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
            damping_component_available,
            *damping_values,
            motion_available,
            *motion_values,
            response_available,
            *response_values,
            air_control_available,
            gravity_acceleration,
            air_control_enabled,
            threshold_available,
            threshold_roll,
            threshold_blocked,
            *rearm_values,
            ground_surface_recovery_delta_q11,
            *state_two_motion_values,
            *surface_response_values,
            *normal_recovery_values,
            velocity_decay_divisor,
            air_orientation_profile_available,
            air_orientation_profile_value,
        )
    )


def _wire_input(
    initial: dict[str, Any],
    frames: list[dict[str, Any]],
) -> str:
    lines = [f"version {NATIVE_REPLAY_WIRE_VERSION}", _initial_wire(initial)]
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
    try:
        recording = load_recording(path)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    # Preserve the detailed V1 diagnostics for legacy files.  Binary and
    # capture-backed recordings are already structurally checked by their
    # loader and can proceed directly through the same replay model.
    if recording.source_format == "legacy-jsonl":
        summary, errors = validate_recording(path)
        if errors:
            print(json.dumps({"summary": summary, "errors": errors}, indent=2, sort_keys=True))
            return 1

    header = recording.header
    initial_state = recording.initial_state
    frames = recording.frame_dicts(include_raw=True)
    if not initial_state:
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
            input=_wire_input(initial_state, frames),
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
            "mode": "strict",
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
    print("mode: strict")
    print("injected derived channels: none")
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
