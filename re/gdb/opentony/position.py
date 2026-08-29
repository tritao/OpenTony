"""Runtime probes for the shared skater position-commit path."""

from __future__ import annotations

from .breakpoint import Context, CountingBreakpoint, TonyBreakpoint
from .knowledge import GLOBALS, function_name_at
from .memory import mem
from .player import PlayerView

POSITION_COMMIT_CALLS = (
    # FUN_00497f40's first integration add.  This is the value built from
    # +0x58/+0x4c immediately before the in-air collision query; the later
    # shared commit call only exposes the already-integrated live position.
    (0x004983C0, "in-air-integrated-position"),
    (0x00498CF5, "in-air-position-1"),
    (0x0049905C, "in-air-position-2"),
    (0x0049917B, "in-air-position-3"),
    (0x0049F0E5, "physics-dispatch-position"),
)

IN_AIR_POSITION_ADJUSTMENT = 0x00498648
IN_AIR_POSITION_TRANSFORM = 0x004E85A0
IN_AIR_POSITION_PIVOT_ANGLE = 0x00498459
IN_AIR_UPRIGHT_CORRECTION = 0x0049C330
ACTION_PHYSICS_STEP = 0x00493370
ACTION_PHYSICS_ANGLE_READ = 0x00493D23
ACTION_PHYSICS_NEGATIVE_ANGLE = 0x00493D36
ACTION_PHYSICS_POSITIVE_ANGLE = 0x00493D60
ACTION_PHYSICS_FINAL_ANGLE = 0x00493D74


def _word_record(address: int, memory=None) -> dict:
    memory = memory or mem
    raw = memory.bytes(address, 4)
    word = memory.word32(address)
    return {"address": f"0x{address:08x}", "raw": raw.hex(), **word._asdict()}


def _fixed_vector_record(address: int, memory=None) -> dict:
    memory = memory or mem
    fixed = memory.fixed_vec3(address)
    return {"raw": list(fixed.raw), "fixed": list(fixed.values)}


def _fixed_values_record(values: list[int]) -> dict:
    signed = [value - 0x100000000 if value & 0x80000000 else value for value in values]
    return {
        "raw": [value & 0xFFFFFFFF for value in values],
        "signed": signed,
        "fixed": [value / 65536.0 for value in signed],
    }


def _signed_short_vector_record(address: int, memory=None) -> dict:
    """Record one of the short-vector rows used by the orientation matrix."""

    memory = memory or mem
    raw = [memory.u16(address + index * 2) for index in range(3)]
    return {"raw": raw, "signed": [value - 0x10000 if value & 0x8000 else value for value in raw]}


def _basis_vector_record(address: int, memory=None) -> dict:
    """Record the sign-extended Q12 basis words used by physics math."""

    memory = memory or mem
    signed = [memory.s32(address + index * 4) for index in range(3)]
    return {"raw": signed, "q12": [value / 4096.0 for value in signed]}


def _orientation_record(player: int, memory=None) -> dict:
    """Capture the turn accumulator, matrix rows, and integer basis at a commit."""

    memory = memory or mem
    return {
        "turn_accumulator": _word_record(player + 0x3144, memory),
        "turn_mirror": _word_record(player + 0x3148, memory),
        "matrix_rows": {
            "row_0": _signed_short_vector_record(player + 0x2E58, memory),
            "row_1": _signed_short_vector_record(player + 0x2E5E, memory),
            "row_2": _signed_short_vector_record(player + 0x2E64, memory),
        },
        "basis": {
            "basis_0": _basis_vector_record(player + 0x30F4, memory),
            "basis_1": _basis_vector_record(player + 0x3100, memory),
            "basis_2": _basis_vector_record(player + 0x310C, memory),
        },
        "correction_58": _fixed_vector_record(player + 0x58, memory),
    }


