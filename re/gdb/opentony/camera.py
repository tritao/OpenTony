"""Evidence-preserving runtime views for the recovered THPS2 camera object."""

from __future__ import annotations

import struct

from .breakpoint import Context, CountingBreakpoint, TonyBreakpoint
from .knowledge import GLOBALS, function_address

CAMERA_SIZE = 0x674
PLAYER_CAMERA_OFFSET = 0x29B0
PLAYER_SECONDARY_LINK_OFFSET = 0x29BC
VIEWPORT_STATE = 0x00563A38
VIEWPORT_SCALE_X = 0x00563A6C
VIEWPORT_SCALE_Y = 0x00563A70
PREPARED_VIEW_A = 0x005620C0
PREPARED_VIEW_B = 0x005620E8
VIEWPORT_POINTER = 0x005620E0
GEOMETRY_SCRATCH = 0x006A3E80
GEOMETRY_SUBMISSION = 0x004D11D0
GEOMETRY_RASTER_RETURN = 0x004D14C7
VERTEX_TRANSFORM = 0x004D29E0
VERTEX_TRANSFORM_RETURN = 0x004D2D9E
TRANSFORMED_VERTEX_SCRATCH = 0x00570878
# Coefficients and offsets consumed by the ordinary (non-special) packed
# vertex branch at 0x004d2b93. Keep them raw until the live source/output
# pairing proves the final screen/depth interpretation.
COMMON_VERTEX_LINEAR = 0x0056E84C
COMMON_VERTEX_BIAS = 0x0058F318
COMMON_VERTEX_PERSPECTIVE = 0x0057E878
COMMON_VERTEX_PERSPECTIVE_BASE = 0x005808A0
COMMON_VERTEX_OBJECT_BASIS = 0x00563A18
COMMON_VERTEX_RELATIVE_TRANSLATION = 0x00563AF8
VIEW_RECORD_GLOBAL = 0x005620E0
CAMERA_COLLISION_QUERY = 0x00466090
CAMERA_COLLISION_RESULT = 0x0040E790
VIEW_INPUT_VERTICAL_SCALE_OFFSET = 0x0C  # short word 6
CAMERA_POINT_TABLE = 0x0055FA58
CAMERA_POINT_COUNT = 0x0055FAE4
CAMERA_POINT_FLAGS = 0x00524CB8
CAMERA_POINT_STATE_MAIN = 0x0056E5D8
CAMERA_POINT_STATE_SECONDARY = 0x0056E450
CAMERA_POINT_SELECT_POSTCALL = 0x004CD7B6
# Camera_Update 0x0040f850's viewport/framing control inputs. These are
# deliberately recorded as raw globals: their producer ownership is not yet
# promoted to a single subsystem-level type.
VIEWPORT_PARAMETER_GLOBAL = 0x00524AA4
VIEWPORT_PARAMETER_X = 0x00524A40
VIEWPORT_PARAMETER_Y = 0x00524A44
VIEWPORT_PARAMETER_Z = 0x00524A48
VIEWPORT_PARAMETER_STATE = 0x0055FA30
VIEWPORT_PARAMETER_RESTORE = 0x0055FA48
VIEWPORT_PARAMETER_DECREMENT = 0x0056B008
VIEWPORT_PARAMETER_INCREMENT = 0x0056B018
VIEWPORT_PARAMETER_RESET = 0x0056AFF8
FRAMING_RESTORE_AXES = 0x0056B244
FRAMING_ROTATION_DECREMENT = 0x0056B174
FRAMING_ROTATION_INCREMENT = 0x0056B184
FRAMING_X_DECREMENT = 0x0056B1E4
FRAMING_X_INCREMENT = 0x0056B1F4
FRAMING_Y_DECREMENT = 0x0056B204
FRAMING_Y_INCREMENT = 0x0056B214
FRAMING_Z_DECREMENT = 0x0056B1A4
FRAMING_Z_INCREMENT = 0x0056B1B4
FRAMING_DIRECTION_INPUT = 0x0056B254
FRAMING_GLOBAL_X = 0x0055F9A4
FRAMING_GLOBAL_Y = 0x0055F910
FRAMING_GLOBAL_Z = 0x0055F978
# 0x00468b30 produces the Q8 rate consumed by Camera_Update on the following
# iteration. Keep the timing state raw so the producer/consumer delay can be
# checked against the present-clocked camera records.
SIMULATION_TICK = 0x0056E31C
SIMULATION_TIME = 0x0056E320
SIMULATION_DELTA_Q8 = 0x0056865C
TIMING_PREVIOUS_TIME = 0x00568604
TIMING_RING = 0x0056868C
TIMING_RING_INDEX = 0x0056A934
TIMING_DELTA_Q11 = 0x0056A93C
TIMING_PHASE_HALF = 0x0056A940
TIMING_PHASE_PARITY = 0x0056A944
TIMING_SLOW_RATE = 0x0056A948
TIMING_PROGRESS_Q8 = 0x00568810
TIMING_PROGRESS_INTEGER = 0x005685F4
TIMING_PAUSED = 0x00561C04
SIMULATION_PAUSED = 0x0056A8E0


def _steady_single_view_input(memory, address: int) -> bool:
    """Recognize the normalized 640x480 one-player view fixture."""

    if not memory.readable(address, 14 * 2):
        return False
    values = [memory.s16(address + index * 2) for index in range(14)]
    return values[:4] == [640, 480, 0, 0] and values[7:10] == [12, 320, 240]


def _words(memory, address: int, count: int = 3) -> dict[str, list[int | float]]:
    """Return raw words plus deliberately uncommitted numeric interpretations."""

    raw = list(struct.unpack(f"<{count}I", memory.bytes(address, count * 4)))
    signed = [value - (1 << 32) if value & 0x80000000 else value for value in raw]
    return {
        "raw": raw,
        "s32": signed,
        "fixed16": [value / 65536.0 for value in signed],
        "fixed16_candidate": [value / 65536.0 for value in signed],
        "q12": [value / 4096.0 for value in signed],
        "q12_candidate": [value / 4096.0 for value in signed],
        "f32_candidate": list(struct.unpack(f"<{count}f", struct.pack(f"<{count}I", *raw))),
    }


def _field_words(memory, camera: int, offset: int, count: int = 1):
    return _words(memory, camera + offset, count)


def _optional_words(memory, address: int, count: int = 1):
    if not memory.readable(address, count * 4):
        return None
    return _words(memory, address, count)


def _optional_u32(memory, address: int) -> int | None:
    return memory.u32(address) if memory.readable(address, 4) else None


def _optional_s32(memory, address: int) -> int | None:
    return memory.s32(address) if memory.readable(address, 4) else None


