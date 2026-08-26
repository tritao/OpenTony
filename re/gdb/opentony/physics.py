"""First conservative runtime probe for the skater physics dispatcher."""

from __future__ import annotations

import struct

from .breakpoint import Context, CountingBreakpoint
from .knowledge import GLOBALS, function_address
from .player import PlayerView

PLAYER_DIFF_SIZE = 0x3208

# Static case-to-callee map from the dispatcher switch.  Runtime probes keep
# this as a candidate list because several cases call more than one helper.
DISPATCH_HANDLER_CANDIDATES = {
    0: ["0x0049dad0", "0x00496550", "0x00495cc0", "0x0049d9c0"],
    1: ["0x00497f40"],
    2: ["0x00496550"],
    3: ["0x00497f40"],
    4: ["0x00494210"],
    5: ["0x00499710"],
    6: ["0x004993f0", "0x00497f40"],
    7: ["0x0049dad0", "0x00496550", "0x00495cc0", "0x0049d9c0"],
    8: ["0x004995d0"],
}

# These are the dedicated non-ground/non-air dispatcher callees.  Keep the
# addresses explicit: unlike the common in-air handler, their generated
# names are not yet stable enough to use as the probe's identity.
SPECIAL_HANDLER_INFO = {
    0x00494210: (4, "State4Routine_94210"),
    0x00499710: (5, "State5Routine_99710"),
    0x004993F0: (6, "State6Routine_993f0"),
    0x004995D0: (8, "State8Routine_995d0"),
}

PHYSICS_STATE_REQUEST = 0x004900B0
PHYSICS_STATE_WRITER = 0x004902BF
AIR_COLLISION_QUERY = 0x00498A7D
ACTION_STATE_SIZE = 0x10
OLLIE_LATCH_WRITERS = {
    0x0049A640: "ground_or_state7_latch",
    0x0049A6AC: "state4_or_state5_latch",
    0x0049A751: "launch_latch_consume",
}

# The action updater at 0x00489a10 advances these records in 0x10-byte
# strides. Keep the complete low-action bank here so a physics trace can show
# which action, if any, is live at the launch boundary; the directional
# records are retained as well because they are useful controls for the
# grounded/airborne comparison.
ACTION_STATE_LAYOUT = {
    "jump": (0x00, 0x0010),
    "grind": (0x10, 0x0080),
    "grab": (0x20, 0x0020),
    "kick": (0x30, 0x0040),
    "spinleft": (0x40, 0x0004),
    "nollie": (0x50, 0x0001),
    "spinright": (0x60, 0x0008),
    "switch": (0x70, 0x0002),
    "left": (0x80, 0x8000),
    "right": (0x90, 0x2000),
    "up": (0xa0, 0x1000),
    "down": (0xb0, 0x4000),
}

# At 0x00498a7d the first collision-query result has just been copied from
# the temporary cast record into EAX, and ESP has already been restored by
# ``add esp, 0x1c`` at 0x00498a7a.  The same local result slot is subsequently
# tested at [ESP+0xf0] (and may be refreshed if the handler reruns the cast).
# Keep a bounded raw window around that slot; the contact origin/packed normal
# fields are populated in this same window and are intentionally left unnamed
# until a runtime capture confirms them.
AIR_COLLISION_STACK_OFFSETS = tuple(range(0xE0, 0x124, 4))
AIR_COLLISION_GLOBALS = {
    "material_flags": 0x0056B768,
    "material_flags_secondary": 0x0056B7A8,
    "material_flags_contact": 0x0056B7AC,
    "material_flags_transient": 0x0056B7B8,
    "material_type": 0x0056B7E8,
}


def _action_state(memory, address: int) -> dict[str, int | str]:
    raw = memory.bytes(address, ACTION_STATE_SIZE)
    return {
        "address": f"0x{address:08x}",
        "raw": raw.hex(),
        "byte0": raw[0],
        "byte1": raw[1],
        "u32_4": int.from_bytes(raw[4:8], "little"),
        "u32_8": int.from_bytes(raw[8:12], "little"),
        "u32_c": int.from_bytes(raw[12:16], "little"),
    }


def _input_observation(memory) -> dict:
    action_mask_address = GLOBALS.get("ActionMask")
    action_state_address = GLOBALS.get("InputActionStates")
    if action_mask_address is None or action_state_address is None:
        return {"action_mask": None, "action_mask_address": None, "jump_action_state": None}
    action_states = {
        name: _action_state(memory, action_state_address + offset)
        for name, (offset, _bit) in ACTION_STATE_LAYOUT.items()
    }
    return {
        "action_mask": memory.u16(action_mask_address),
        "action_mask_address": f"0x{action_mask_address:08x}",
        "jump_action_state": action_states["jump"],
        "action_states": action_states,
    }