class PositionCommitBreakpoint(CountingBreakpoint):
    """Log arguments and state at a caller-side callsite."""

    def __init__(self, address: int, label: str, count: int, writer=None):
        self.label = label
        self.writer = writer
        super().__init__(address, count=count, internal=True)

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        if self.label == "in-air-integrated-position":
            # FUN_00497f40 loads ECX with &player->position before calling the
            # shared vector add.  The other callsites use the player object as
            # their thiscall receiver, so normalize this one before reading
            # the surrounding player state.
            player -= PlayerView.POSITION_OFFSET
        if not ctx.memory.valid(player):
            return False
        view = PlayerView(player, ctx.memory)
        # These breakpoints are placed on the five-byte CALL instructions,
        # after the caller has pushed the three cdecl arguments.  They are not
        # function-entry breakpoints, so the first argument is at ESP.
        arguments = [ctx.callsite_arg(index) for index in range(3)]
        record = {
            "type": "position_commit",
            "frame": ctx.frame,
            "callsite": self.label,
            "eip": f"0x{ctx.eip:08x}",
            "function": function_name_at(ctx.eip),
            "caller_return": None,
            "player": f"0x{player:08x}",
            "arguments": [_word_record(ctx.esp + index * 4, ctx.memory) for index in range(3)],
            "argument_values": [f"0x{value:08x}" for value in arguments],
            "argument_vector": (
                _fixed_vector_record(arguments[0], ctx.memory)
                if ctx.memory.valid(arguments[0])
                else None
            ),
            "position_before": _fixed_vector_record(player + view.POSITION_OFFSET, ctx.memory),
            "vector_4c": _fixed_vector_record(player + view.VECTOR_4C_OFFSET, ctx.memory),
            "position_history": _fixed_vector_record(player + view.POSITION_HISTORY_OFFSET, ctx.memory),
            "orientation": _orientation_record(player, ctx.memory),
            "physics_state": view.physics_state,
            "unknown_state": view.unknown_state,
        }
        action_mask_address = GLOBALS.get("ActionMask")
        keyboard_address = GLOBALS.get("KeyboardState")
        if action_mask_address is not None:
            record["action_mask"] = ctx.memory.u16(action_mask_address)
        if keyboard_address is not None:
            keyboard = ctx.memory.bytes(keyboard_address, 0x100)
            record["held_keys"] = [code for code, value in enumerate(keyboard) if value & 0x80]
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class InAirPositionAdjustmentBreakpoint(TonyBreakpoint):
    """Capture the in-air local displacement applied after first integration."""

    def __init__(self, writer=None):
        self.writer = writer
        super().__init__(IN_AIR_POSITION_ADJUSTMENT, internal=True)

    def on_hit(self, ctx: Context) -> None:
        player = ctx.register("ebp")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return

        stack_offsets = (0x40, 0x44, 0x48, 0x13C, 0x140, 0x144)
        stack_words = {
            f"0x{offset:04x}": ctx.memory.u32(ctx.esp + offset)
            for offset in stack_offsets
            if ctx.memory.readable(ctx.esp + offset, 4)
        }
        source_pairs = (
            (0x13C, 0x40),
            (0x140, 0x44),
            (0x144, 0x48),
        )
        delta = [
            (stack_words[f"0x{left:04x}"] - stack_words[f"0x{right:04x}"])
            & 0xFFFFFFFF
            for left, right in source_pairs
        ]
        position_before = list(ctx.memory.u32_vec3(player + 0x08))
        position_after = [
            (before + change) & 0xFFFFFFFF
            for before, change in zip(position_before, delta)
        ]
        record = {
            "type": "position_adjustment",
            "function": "Skater_DoPhysicsInAir.position-adjustment",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "stack_pointer": f"0x{ctx.esp:08x}",
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "stack_words": stack_words,
            "delta": _fixed_values_record(delta),
            "position_before": _fixed_values_record(position_before),
            "position_after": _fixed_values_record(position_after),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)