def _common_vertex_transform_record(memory) -> dict:
    """Capture raw state consumed by the common vertex-transform branch."""

    return {
        "linear_rows": [
            _optional_words(memory, COMMON_VERTEX_LINEAR + row * 0x0c, 3)
            for row in range(3)
        ],
        "bias": _optional_words(memory, COMMON_VERTEX_BIAS, 3),
        "perspective_factors": _optional_words(
            memory, COMMON_VERTEX_PERSPECTIVE, 3
        ),
        "perspective_constants": _optional_words(
            memory, COMMON_VERTEX_PERSPECTIVE_BASE, 3
        ),
        "perspective_depth_scale": _optional_words(memory, 0x005808A8),
        "perspective_depth_limit": _optional_words(memory, 0x0058089C),
        "state_flags": _optional_u32(memory, 0x0057E884),
        "source_x_scale": _optional_words(memory, 0x00549D60),
        "clip_threshold": _optional_words(memory, 0x00518484),
        "depth_threshold": _optional_words(memory, 0x0059D33C),
    }


def _common_vertex_producer_record(memory) -> dict:
    """Capture the raw inputs assembled by the common model submitter."""

    view_record = _optional_u32(memory, VIEW_RECORD_GLOBAL) or 0
    return {
        "object_basis_q12": _s16_array(memory, COMMON_VERTEX_OBJECT_BASIS, 9),
        "view_record": f"0x{view_record:08x}" if view_record else None,
        "view_basis_q12": _s16_array(memory, view_record + 0x74, 9)
        if view_record and memory.readable(view_record + 0x74, 0x12)
        else None,
        "relative_translation_s32": [
            _optional_s32(memory, COMMON_VERTEX_RELATIVE_TRANSLATION + index * 4)
            for index in range(3)
        ],
    }


def _wrap_s32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - (1 << 32) if value & 0x80000000 else value


def _raw_block(memory, address: int, size: int) -> dict | None:
    """Capture a pointed-to block without making its layout part of the probe API."""

    if not address or not memory.readable(address, size):
        return None
    return {"address": f"0x{address:08x}", "size": size, "raw": memory.bytes(address, size).hex()}


def _s16_array(memory, address: int, count: int) -> list[dict[str, int]] | None:
    if not address or not memory.readable(address, count * 2):
        return None
    return [_short(memory, address + index * 2) for index in range(count)]


def _angle_fields(memory, camera: int) -> dict:
    raw = _s16_array(memory, camera + 0x14, 3)
    if raw is None:
        return {"raw": None, "angle_units": None}
    return {
        "raw": raw,
        "angle_units": [field["raw_u16"] & 0x0FFF for field in raw],
        "turn_fraction": [(field["raw_u16"] & 0x0FFF) / 4096.0 for field in raw],
    }


def _short(memory, address: int) -> dict[str, int]:
    return {
        "raw_u16": memory.u16(address),
        "signed_s16": memory.s16(address),
    }


