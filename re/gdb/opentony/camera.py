"""Evidence-preserving runtime views for the recovered THPS2 camera object."""

from __future__ import annotations

import struct

from .breakpoint import Context, CountingBreakpoint
from .knowledge import GLOBALS, function_address

CAMERA_SIZE = 0x674
PLAYER_CAMERA_OFFSET = 0x29B0
PLAYER_SECONDARY_LINK_OFFSET = 0x29BC
VIEWPORT_STATE = 0x00563A38
VIEWPORT_SCALE_X = 0x00563A6C
VIEWPORT_SCALE_Y = 0x00563A70


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


def _optional_u32(memory, address: int) -> int | None:
    return memory.u32(address) if memory.readable(address, 4) else None


def _optional_s32(memory, address: int) -> int | None:
    return memory.s32(address) if memory.readable(address, 4) else None


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
        "time_or_smoothing_a": _field_words(memory, camera, 0x410),
        "time_or_smoothing_b": _field_words(memory, camera, 0x414),
        "smoothing_counter_a": _field_words(memory, camera, 0x5D8),
        "smoothing_counter_b": _field_words(memory, camera, 0x5DC),
        "smoothing_counter_c": _field_words(memory, camera, 0x5E0),
        "smoothing_counter_d": _field_words(memory, camera, 0x5E4),
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
        record["tripod_follow_offset"] = _words(memory, tripod + 0x310C, 3)
        record["tripod_physics_state"] = memory.u32(tripod + 0x30B8)
        record["tripod_behavior_flag"] = memory.u32(tripod + 0x2F64)
        record["tripod_effect_gate"] = memory.u32(tripod + 0x2DDC)
        record["tripod_effect_transform_gate"] = memory.u32(tripod + 0x2C68)
        record["tripod_unknown_state"] = memory.u32(tripod + 0x30C4)
    else:
        record["tripod_position"] = None
        record["tripod_follow_offset"] = None
        record["tripod_physics_state"] = None
        record["tripod_behavior_flag"] = None
        record["tripod_effect_gate"] = None
        record["tripod_effect_transform_gate"] = None
        record["tripod_unknown_state"] = None
    return record


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