def _changed_words(previous: bytes, current: bytes) -> list[dict[str, int | str]]:
    """Return raw 32-bit changes while preserving their player-relative offsets."""

    changes = []
    for index, (before_word, after_word) in enumerate(
        zip(
            struct.iter_unpack("<I", previous),
            struct.iter_unpack("<I", current),
            strict=True,
        )
    ):
        before = before_word[0]
        after = after_word[0]
        if before != after:
            changes.append(
                {
                    "offset": f"0x{index * 4:04x}",
                    "before": before,
                    "after": after,
                }
            )
    return changes


class PhysicsProbe(CountingBreakpoint):
    """Observe the dispatcher without assigning meanings to its state enum."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("physics_dispatch"), count=count, internal=True)
        self.writer = writer
        self._previous_unknown_state = None

    def on_count(self, ctx: Context) -> bool:
        # Skater_PhysicsDispatcher is a __thiscall routine.  The player is in
        # ECX, not at the first cdecl stack argument slot.
        player = ctx.this_ptr()
        if not ctx.memory.valid(player):
            return False
        view = PlayerView(player, ctx.memory)
        position = view.position
        position_history = view.position_history
        record = {
            "type": "physics",
            "frame": ctx.frame,
            "function": "Skater_PhysicsDispatcher",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "previous_physics_state": view.previous_physics_state,
            "unknown_state": view.unknown_state,
            "previous_unknown_state": self._previous_unknown_state,
            "unknown_state_changed": (
                self._previous_unknown_state is not None
                and self._previous_unknown_state != view.unknown_state
            ),
            "dispatcher_case": view.physics_state,
            "handler_candidates": DISPATCH_HANDLER_CANDIDATES.get(view.physics_state, []),
            "position_raw": list(view.position_raw),
            "position_fixed": list(position.values),
            "position_history_raw": list(view.position_history_raw),
            "position_history_fixed": list(position_history.values),
            **_input_observation(ctx.memory),
        }
        self._previous_unknown_state = view.unknown_state
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class SyntheticPhysicsStateForceProbe(CountingBreakpoint):
    """Inject one raw state at a live dispatcher entry for handler smoke tests.

    This is deliberately not a transition writer.  It changes the field
    directly so a controlled experiment can execute a dispatcher case whose
    natural collision/action predicate has not yet been reproduced.  The
    emitted event makes that distinction explicit.
    """

    ADDRESS = 0x0049DB93

    def __init__(self, state: int, count: int = 1, writer=None):
        super().__init__(self.ADDRESS, count=count, internal=True)
        self.state = state
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        # The dispatcher prologue has copied ECX to ESI by this instruction;
        # the following load is the first read of player+0x30b8.
        player = ctx.register("esi")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        previous = ctx.memory.u32(player + PlayerView.PHYSICS_STATE_OFFSET)
        if previous != 0:
            return False
        ctx.memory.write_u32(player + PlayerView.PHYSICS_STATE_OFFSET, self.state)
        record = {
            "type": "physics_state_force",
            "synthetic": True,
            "function": "Skater_PhysicsDispatcher.state_load",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.memory.u32(ctx.esp + 8):08x}"
            if ctx.memory.readable(ctx.esp + 8, 4)
            else None,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "physics_state_before": previous,
            "physics_state_forced": self.state,
            "field": "player+0x30b8",
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class MovementPhysicsProbe(CountingBreakpoint):
    """Observe the action/velocity step before the main physics frame."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("skater_action_step"), count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        # Skater_ActionPhysicsStep is a __thiscall routine. It runs at the
        # start of 0x0049e680, before prephysics/ollie and before the
        # dispatcher, and consumes the directional action records at the
        # player-relative action bank pointer (+0x2ccc).
        player = ctx.this_ptr()
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        record = {
            "type": "movement_physics_step",
            "function": "Skater_ActionPhysicsStep",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "previous_physics_state": view.previous_physics_state,
            "position_raw": list(view.position_raw),
            "velocity_raw": list(ctx.memory.u32_vec3(player + 0x4C)),
            "acceleration_raw": list(ctx.memory.u32_vec3(player + 0x58)),
            "movement_target_x": ctx.memory.s32(player + 0x3144),
            "movement_target_z": ctx.memory.s32(player + 0x3148),
            "steering_active": ctx.memory.s32(player + 0x2E7C),
            "brake_mode": ctx.memory.s32(player + 0x2E78),
            "heading_input": ctx.memory.s8(player + 0x31A1),
            "heading_deadband": ctx.memory.s8(player + 0x31A2),
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class InAirHandlerProbe(CountingBreakpoint):
    """Confirm runtime entry into the candidate in-air handler."""

    ADDRESS = 0x00497F40

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(self.ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        record = {
            "type": "physics_handler",
            "handler": "0x00497f40",
            "function": "Skater_DoPhysicsInAir",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "previous_physics_state": view.previous_physics_state,
            "unknown_state": view.unknown_state,
            "position_raw": list(view.position_raw),
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


def _special_handler_context(player: int, memory) -> dict:
    """Capture raw motion/contact context at a dedicated state entry."""

    def s32_vec3(address: int) -> list[int]:
        return [memory.s32(address + index * 4) for index in range(3)]

    def signed_short_vec3(address: int) -> list[int]:
        values = [memory.u16(address + index * 2) for index in range(3)]
        return [value - 0x10000 if value & 0x8000 else value for value in values]

    return {
        "velocity_raw": list(memory.u32_vec3(player + 0x4C)),
        "velocity_fixed": list(memory.fixed_vec3(player + 0x4C).values),
        "acceleration_raw": list(memory.u32_vec3(player + 0x58)),
        "acceleration_fixed": list(memory.fixed_vec3(player + 0x58).values),
        "basis_q12": {
            "basis_0": s32_vec3(player + 0x30F4),
            "basis_1": s32_vec3(player + 0x3100),
            "basis_2": s32_vec3(player + 0x310C),
        },
        "orientation_rows_raw": {
            "row_0": signed_short_vec3(player + 0x2E58),
            "row_1": signed_short_vec3(player + 0x2E5E),
            "row_2": signed_short_vec3(player + 0x2E64),
        },
        "contact_fields": {
            "surface_vector_raw": list(memory.u32_vec3(player + 0x3118)),
            "surface_normal_xy_raw": memory.u32(player + 0x3128),
            "surface_normal_z_raw": memory.u32(player + 0x312C),
            "surface_effect_raw": memory.u32(player + 0x2DB4),
        },
        "state_fields": {
            "unknown_state": memory.u32(player + 0x30C4),
            "previous_state": memory.u32(player + 0x30C0),
            "ollie_pending": memory.u32(player + 0x2DD8),
            "ollie_in_progress": memory.u32(player + 0x2DDC),
            "ollie_latch": memory.u32(player + 0x2DE0),
            "ollie_charge": memory.u32(player + 0x2DE8),
            "special_action": memory.u32(player + 0x29C8),
            "off_ground_mode": memory.u32(player + 0x2F60),
        },
    }


class SpecialPhysicsHandlerProbe(CountingBreakpoint):
    """Observe entry into one of the dedicated raw-state handlers."""

    def __init__(self, address: int, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.writer = writer
        self.state, self.stage = SPECIAL_HANDLER_INFO[address]

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        record = {
            "type": "physics_special_handler",
            "handler": f"0x{self.address:08x}",
            "stage": self.stage,
            "function": self.stage,
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "previous_physics_state": view.previous_physics_state,
            "unknown_state": view.unknown_state,
            "position_raw": list(view.position_raw),
            **_special_handler_context(player, ctx.memory),
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class AirCollisionQueryProbe(CountingBreakpoint):
    """Capture the in-air collision result before landing branch selection.

    This is deliberately a raw observation probe.  The retail handler has
    more than one contact/result predicate, so this records the first cast
    result, the repeated result slot, the nearby cast payload, and the
    material globals without assigning a landing meaning to any one value.
    """

    ADDRESS = AIR_COLLISION_QUERY

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(self.ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        # Unlike the dispatcher and handler entries, this is an instruction
        # breakpoint inside a __fastcall routine.  EBP is the player here;
        # ECX no longer denotes the object at this point.
        player = ctx.register("ebp")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        stack_words = {}
        for offset in AIR_COLLISION_STACK_OFFSETS:
            address = ctx.esp + offset
            if ctx.memory.readable(address, 4):
                stack_words[f"0x{offset:04x}"] = ctx.memory.u32(address)
        record = {
            "type": "air_collision_query",
            "function": "Skater_DoPhysicsInAir.collision_result",
            "eip": f"0x{ctx.eip:08x}",
            "callsite": f"0x{self.ADDRESS:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "stack_pointer": f"0x{ctx.esp:08x}",
            "physics_state": view.physics_state,
            "previous_physics_state": view.previous_physics_state,
            "unknown_state": view.unknown_state,
            "position_raw": list(view.position_raw),
            "velocity_raw": list(ctx.memory.u32_vec3(player + 0x4C)),
            "acceleration_raw": list(ctx.memory.u32_vec3(player + 0x58)),
            # EAX is the value loaded from [old ESP+0x10c] immediately before
            # the stack cleanup at 0x00498a7a.
            "collision_result_flag": ctx.register("eax"),
            # This is the same result local checked again at 0x00498add; it is
            # not a second independent collision query at this breakpoint.
            "collision_result_slot": stack_words.get("0x00f0"),
            "collision_stack_words": stack_words,
            "collision_globals": {
                name: ctx.memory.u32(address)
                for name, address in AIR_COLLISION_GLOBALS.items()
            },
            # These player fields are sampled before this handler publishes
            # the current cast result. They are retained as pre-query raw
            # context, not presented as the current contact normal.
            "player_contact_fields_before_query": {
                "surface_vector_raw": list(ctx.memory.u32_vec3(player + 0x3118)),
                "surface_normal_xy_raw": ctx.memory.u32(player + 0x3128),
                "surface_normal_z_raw": ctx.memory.u32(player + 0x312C),
                "surface_effect_raw": ctx.memory.u32(player + 0x2DB4),
            },
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class PhysicsStateRequestProbe(CountingBreakpoint):
    """Record callers requesting a new physics state through 0x004900b0."""

    ADDRESS = PHYSICS_STATE_REQUEST

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(self.ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        record = {
            "type": "physics_state_request",
            "function": "Physics_SetState",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "state_before": view.physics_state,
            "state_requested": ctx.arg(0),
            "reason": ctx.arg(1),
            "reason_hex": f"0x{ctx.arg(1):08x}",
            "physics_state_writer": f"0x{PHYSICS_STATE_WRITER:08x}",
            "previous_physics_state": view.previous_physics_state,
            "unknown_state": view.unknown_state,
            "position_raw": list(view.position_raw),
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class PhysicsStateWriterProbe(CountingBreakpoint):
    """Record the exact instruction that stores Player+0x30b8."""

    ADDRESS = PHYSICS_STATE_WRITER

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(self.ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        # The helper keeps the object in ESI and the requested state in EBP
        # until the store at 0x004902bf.  This is an instruction breakpoint,
        # so the old value is still present in memory.
        player = ctx.register("esi")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        record = {
            "type": "physics_state_writer",
            "function": "Physics_SetState.store_30b8",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "state_before": view.physics_state,
            "state_after": ctx.register("ebp"),
            "writer_pc": f"0x{self.ADDRESS:08x}",
            "unknown_state": view.unknown_state,
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class OllieLatchProbe(CountingBreakpoint):
    """Record exact pre-write PCs for the ollie latch/launch stores."""

    def __init__(self, address: int, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        view = PlayerView(player, ctx.memory)
        record = {
            "type": "ollie_latch_writer",
            "writer_pc": f"0x{ctx.eip:08x}",
            "writer": OLLIE_LATCH_WRITERS.get(ctx.eip, "unknown"),
            "function": "Skater_PrePhysicsOllie",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "physics_state": view.physics_state,
            "charge": ctx.memory.u32(player + 0x2DE8),
            "latch_before": ctx.memory.u32(player + 0x2DE0),
            "pending_before": ctx.memory.u32(player + 0x2DD8),
            "in_progress": ctx.memory.u32(player + 0x2DDC),
            **_input_observation(ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class PlayerDiffProbe(CountingBreakpoint):
    """Record per-dispatch player-word changes and the active action mask."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(function_address("physics_dispatch"), count=count, internal=True)
        self.writer = writer
        self._previous_player = None
        self._previous_bytes = None

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        if not ctx.memory.valid(player):
            return False
        current = ctx.memory.bytes(player, PLAYER_DIFF_SIZE)
        previous = self._previous_bytes if self._previous_player == player else None
        action_mask_address = GLOBALS.get("ActionMask")
        record = {
            "type": "player_diff",
            "frame": ctx.frame,
            "function": "Skater_PhysicsDispatcher",
            "eip": f"0x{ctx.eip:08x}",
            "player": f"0x{player:08x}",
            "physics_state": ctx.memory.u32(player + PlayerView.PHYSICS_STATE_OFFSET),
            "action_mask": (
                ctx.memory.u16(action_mask_address) if action_mask_address is not None else None
            ),
            "snapshot_size": PLAYER_DIFF_SIZE,
            "previous_available": previous is not None,
            "changed_words": _changed_words(previous, current) if previous is not None else [],
        }
        self._previous_player = player
        self._previous_bytes = current
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True