def camera_record(ctx: Context, camera: int) -> dict:
    """Capture camera fields without prematurely assigning their final types."""

    memory = ctx.memory
    player = memory.u32(GLOBALS["Player"])
    level = memory.u32(GLOBALS["CurrentLevel"]) if "CurrentLevel" in GLOBALS else None
    tripod = memory.u32(camera + 0x3A4)
    secondary_target = memory.u32(camera + 0x3DC)

    camera_fields = {
        "position": _field_words(memory, camera, 0x08, 3),
        "orientation_angles": _angle_fields(memory, camera),
        "anchor_target": _field_words(memory, camera, 0x3C0, 3),
        "secondary_anchor": _field_words(memory, camera, 0x3E4, 3),
        "look_target": _field_words(memory, camera, 0x3F0, 3),
        "current_vector": _field_words(memory, camera, 0x448, 4),
        "target_vector": _field_words(memory, camera, 0x45C, 4),
        "previous_vector": _field_words(memory, camera, 0x470, 4),
        "saved_vector": _field_words(memory, camera, 0x498, 4),
        "cached_render_vector": _field_words(memory, camera, 0x650, 4),
        "screen_effect_offset": _field_words(memory, camera, 0x63C, 3),
        "history_vector_a": _field_words(memory, camera, 0x5B8, 3),
        "history_vector_b": _field_words(memory, camera, 0x5C4, 3),
        "mode_vector": _field_words(memory, camera, 0x610, 3),
        "distance_q4": _field_words(memory, camera, 0x5D0),
        "distance_step_q4": _field_words(memory, camera, 0x61C),
        "distance_history": _field_words(memory, camera, 0x620, 6),
        "death_target_position": _field_words(memory, camera, 0x574, 3),
        "death_start_position": _field_words(memory, camera, 0x594, 3),
        "point_start_position": _field_words(memory, camera, 0x564, 3),
        # Camera_FollowTarget reads the tripod's +0x310c offset and the
        # camera's +0x5b4 signed angle before constructing the target basis.
        # Keep both raw so a replay can validate that producer independently
        # of the camera math.
        "follow_rotation_raw": _short(memory, camera + 0x5B4),
        "viewport_zoom_candidate": _field_words(memory, camera, 0x40C),
        "framing_globals_raw": {
            "x": _optional_s32(memory, VIEWPORT_PARAMETER_X),
            "y": _optional_s32(memory, VIEWPORT_PARAMETER_Y),
            "z": _optional_s32(memory, VIEWPORT_PARAMETER_Z),
        },
        "time_or_smoothing_a": _field_words(memory, camera, 0x410),
        "time_or_smoothing_b": _field_words(memory, camera, 0x414),
            "smoothing_counter_a": _field_words(memory, camera, 0x5D8),
            "smoothing_counter_b": _field_words(memory, camera, 0x5DC),
            "smoothing_counter_c": _field_words(memory, camera, 0x5E0),
            "smoothing_counter_d": _field_words(memory, camera, 0x5E4),
            "follow_effect_counter_raw": _field_words(memory, camera, 0x5E8),
            "alternate_follow_phase_a_raw": _short(memory, camera + 0x434),
            "alternate_follow_phase_b_raw": _short(memory, camera + 0x436),
            "alternate_follow_integrator_raw": _field_words(memory, camera, 0x5EC),
            "alternate_follow_counter_raw": _field_words(memory, camera, 0x5F0),
        }
    camera_fields.update(
        {
            "tripod_link": tripod,
            "secondary_target_link": secondary_target,
            "target_valid": memory.u32(camera + 0x3E0),
            "mode": memory.u32(camera + 0x504),
            "update_tick": memory.u32(camera + 0x510),
            "death_camera_tick": memory.u32(camera + 0x570),
            "point_camera_tick": memory.u32(camera + 0x55C),
            "point_acceleration_flag": memory.u8(camera + 0x560),
            "shake_x": _short(memory, camera + 0x4F2),
            "shake_y": _short(memory, camera + 0x4F4),
            "shake_z": _short(memory, camera + 0x4F6),
            "shake_decay_rate_x": memory.u8(camera + 0x4F8),
            "shake_decay_rate_y": memory.u8(camera + 0x4F9),
            "shake_decay_rate_z": memory.u8(camera + 0x4FA),
            "shake_angle_raw": memory.u32(camera + 0x4FC),
            "shake_phase_raw": memory.u16(camera + 0x500),
            "follow_state_flag": memory.u8(camera + 0x418),
            "follow_transition_active": memory.u8(camera + 0x5D4),
            "follow_preparation_counter": memory.u32(camera + 0x60C),
            "transform_valid": memory.u8(camera + 0x4A8),
            "transform_fallback": memory.u8(camera + 0x4A9),
        }
    )

    record = {
        "type": "camera",
        "frame": ctx.frame,
        "function": "Camera_Update",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "level": level,
        "player": f"0x{player:08x}",
        "camera": f"0x{camera:08x}",
        "camera_size": CAMERA_SIZE,
        "camera_vtable": f"0x{memory.u32(camera):08x}",
        "player_camera_offset": f"0x{PLAYER_CAMERA_OFFSET:04x}",
        "player_secondary_link": (
            f"0x{memory.u32(player + PLAYER_SECONDARY_LINK_OFFSET):08x}"
            if player and memory.valid(player)
            else None
        ),
        "tripod": f"0x{tripod:08x}",
        "secondary_target": f"0x{secondary_target:08x}",
        "camera_fields": camera_fields,
        "viewport_control_before": {
            "global_parameter_raw": _optional_s32(
                memory, VIEWPORT_PARAMETER_GLOBAL),
            "global_parameter_x_raw": _optional_s32(
                memory, VIEWPORT_PARAMETER_X),
            "global_parameter_y_raw": _optional_s32(
                memory, VIEWPORT_PARAMETER_Y),
            "global_parameter_z_raw": _optional_s32(
                memory, VIEWPORT_PARAMETER_Z),
            "parameter_state_raw": _optional_u32(
                memory, VIEWPORT_PARAMETER_STATE),
            "restore_pending_raw": _optional_u32(
                memory, VIEWPORT_PARAMETER_RESTORE),
            "decrement": _optional_u32(
                memory, VIEWPORT_PARAMETER_DECREMENT),
            "increment": _optional_u32(
                memory, VIEWPORT_PARAMETER_INCREMENT),
            "reset": _optional_u32(memory, VIEWPORT_PARAMETER_RESET),
            "global_override": _optional_s32(memory, VIEWPORT_PARAMETER_STATE),
            "restore_axes": _optional_u32(memory, FRAMING_RESTORE_AXES),
            "rotation_decrement": _optional_u32(
                memory, FRAMING_ROTATION_DECREMENT),
            "rotation_increment": _optional_u32(
                memory, FRAMING_ROTATION_INCREMENT),
            "x_decrement": _optional_u32(memory, FRAMING_X_DECREMENT),
            "x_increment": _optional_u32(memory, FRAMING_X_INCREMENT),
            "y_decrement": _optional_u32(memory, FRAMING_Y_DECREMENT),
            "y_increment": _optional_u32(memory, FRAMING_Y_INCREMENT),
            "z_decrement": _optional_u32(memory, FRAMING_Z_DECREMENT),
            "z_increment": _optional_u32(memory, FRAMING_Z_INCREMENT),
            "direction_input_raw": _optional_u32(
                memory, FRAMING_DIRECTION_INPUT),
            "restore_global_x": _optional_s32(memory, FRAMING_GLOBAL_X),
            "restore_global_y": _optional_s32(memory, FRAMING_GLOBAL_Y),
            "restore_global_z": _optional_s32(memory, FRAMING_GLOBAL_Z),
        },
        "timing_before": _timing_record(memory),
    }
    if player and memory.valid(player):
        record["player_position"] = _words(memory, player + 0x08, 3)
        record["player_physics_state"] = memory.u32(player + 0x30B8)
        record["player_unknown_state"] = memory.u32(player + 0x30C4)
    else:
        record["player_position"] = None
        record["player_physics_state"] = None
        record["player_unknown_state"] = None
    if tripod and memory.valid(tripod):
        record["tripod_position"] = _words(memory, tripod + 0x08, 3)
        record["tripod_distance_sample_offset"] = _words(
            memory, tripod + 0x4C, 3)
        record["tripod_follow_offset"] = _words(memory, tripod + 0x310C, 3)
        record["tripod_physics_state"] = memory.u32(tripod + 0x30B8)
        record["tripod_behavior_flag"] = memory.u32(tripod + 0x2F64)
        record["tripod_effect_gate"] = memory.u32(tripod + 0x2DDC)
        record["tripod_effect_transform_gate"] = memory.u32(tripod + 0x2C68)
        record["tripod_unknown_state"] = memory.u32(tripod + 0x30C4)
    else:
        record["tripod_position"] = None
        record["tripod_distance_sample_offset"] = None
        record["tripod_follow_offset"] = None
        record["tripod_physics_state"] = None
        record["tripod_behavior_flag"] = None
        record["tripod_effect_gate"] = None
        record["tripod_effect_transform_gate"] = None
        record["tripod_unknown_state"] = None
    return record


def _timing_record(memory) -> dict:
    """Capture the raw clock/rate state surrounding Camera_Update."""

    return {
        "simulation_tick": _optional_s32(memory, SIMULATION_TICK),
        "simulation_time": _optional_s32(memory, SIMULATION_TIME),
        "simulation_delta_q8": _optional_s32(memory, SIMULATION_DELTA_Q8),
        "previous_simulation_time": _optional_s32(memory, TIMING_PREVIOUS_TIME),
        "recent_deltas": [
            _optional_s32(memory, TIMING_RING + index * 4)
            for index in range(3)
        ],
        "ring_index": _optional_u32(memory, TIMING_RING_INDEX),
        "delta_q11": _optional_s32(memory, TIMING_DELTA_Q11),
        "phase_half": _optional_s32(memory, TIMING_PHASE_HALF),
        "phase_parity": _optional_u32(memory, TIMING_PHASE_PARITY),
        "slow_rate": _optional_u32(memory, TIMING_SLOW_RATE),
        "progress_q8": _optional_s32(memory, TIMING_PROGRESS_Q8),
        "progress_integer": _optional_s32(memory, TIMING_PROGRESS_INTEGER),
        "timing_paused": _optional_u32(memory, TIMING_PAUSED),
        "simulation_paused": _optional_u32(memory, SIMULATION_PAUSED),
    }