class InAirPositionPivotAngleBreakpoint(TonyBreakpoint):
    """Capture the angle source for the in-air orientation/pivot block."""

    def __init__(self, writer=None):
        self.writer = writer
        super().__init__(IN_AIR_POSITION_PIVOT_ANGLE, internal=True)

    def on_hit(self, ctx: Context) -> None:
        player = ctx.register("ebp")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return

        record = {
            "type": "position_pivot_angle",
            "function": "Skater_DoPhysicsInAir.orientation-pivot",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "angle_source": _word_record(player + 0x2E84, ctx.memory),
            "orientation": _orientation_record(player, ctx.memory),
            "controller": _word_record(player + 0x2CCC, ctx.memory),
            "controller_slots": {
                f"0x{offset:02x}": _word_record(
                    ctx.memory.ptr(player + 0x2CCC) + offset,
                    ctx.memory,
                )
                for offset in (0x10, 0x20, 0xA0, 0xB0)
            }
            if ctx.memory.valid(ctx.memory.ptr(player + 0x2CCC))
            else None,
            "vertical_lean": _word_record(player + 0x31A2, ctx.memory),
            "air_control_gate": _word_record(0x0056B7F0, ctx.memory),
            "orientation_gate": _word_record(player + 0x2C80, ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)


class InAirPositionTransformBreakpoint(TonyBreakpoint):
    """Capture the matrix conversion feeding the in-air position adjustment."""

    CALLER_RETURNS = {0x00498459, 0x00498605}

    def __init__(self, writer=None):
        self.writer = writer
        super().__init__(IN_AIR_POSITION_TRANSFORM, internal=True)

    def on_hit(self, ctx: Context) -> None:
        caller_return = ctx.return_address()
        if caller_return not in self.CALLER_RETURNS:
            return
        player = ctx.register("ebp")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return

        matrix = ctx.arg(0)
        vector = ctx.arg(1)
        output = ctx.arg(2)
        record = {
            "type": "position_transform",
            "function": "Fixed_MatrixConvertQ12",
            "eip": f"0x{ctx.eip:08x}",
            "caller_return": f"0x{caller_return:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "matrix": [ctx.memory.s16(matrix + index * 2) for index in range(9)],
            "vector": _fixed_vector_record(vector, ctx.memory),
            "output_before": _fixed_vector_record(output, ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)


class InAirUprightCorrectionBreakpoint(TonyBreakpoint):
    """Capture the live inputs at the common-air upright helper entry."""

    def __init__(self, writer=None):
        self.writer = writer
        super().__init__(IN_AIR_UPRIGHT_CORRECTION, internal=True)

    def on_hit(self, ctx: Context) -> None:
        player = ctx.register("ecx")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return
        record = {
            "type": "air_upright_correction",
            "function": "Skater_DoPhysicsInAir.upright-correction",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "unknown_state": ctx.memory.u32(player + 0x30C4),
            "forward": _basis_vector_record(player + 0x30F4, ctx.memory),
            "air_motion": _basis_vector_record(player + 0x310C, ctx.memory),
            "global_up": _basis_vector_record(0x0056B7C0, ctx.memory),
            "orientation": _orientation_record(player, ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)


class ActionPhysicsAngleBreakpoint(TonyBreakpoint):
    """Trace the causal producer of the in-air orientation angle."""

    def __init__(
        self,
        address: int,
        label: str,
        writer=None,
        player_register: str = "ecx",
    ):
        self.label = label
        self.writer = writer
        self.player_register = player_register
        super().__init__(address, internal=True)

    def on_hit(self, ctx: Context) -> None:
        player = ctx.register(self.player_register)
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return

        controller = ctx.memory.ptr(player + 0x2CCC)
        record = {
            "type": "action_physics_angle",
            "function": "Skater_ActionPhysicsStep.angle-producer",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "phase": self.label,
            "player": f"0x{player:08x}",
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "angle": _word_record(player + 0x2E84, ctx.memory),
            "frame_scale": _word_record(0x0056865C, ctx.memory),
            "vertical_lean": _word_record(player + 0x31A2, ctx.memory),
            "orientation_gate": _word_record(player + 0x2C80, ctx.memory),
            "air_control_gate": _word_record(0x0056B7F0, ctx.memory),
            "controller": _word_record(player + 0x2CCC, ctx.memory),
            "controller_slots": {
                f"0x{offset:02x}": _word_record(controller + offset, ctx.memory)
                for offset in (0x10, 0x20, 0x80, 0x90, 0xA0, 0xB0)
            }
            if ctx.memory.valid(controller)
            else None,
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