def camera_timing_record(ctx: Context) -> dict:
    """Capture the post-0x00468b30 camera-rate producer state."""

    return {
        "type": "camera_timing",
        "frame": ctx.frame,
        "function": "Camera_TimingProducer",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "timing": _timing_record(ctx.memory),
    }


def camera_effect_record(ctx: Context) -> dict:
    """Capture the raw state consumed by Camera_ApplyEffects 0x0040c370."""

    memory = ctx.memory
    camera = ctx.this_ptr()
    tripod = memory.u32(camera + 0x3A4)
    effect_offsets = (0x4D4, 0x4DC, 0x4E0, 0x4E8, 0x530, 0x532,
                      0x534, 0x536, 0x538, 0x540, 0x542, 0x544,
                      0x546, 0x548, 0x550, 0x554, 0x558, 0x5D8,
                      0x5DC, 0x5E0, 0x5E4, 0x63C, 0x670)
    return {
        "type": "camera_effects",
        "frame": ctx.frame,
        "function": "Camera_ApplyEffects",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "camera": f"0x{camera:08x}",
        "mode": memory.u32(camera + 0x504),
        "update_tick": memory.u32(camera + 0x510),
        "follow_state_flag": memory.u8(camera + 0x418),
        "follow_transition_active": memory.u8(camera + 0x5D4),
        "tripod": f"0x{tripod:08x}" if tripod else None,
        "tripod_physics_state": memory.u32(tripod + 0x30B8)
        if tripod and memory.valid(tripod)
        else None,
        "tripod_effect_gate": memory.u32(tripod + 0x2DDC)
        if tripod and memory.valid(tripod)
        else None,
        "tripod_effect_transform_gate": memory.u32(tripod + 0x2C68)
        if tripod and memory.valid(tripod)
        else None,
        "global_guards": {
            "effects_disabled": _optional_u32(memory, 0x00561C04),
            "camera_effects_disabled": _optional_u32(memory, 0x0056A8E0),
            "render_effects_disabled": _optional_u32(memory, 0x0056A86C),
            "global_override": _optional_u32(memory, 0x0055FA30),
        },
        "vertical_effect_q16": _optional_s32(memory, 0x0055F94C),
        "raw_fields": {
            f"0x{offset:03x}": _field_words(memory, camera, offset)
            for offset in effect_offsets
        },
    }


def view_projection_record(ctx: Context) -> dict:
    """Capture the view/projection handoff as raw records and fixed-point hints."""

    memory = ctx.memory
    viewport = ctx.arg(0)
    view_input = ctx.arg(1)
    render_state = ctx.arg(2)
    player = memory.u32(GLOBALS["Player"])
    camera = memory.u32(player + PLAYER_CAMERA_OFFSET) if player and memory.valid(player) else 0

    record = {
        "type": "view_projection",
        "frame": ctx.frame,
        "function": "Render_SetViewProjection",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "arguments": {
            "viewport": f"0x{viewport:08x}",
            "view_input": f"0x{view_input:08x}",
            "render_state": f"0x{render_state:08x}",
        },
        "viewport_block": _raw_block(memory, viewport, 0x70),
        "view_input_block": _raw_block(memory, view_input, 0x20),
        "render_state_block": _raw_block(memory, render_state, 0x20),
        "viewport_words": _words(memory, viewport, 8) if memory.readable(viewport, 0x20) else None,
        "view_input_shorts": _s16_array(memory, view_input, 14),
        "viewport_matrix_a": _s16_array(memory, viewport + 0x34, 9)
        if memory.readable(viewport + 0x34, 0x12)
        else None,
        "viewport_matrix_b": _s16_array(memory, viewport + 0x54, 9)
        if memory.readable(viewport + 0x54, 0x12)
        else None,
        "camera": f"0x{camera:08x}" if camera else None,
        "camera_angles": _angle_fields(memory, camera) if camera and memory.valid(camera) else None,
        "camera_look_target": _field_words(memory, camera, 0x3F0, 3)
        if camera and memory.valid(camera)
        else None,
        "camera_viewport_raw": memory.u32(camera + 0x40C)
        if camera and memory.valid(camera)
        else None,
        "viewport_state_pointer": f"0x{memory.u32(VIEWPORT_STATE):08x}"
        if memory.readable(VIEWPORT_STATE, 4)
        else None,
        "viewport_scale_x_raw": memory.u32(VIEWPORT_SCALE_X)
        if memory.readable(VIEWPORT_SCALE_X, 4)
        else None,
        "viewport_scale_y_raw": memory.u32(VIEWPORT_SCALE_Y)
        if memory.readable(VIEWPORT_SCALE_Y, 4)
        else None,
        "common_vertex_transform": _common_vertex_transform_record(memory),
    }
    return record


def actor_submission_record(ctx: Context) -> dict:
    """Capture the game-owned object pointer entering actor submission.

    The callee receives its render-object argument on the stack. The prefix is
    intentionally raw: fields at +0x04, +0x1a, +0x1f, +0x24, and +0x30 are
    consumed by the original submission routine, but their durable C++ type
    and ownership are not yet proven.
    """

    memory = ctx.memory
    actor = ctx.arg(0)
    record = {
        "type": "actor_submission",
        "frame": ctx.frame,
        "function": "Render_SubmitActor",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "actor": f"0x{actor:08x}" if actor else None,
        "actor_prefix": _raw_block(memory, actor, 0x40),
    }
    if actor and memory.readable(actor, 0x40):
        record["raw_fields"] = {
            "flags_u16_at_0x04": memory.u16(actor + 0x04),
            "material_or_index_u16_at_0x1a": memory.u16(actor + 0x1A),
            "type_u8_at_0x1f": memory.u8(actor + 0x1F),
            "resource_u32_at_0x24": memory.u32(actor + 0x24),
            "aux_u32_at_0x30": memory.u32(actor + 0x30),
        }
    else:
        record["raw_fields"] = None
    return record


def geometry_submission_record(ctx: Context) -> dict:
    """Capture the raw game-owned geometry handoff after view preparation.

    ``0x004d11d0`` is a callsite-level boundary in the current static
    recovery, not a promoted renderer function.  Keep the argument and
    prepared transform records raw so the probe does not assign ownership to
    the downstream geometry/backend structures prematurely.
    """

    memory = ctx.memory
    geometry = ctx.arg(0)
    player = memory.u32(GLOBALS["Player"])
    camera = (
        memory.u32(player + PLAYER_CAMERA_OFFSET)
        if player and memory.valid(player)
        else 0
    )
    viewport_pointer = memory.u32(VIEWPORT_POINTER) if memory.readable(VIEWPORT_POINTER, 4) else 0
    return {
        "type": "geometry_submission",
        "frame": ctx.frame,
        "function": "Render_GeometrySubmissionCallsite",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "level": (
            memory.u32(GLOBALS["CurrentLevel"])
            if "CurrentLevel" in GLOBALS and memory.readable(GLOBALS["CurrentLevel"], 4)
            else None
        ),
        "player": f"0x{player:08x}" if player else None,
        "arguments": {
            "geometry": f"0x{geometry:08x}" if geometry else None,
            "vertex_count_or_index": ctx.arg(1),
            "arg2": ctx.arg(2),
            "arg3": ctx.arg(3),
        },
        "geometry_prefix": _raw_block(memory, geometry, 0x40),
        "prepared_view_a_s16": _s16_array(memory, PREPARED_VIEW_A, 15),
        "prepared_view_b_s16": _s16_array(memory, PREPARED_VIEW_B, 15),
        "geometry_scratch": _raw_block(memory, GEOMETRY_SCRATCH, 0x60),
        "viewport_pointer": f"0x{viewport_pointer:08x}" if viewport_pointer else None,
        "viewport_block": _raw_block(memory, viewport_pointer, 0x90),
        "camera": f"0x{camera:08x}" if camera else None,
        "camera_viewport_raw": (
            memory.u32(camera + 0x40C)
            if camera and memory.readable(camera + 0x40C, 4)
            else None
        ),
    }


def geometry_raster_return_record(ctx: Context) -> dict | None:
    """Capture the fixed-point geometry routine's post-transform scratch.

    The function at 0x004d11d0 converts the prepared integer geometry into
    the raster-side byte records before returning at 0x004d14c7.  These are
    raw records only: the camera contract should not name a screen/depth
    convention until the per-vertex fields are correlated with clipping.
    """

    if ctx.eip != GEOMETRY_RASTER_RETURN:
        return None
    memory = ctx.memory
    return {
        "type": "geometry_raster_return",
        "frame": ctx.frame,
        "function": "Render_GeometryRasterTail",
        "eip": f"0x{ctx.eip:08x}",
        # 0x004d11d0 reads these globals as the transformed translation,
        # prepared matrix, and fallback matrix/vertex scratch respectively.
        "transform_scratch": _raw_block(memory, 0x006A3E48, 0x80),
        "prepared_matrix_s16": _s16_array(memory, 0x006A3EC8, 9),
        "geometry_matrix_s16": _s16_array(memory, GEOMETRY_SCRATCH, 12),
        # The routine advances 4 bytes per submitted vertex at this global.
        "raster_vertex_scratch": _raw_block(memory, 0x0057E888, 0x100),
    }


def transformed_vertex_record(
    ctx: Context,
    call_arguments: dict[str, int] | None = None,
) -> dict | None:
    """Capture ordinary model-path projected vertices at the return tail."""

    memory = ctx.memory
    player = _optional_u32(memory, GLOBALS["Player"]) or 0
    level = _optional_u32(memory, GLOBALS["CurrentLevel"])
    call_arguments = call_arguments or {
        "input_vertices": ctx.arg(0),
        "vertex_count_raw": ctx.arg(1),
        "state": ctx.arg(2),
    }
    input_vertices = call_arguments["input_vertices"]
    raw_vertex_count = call_arguments["vertex_count_raw"]
    vertex_count = min(raw_vertex_count & 0xffff, 256)
    record_size = 7 * 4
    scratch_size = vertex_count * record_size

    record = {
        "type": "transformed_vertices",
        "frame": ctx.frame,
        "function": "Render_TransformVertices",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "transform_entry": f"0x{VERTEX_TRANSFORM:08x}",
        "boundary": f"0x{VERTEX_TRANSFORM_RETURN:08x}",
        "level": level,
        "player": f"0x{player:08x}" if player else None,
        "arguments": {
            "input_vertices": f"0x{input_vertices:08x}" if input_vertices else None,
            "vertex_count_raw": raw_vertex_count,
            "vertex_count": vertex_count,
            "state": call_arguments["state"],
        },
        "record_stride": record_size,
        "scratch_address": f"0x{TRANSFORMED_VERTEX_SCRATCH:08x}",
    }

    if not player or not memory.valid(player):
        record.update({"accepted": False, "rejection": "player_unreadable", "vertices": []})
        return record
    camera = memory.u32(player + PLAYER_CAMERA_OFFSET)
    if not camera or not memory.valid(camera):
        record.update({"accepted": False, "rejection": "camera_unreadable", "vertices": []})
        return record
    record["camera"] = f"0x{camera:08x}"
    if not vertex_count:
        record.update({"accepted": False, "rejection": "zero_vertex_count", "vertices": []})
        return record
    if not memory.readable(TRANSFORMED_VERTEX_SCRATCH, scratch_size):
        record.update({"accepted": False, "rejection": "scratch_unreadable", "vertices": []})
        return record

    words = [
        list(struct.unpack(
            "<7I",
            memory.bytes(TRANSFORMED_VERTEX_SCRATCH + index * record_size, record_size),
        ))
        for index in range(vertex_count)
    ]
    vertices = []
    for values in words:
        x, y, z, depth = struct.unpack(
            "<4f", struct.pack("<4I", *values[:4]))
        vertices.append({
            "raw_words": values,
            "projected_x": x,
            "projected_y": y,
            "projected_z": z,
            "reciprocal_depth": depth,
            "source_flags_bits": values[4],
            "clip_flags_bits": values[5],
            "auxiliary_bits": values[6],
        })

    record.update({
        "accepted": True,
        # 0x004d29e0 advances the source geometry by eight bytes per vertex:
        # three signed shorts followed by the packed vertex flags. Keeping
        # this input beside the completed output makes projection calibration
        # reproducible without guessing the model-space scale.
        "input_vertex_stride": 8,
        "input_vertices_raw": _raw_block(
            memory, input_vertices, vertex_count * 8
        ) if input_vertices and memory.readable(input_vertices, vertex_count * 8)
        else None,
        "common_vertex_producer": _common_vertex_producer_record(memory),
        "common_vertex_transform": _common_vertex_transform_record(memory),
        "scratch": _raw_block(memory, TRANSFORMED_VERTEX_SCRATCH, scratch_size),
        "vertices": vertices,
    })
    return record


def camera_point_select_record(ctx: Context) -> dict | None:
    """Capture the gameplay-owned point-selection producer at 0x00411fc0.

    The selector is called with a skater/object pointer in ECX. It constructs
    a candidate from the object's world position and +0x310c camera offset,
    searches the registered point IDs, and may rewrite the linked camera's
    mode/target fields. Only the producer inputs and pre-call camera state are
    recorded here; the selected point is committed later in the same call.
    """

    memory = ctx.memory
    player = ctx.this_ptr()
    if not player or not memory.valid(player):
        return None
    camera = memory.u32(player + PLAYER_CAMERA_OFFSET)
    if not camera or not memory.valid(camera):
        return None

    position = _words(memory, player + 0x08, 3)
    offset = _words(memory, player + 0x310C, 3)
    candidate_raw = [
        _wrap_s32(
            position["raw"][index]
            + (_wrap_s32(offset["raw"][index]) * 0x6E)
        )
        for index in range(3)
    ]

    point_count = _optional_u32(memory, CAMERA_POINT_COUNT)
    point_count = max(0, min(point_count or 0, 0x46))
    record = {
        "type": "camera_point_select",
        "frame": ctx.frame,
        "function": "Camera_PointSelect",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "level": (
            _optional_u32(memory, GLOBALS["CurrentLevel"])
            if "CurrentLevel" in GLOBALS
            else None
        ),
        "player": f"0x{player:08x}",
        "camera": f"0x{camera:08x}",
        "player_position": position,
        "player_camera_offset": offset,
        "candidate_position_raw": candidate_raw,
        "registered_point_count": point_count,
        "registered_point_ids": (
            _s16_array(memory, CAMERA_POINT_TABLE, point_count)
            if point_count and memory.readable(CAMERA_POINT_TABLE, point_count * 2)
            else []
        ),
        "point_state": {
            "main": _raw_block(memory, CAMERA_POINT_STATE_MAIN, 0x50),
            "secondary": _raw_block(memory, CAMERA_POINT_STATE_SECONDARY, 0x50),
        },
        "camera_before": {
            "mode": _optional_u32(memory, camera + 0x504),
            "target_valid": _optional_u32(memory, camera + 0x3E0),
            "primary_link": _optional_u32(memory, camera + 0x3A4),
            "secondary_link": _optional_u32(memory, camera + 0x3DC),
            "anchor_flag": memory.u8(camera + 0x3AC)
            if memory.readable(camera + 0x3AC, 1)
            else None,
            "target_initialized": memory.u8(camera + 0x3BC)
            if memory.readable(camera + 0x3BC, 1)
            else None,
            "anchor_target": _field_words(memory, camera, 0x3C0, 3),
        },
    }
    return record


def camera_point_state_record(ctx: Context) -> dict | None:
    """Capture the post-call state at 0x004cd7b6.

    The selector has several early returns and its render-visible viewport
    writes happen after the point/mode handoff. The caller's instruction after
    the call is therefore the stable boundary for observing the selected
    registry flags, action variant, camera +0x5b4, and viewport word 6.
    """

    memory = ctx.memory
    player = ctx.register("esi")
    if not player or not memory.valid(player):
        return None
    camera = memory.u32(player + PLAYER_CAMERA_OFFSET)
    if not camera or not memory.valid(camera):
        return None
    main_player = (
        _optional_u32(memory, GLOBALS["Player"])
        if "Player" in GLOBALS
        else None
    )
    state_address = (
        CAMERA_POINT_STATE_MAIN
        if player == main_player
        else CAMERA_POINT_STATE_SECONDARY
    )
    if not memory.readable(state_address, 0x50):
        return None
    current_index = memory.s32(state_address + 0x28)
    if current_index < 0 or current_index >= 0x46:
        return None
    flags_address = CAMERA_POINT_FLAGS + current_index * 4
    if not memory.readable(flags_address, 4):
        return None
    active_viewport = _optional_u32(memory, VIEWPORT_STATE)
    return {
        "type": "camera_point_state",
        "frame": ctx.frame,
        "function": "Camera_PointSelect_PostCall",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{ctx.caller():08x}",
        "player": f"0x{player:08x}",
        "camera": f"0x{camera:08x}",
        "state": f"0x{state_address:08x}",
        "current_point_index": current_index,
        "previous_point_index": memory.s32(state_address + 0x40),
        "action_variant_raw": memory.s32(state_address + 0x4c),
        "selected_point": _words(memory, state_address + 0x2c, 3),
        "registry_flags": memory.u32(flags_address),
        "camera_after": {
            "mode": memory.u32(camera + 0x504),
            "target_valid": memory.u8(camera + 0x3e0),
            "primary_link": memory.u32(camera + 0x3a4),
            "secondary_link": memory.u32(camera + 0x3dc),
            "anchor_update_flag": memory.u8(camera + 0x3ac),
            "tripod_anchor_flag": memory.u8(camera + 0x3bc),
            "follow_rotation_raw": memory.s16(camera + 0x5b4),
            "anchor_target": _field_words(memory, camera, 0x3c0, 3),
        },
        "viewport_after": {
            "address": f"0x{active_viewport:08x}" if active_viewport else None,
            "word6_raw": (
                memory.u16(active_viewport + 0x0c)
                if active_viewport and memory.readable(active_viewport + 0x0c, 2)
                else None
            ),
        },
        "framing_globals": {
            "x": _optional_s32(memory, VIEWPORT_PARAMETER_X),
            "y": _optional_s32(memory, VIEWPORT_PARAMETER_Y),
            "z": _optional_s32(memory, VIEWPORT_PARAMETER_Z),
        },
    }


def camera_position_transform_record(ctx: Context) -> dict | None:
    """Capture one exact 0x004e85a0 camera-tail input triplet."""

    caller = ctx.caller()
    if caller not in {0x0040ECB8, 0x0040ECEE}:
        return None
    matrix = ctx.arg(0)
    vector = ctx.arg(1)
    output = ctx.arg(2)
    camera = ctx.register("ebp")
    return {
        "type": "camera_position_transform",
        "frame": ctx.frame,
        "function": "Fixed_MatrixConvertQ12",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{caller:08x}",
        "camera": f"0x{camera:08x}" if ctx.memory.valid(camera) else None,
        "arguments": {
            "matrix": f"0x{matrix:08x}",
            "vector": f"0x{vector:08x}",
            "output": f"0x{output:08x}",
        },
        "matrix_s16": _s16_array(ctx.memory, matrix, 9),
        "vector_q16": _words(ctx.memory, vector, 3)
        if ctx.memory.readable(vector, 0x0C)
        else None,
        "output_before": _words(ctx.memory, output, 3)
        if ctx.memory.readable(output, 0x0C)
        else None,
        "camera_position_producer": {
            "distance_q4": ctx.memory.s32(camera + 0x5D0)
            if ctx.memory.readable(camera + 0x5D0, 4)
            else None,
            "transition_counter": ctx.memory.u32(camera + 0x60C)
            if ctx.memory.readable(camera + 0x60C, 4)
            else None,
            "effect_counter_a": ctx.memory.u32(camera + 0x5D8)
            if ctx.memory.readable(camera + 0x5D8, 4)
            else None,
            "effect_counter_b": ctx.memory.u32(camera + 0x5E0)
            if ctx.memory.readable(camera + 0x5E0, 4)
            else None,
            "effect_global_vertical_q4": ctx.memory.s32(0x00524A48)
            if ctx.memory.readable(0x00524A48, 4)
            else None,
            "effect_vector_global_q16": _words(ctx.memory, 0x0055F948, 3)
            if ctx.memory.readable(0x0055F948, 0x0C)
            else None,
        },
    }


def camera_collision_record(ctx: Context) -> dict | None:
    """Capture the world-query boundary used by Camera_SmoothAndValidate."""

    caller = ctx.caller()
    # The normal camera collision path returns to 0x0040e790. Keep this
    # filter broad enough for nearby compiler/layout variants while avoiding
    # unrelated world queries at the same shared helper.
    if not 0x0040E000 <= caller < 0x0040ED53:
        return None
    memory = ctx.memory
    camera = ctx.register("ebp")
    if not camera or not memory.valid(camera):
        return None
    query = ctx.arg(0)
    return {
        "type": "camera_collision_query",
        "frame": ctx.frame,
        "function": "Camera_WorldCollisionQuery",
        "eip": f"0x{ctx.eip:08x}",
        "caller": f"0x{caller:08x}",
        "camera": f"0x{camera:08x}",
        "arguments": {
            "query": f"0x{query:08x}",
            "query_flag": ctx.arg(1),
        },
        # The pointed-to layout is still provisional. Preserve enough raw
        # bytes to correlate the query with the camera's endpoint/result
        # writes without naming world-collision fields prematurely.
        "query_block": _raw_block(memory, query, 0x30),
        "camera_before": {
            "anchor_target": _field_words(memory, camera, 0x3C0, 3),
            "position": _field_words(memory, camera, 0x08, 3),
            "distance_q4": _optional_s32(memory, camera + 0x5D0),
            "distance_step_q4": _optional_s32(memory, camera + 0x61C),
            "transition_counter": _optional_u32(memory, camera + 0x60C),
            "effect_counter_a": _optional_s32(memory, camera + 0x5D8),
            "effect_counter_b": _optional_s32(memory, camera + 0x5E0),
            "target_transform": _field_words(memory, camera, 0x45C, 4),
        },
        "world_collision_disabled": _optional_u32(memory, 0x0056A86C),
    }


def camera_collision_result_record(ctx: Context) -> dict | None:
    """Capture the caller-side result immediately after the world query."""

    if ctx.eip != CAMERA_COLLISION_RESULT:
        return None
    memory = ctx.memory
    camera = ctx.register("ebp")
    if not camera or not memory.valid(camera):
        return None
    stack = ctx.esp
    # The caller's two argument pushes remain on the stack at 0x0040e790.
    # The query pointer therefore resolves to stack+0xf8. The query record's
    # result field is word 0x1a, i.e. query+0x68; 0x00462a20 writes the hit
    # face pointer there and 0x004624d0 initializes it to zero.
    query = stack + 0xF8
    return {
        "type": "camera_collision_result",
        "frame": ctx.frame,
        "function": "Camera_WorldCollisionQuery_Result",
        "eip": f"0x{ctx.eip:08x}",
        # At this instruction the call has already returned, so the top of
        # the stack is the first pushed argument, not a return address.
        # The static callsite is 0x0040e78b and the resume point is 0x0040e790.
        "caller": "0x0040e78b",
        "resume_eip": f"0x{ctx.eip:08x}",
        "camera": f"0x{camera:08x}",
        "stack": f"0x{stack:08x}",
        "query": f"0x{query:08x}",
        "query_result_field_offset": 0x68,
        # This is the exact caller-side test at 0x0040e790. Static dataflow
        # identifies it as the query's hit-face pointer; retain both names so
        # traces remain useful if a later build changes the result payload.
        "collision_result_raw": _optional_u32(memory, query + 0x68),
        "hit_face_raw": _optional_u32(memory, query + 0x68),
        "candidate_segment": {
            "start": _words(memory, query, 3)
            if memory.readable(query, 0x0C)
            else None,
            "end": _words(memory, query + 0x0C, 3)
            if memory.readable(query + 0x0C, 0x0C)
            else None,
        },
        "query_block_after": _raw_block(memory, query, 0x8C),
        "target_transform_after": _field_words(memory, camera, 0x45C, 4),
        "camera_position": _field_words(memory, camera, 0x08, 3),
        "transition_counter": _optional_u32(memory, camera + 0x60C),
        "world_collision_disabled": _optional_u32(memory, 0x0056A86C),
    }


class CameraProbe(CountingBreakpoint):
    """Sample the actual camera-update entry, before mode-specific work runs."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("camera_update"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        camera = ctx.this_ptr()
        if not ctx.memory.valid(camera):
            return False
        record = camera_record(ctx, camera)
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        # Avoid importing commands at module load time (commands imports this module).
        import gdb

        gdb.write(f"camera probe complete: {self.hits} observations\n")


class CameraModeOverrideProbe(CountingBreakpoint):
    """Force one camera-update entry through a selected raw mode."""

    def __init__(self, mode: int, hold_updates: int = 1, writer=None):
        # The first accepted update gets the requested mode; the following
        # accepted update restores normal follow so the probe cannot leave the
        # retail camera permanently altered.  The mode field is camera+0x504.
        super().__init__(
            function_address("camera_update"),
            count=hold_updates + 1,
            internal=True,
        )
        self.mode = mode
        self.hold_updates = hold_updates
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        camera = ctx.this_ptr()
        if not ctx.memory.valid(camera):
            return False
        original_mode = ctx.memory.u32(camera + 0x504)
        written_mode = self.mode if self.hits < self.hold_updates else 1
        ctx.memory.write_u32(camera + 0x504, written_mode)
        record = camera_record(ctx, camera)
        record["camera_mode_override"] = {
            "original_mode": original_mode,
            "written_mode": written_mode,
            "requested_mode": self.mode,
            "hold_updates": self.hold_updates,
            "sequence_index": self.hits,
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(
            f"camera mode override complete: mode {self.mode}, "
            f"hold {self.hold_updates}, observations {self.hits}\n"
        )


class CameraTimingProbe(CountingBreakpoint):
    """Sample the timing/rate producer after it computes the next rate."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(0x00468B30, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = camera_timing_record(ctx)
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera timing probe complete: {self.hits} observations\n")


class CameraPointSelectProbe(CountingBreakpoint):
    """Sample the player-owned camera-point selection producer."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("camera_point_select"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = camera_point_select_record(ctx)
        if record is None:
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera point-select probe complete: {self.hits} observations\n")


class CameraPointStateProbe(CountingBreakpoint):
    """Sample the render-visible point state after the selector returns."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(CAMERA_POINT_SELECT_POSTCALL, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = camera_point_state_record(ctx)
        if record is None:
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera point-state probe complete: {self.hits} observations\n")


class CameraEffectProbe(CountingBreakpoint):
    """Sample the gameplay/effect producer boundary inside the camera path."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("camera_effects"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        camera = ctx.this_ptr()
        if not ctx.memory.valid(camera):
            return False
        record = camera_effect_record(ctx)
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera effects probe complete: {self.hits} observations\n")


class ViewProjectionProbe(CountingBreakpoint):
    """Sample the game-owned view/projection preparation entry."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("view_projection"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = view_projection_record(ctx)
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"view projection probe complete: {self.hits} observations\n")


class ViewProjectionPerturbProbe(CountingBreakpoint):
    """Alternate the raw vertical-scale input at view setup entry.

    This is a controlled calibration probe, not a gameplay feature. It waits
    for a live level player/camera and a positive word-6 value, then alternates
    the live record between its observed baseline and half that value.
    Menu/front-end view calls are deliberately ignored: they can satisfy the
    positive-value test but have no gameplay geometry to validate downstream.
    Downstream view/geometry probes can therefore test whether the field
    changes the prepared basis and packets.
    """

    def __init__(
        self,
        count: int | None = None,
        writer=None,
        *,
        freeze_input: bool = False,
    ):
        super().__init__(function_address("view_projection"), count=count, internal=True)
        self.writer = writer
        self.freeze_input = freeze_input
        self.baseline: int | None = None
        self.baseline_block: bytes | None = None

    def on_count(self, ctx: Context) -> bool:
        memory = ctx.memory
        player = memory.u32(GLOBALS["Player"])
        level = memory.u32(GLOBALS["CurrentLevel"])
        camera = (
            memory.u32(player + PLAYER_CAMERA_OFFSET)
            if player and memory.valid(player)
            else 0
        )
        if level > 12 or not player or not memory.valid(player):
            return False
        if not camera or not memory.valid(camera):
            return False
        view_input = ctx.arg(1)
        if self.freeze_input and self.baseline_block is not None:
            memory.write(view_input, self.baseline_block)
        if not _steady_single_view_input(memory, view_input):
            return False
        address = view_input + VIEW_INPUT_VERTICAL_SCALE_OFFSET
        if not memory.readable(address, 2):
            return False
        before = memory.s16(address)
        if self.baseline is None:
            if before <= 0 or not _steady_single_view_input(memory, view_input):
                return False
            self.baseline = before
            if self.freeze_input:
                self.baseline_block = memory.bytes(view_input, 14 * 2)
        mutated = self.hits % 2 == 0
        after = self.baseline // 2 if mutated else self.baseline
        memory.write_u16(address, after & 0xffff)
        if self.writer is not None:
            record = view_projection_record(ctx)
            record["mutation"] = {
                "word": 6,
                "address": f"0x{address:08x}",
                "level": level,
                "player": f"0x{player:08x}",
                "camera": f"0x{camera:08x}",
                "input_frozen": self.freeze_input,
                "baseline": self.baseline,
                "before": before,
                "after": after,
                "mutated": mutated,
            }
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"view projection perturb probe complete: {self.hits} observations\n")


class ActorSubmissionProbe(CountingBreakpoint):
    """Sample one game-owned object/model pointer entering actor submission."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("render_actor"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = actor_submission_record(ctx)
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"actor submission probe complete: {self.hits} observations\n")


class GeometrySubmissionProbe(CountingBreakpoint):
    """Sample the raw geometry packet boundary after camera transforms."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(GEOMETRY_SUBMISSION, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        memory = ctx.memory
        player = memory.u32(GLOBALS["Player"])
        if not player or not memory.valid(player):
            return False
        camera = memory.u32(player + PLAYER_CAMERA_OFFSET)
        if not camera or not memory.valid(camera):
            return False
        record = geometry_submission_record(ctx)
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"geometry submission probe complete: {self.hits} observations\n")


class GeometryRasterReturnProbe(CountingBreakpoint):
    """Sample the post-transform raster scratch paired with geometry calls."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(GEOMETRY_RASTER_RETURN, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = geometry_raster_return_record(ctx)
        if record is None:
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"geometry raster probe complete: {self.hits} observations\n")


class _TransformedVertexEntryProbe(TonyBreakpoint):
    """Carry entry arguments to the completed-transform return tail."""

    def __init__(self, owner):
        super().__init__(VERTEX_TRANSFORM, internal=True)
        self.owner = owner

    def on_hit(self, ctx: Context) -> None:
        self.owner.pending_calls.append({
            "input_vertices": ctx.arg(0),
            "vertex_count_raw": ctx.arg(1),
            "state": ctx.arg(2),
        })


class TransformedVertexProbe(CountingBreakpoint):
    """Sample common projected vertices after the transform has completed."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(VERTEX_TRANSFORM_RETURN, count=count, internal=True)
        self.writer = writer
        self.diagnostics = 0
        self.pending_calls = []
        self.entry_probe = _TransformedVertexEntryProbe(self)

    def on_count(self, ctx: Context) -> bool:
        call_arguments = self.pending_calls.pop(0) if self.pending_calls else None
        record = transformed_vertex_record(ctx, call_arguments)
        if record is None:
            return False
        if not record.get("accepted", False):
            # Frontend/menu model transforms are common before level entry.
            # Retain a small diagnostic sample without consuming the requested
            # gameplay observations.
            if self.diagnostics < 8:
                if self.writer is None:
                    self.emit(record)
                else:
                    self.writer.event(record)
                self.diagnostics += 1
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        self.entry_probe.enabled = False
        gdb.write(f"transformed vertex probe complete: {self.hits} observations\n")


class CameraPositionTransformProbe(CountingBreakpoint):
    """Sample the local/effect inputs at the final camera matrix conversion."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(0x004E85A0, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = camera_position_transform_record(ctx)
        if record is None:
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera position transform probe complete: {self.hits} observations\n")


class CameraCollisionProbe(CountingBreakpoint):
    """Sample the game-owned world query used by the camera position stage."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(CAMERA_COLLISION_QUERY, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = camera_collision_record(ctx)
        if record is None:
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera collision probe complete: {self.hits} observations\n")


class CameraCollisionResultProbe(CountingBreakpoint):
    """Sample the caller-side collision result branch."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(CAMERA_COLLISION_RESULT, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        record = camera_collision_result_record(ctx)
        if record is None:
            return False
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True

    def on_complete(self):
        import gdb

        gdb.write(f"camera collision result probe complete: {self.hits} observations\n")
