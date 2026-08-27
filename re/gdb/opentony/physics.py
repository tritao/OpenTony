"""First conservative runtime probe for the skater physics dispatcher."""

from __future__ import annotations

import struct

from .breakpoint import Context, CountingBreakpoint, TonyBreakpoint
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

# 0x0049b010 is a __thiscall producer.  Keep its raw data contract in the
# probe rather than assigning gameplay names to the profile slots or to the
# source vector.  The direct addresses below are intentionally local to this
# runtime helper until the profile/configuration functions have stable tracked
# symbols.
GROUND_MOTION_PRODUCER = 0x0049B010
GROUND_MOTION_PROFILE_GLOBALS = {
    "mode": 0x00533F38,
    "player_selector": 0x0056A854,
    "profile_table": 0x0056A3D8,
}
GROUND_MOTION_PROFILE_OFFSETS = tuple(index * 0x10 for index in range(16))

# Breakpoints are placed on the instruction that is about to perform each
# store.  This makes the register value the exact value that retail is about
# to publish, while the player field is still the pre-store value.
GROUND_MOTION_CORRECTION_WRITERS = {
    0x0049B2B3: (0x58, "x", "ecx", "animation_or_ordinary"),
    0x0049B2B6: (0x5C, "y", "edx", "animation_or_ordinary"),
    0x0049B2B9: (0x60, "z", "eax", "animation_or_ordinary"),
    0x0049B309: (0x58, "x", "ecx", "animation_5e_or_ordinary"),
    0x0049B30C: (0x5C, "y", "edx", "animation_5e_or_ordinary"),
    0x0049B30F: (0x60, "z", "eax", "animation_5e_or_ordinary"),
    0x0049B3AC: (0x58, "x", "edx", "later_profile"),
    0x0049B3AF: (0x5C, "y", "eax", "later_profile"),
    0x0049B3B2: (0x60, "z", "ecx", "later_profile"),
    0x0049B4E0: (0x58, "x", "eax", "later_profile_repeat"),
    0x0049B4F0: (0x5C, "y", "ecx", "later_profile_repeat"),
    0x0049B4F3: (0x60, "z", "eax", "later_profile_repeat"),
}

GROUND_MOTION_CONTROL_WRITERS = {
    0x0049B0CA: (0x2F2C, "cooldown", "eax", "decrement"),
    0x0049B1D5: (0x2F2C, "cooldown", None, "rearm_0xaa"),
    0x0049B1DF: (0x30A8, "event_pending", None, "set"),
    0x0049B204: (0x2DC8, "speed_threshold", "edx", "random_seed_0xaa"),
    0x0049B20F: (0x0108, "animation_rate", None, "set"),
    0x0049B21B: (0x2F2C, "cooldown", None, "rearm_without_animation"),
    0x0049B339: (0x30AC, "event_code", None, "set"),
    0x0049B34B: (0x30A8, "event_pending", None, "clear"),
    0x0049B427: (0x2F2C, "cooldown", None, "rearm_0xdc"),
    0x0049B44C: (0x2DC8, "speed_threshold", "edx", "random_seed_0xdc"),
    0x0049B457: (0x0108, "animation_rate", None, "set"),
    0x0049B461: (0x30A8, "event_pending", None, "set"),
}
GROUND_MOTION_RANDOM_SITES = {
    0x0049B1C4: "random_seed_0xaa",
    0x0049B416: "random_seed_0xdc",
}
# The outer physics wrapper refreshes the persistent +0x2dc8 threshold after
# dispatch. These are the two post-call return sites for FUN_0048f3a0(3), not
# the B010-local rearm draws above.
GROUND_MOTION_THRESHOLD_RANDOM_SITES = {
    0x0049EAF2: "threshold_seed_0xdc",
    0x0049EB2A: "threshold_seed_0xaa",
}
# These are the other shared-RNG draws in the outer frame prologue. The
# state-two draw feeds +0x2dac; the ordinary frame draw feeds +0x2f3c.
GROUND_MOTION_FRAME_RANDOM_SITES = {
    0x0049E747: "state_two_motion_seed",
    0x0049E836: "frame_speed_seed",
}

# 0x0048f3a0 is currently described by the surrounding reconstruction as a
# shared RNG service.  Its body has not yet been promoted to a native model,
# so trace the function boundary itself as well as the older consumer-local
# return probes.  The direct disassembly presently suggests a stat/profile
# lookup rather than a stateful PRNG; keep that distinction visible in the
# event name until dynamic evidence settles it.
SHARED_RANDOM_SERVICE = 0x0048F3A0

# Return sites for FUN_0048f3a0 draws owned by FUN_0049a280.  Breakpoints are
# placed after the call, where EAX contains the returned roll.  The charge
# pairs and the launch/release sequence are kept distinct because retail
# consumes them from different branches of the prephysics function.
OLLIE_RANDOM_SITES = {
    0x0049A5C0: "charge_cap.first",
    0x0049A5CA: "charge_cap.second",
    0x0049A5FF: "charge_refresh.first",
    0x0049A609: "charge_refresh.second",
    0x0049A993: "impulse.first",
    0x0049A99D: "impulse.second",
    0x0049A9BF: "impulse.third",
    0x0049A9EE: "impulse.fourth",
    0x0049AA3F: "impulse.fifth",
    0x0049AB10: "impulse.first",
    0x0049AB1A: "impulse.second",
    0x0049AB3D: "impulse.third",
    0x0049AB6E: "impulse.fourth",
    0x0049ABC0: "impulse.fifth",
    0x0049AC1F: "early_release.first",
    0x0049AC29: "early_release.second",
}

# FUN_0049d480's shared-RNG return sites. The rescale branch can consume
# three component rolls in addition to its initial threshold roll; the
# ordinary decay path starts at 0x0049d5a5.
VELOCITY_DAMPING_RANDOM_SITES = {
    0x0049D4A6: "rescale_threshold",
    0x0049D4E7: "rescale_x",
    0x0049D51E: "rescale_y",
    0x0049D557: "rescale_z",
    0x0049D5A5: "decay_threshold",
}

# FUN_0049d480 calls the fixed-point component helper once for each response
# component on the randomized decay path.  Break after each call, before the
# retail global scale and shift are applied, so native replay can preserve the
# raw producer rather than baking in one observed response value.
VELOCITY_DAMPING_COMPONENT_SITES = {
    0x0049D5D4: "decay_x",
    0x0049D5EF: "decay_y",
    0x0049D60C: "decay_z",
}

# FUN_004caa20 adds or subtracts a prepared vector from the player's
# temporary +0x58 correction.  The in-air handler uses this boundary for
# action/gravity corrections; retain the operand before it is folded into the
# player so replay can distinguish the producer from the final accumulator.
AIR_CORRECTION_APPLY_ADDRESS = 0x004CAA20
RESPONSE_CORRECTION_ADD_ADDRESS = 0x004CA9F0
# The outer frame wrapper has just completed FUN_004cac30's scale step at
# this address. EBP is the player and +0x58 contains the authoritative
# transient correction retained in the frame-end snapshot.
MOTION_CORRECTION_SOURCE_ADDRESS = 0x0049F1EE

# The local gate is not assigned by B010 itself.  These are the static writer
# chain discovered in the retail image: per-profile source flags, their copy
# into the runtime table, and the final table store consumed by B010.
GROUND_MOTION_PROFILE_WRITERS = {
    0x00413F39: ("source_profile_table_55fc2c", "eax", "ecx", "profile_initializer"),
    0x00413F40: ("source_profile_table_55fc34", "eax", "ecx", "profile_initializer"),
    0x00487D27: ("source_profile_table_55fc2c", "ecx", "eax", "profile_record"),
    0x00487D45: ("source_profile_table_55fc34", "ecx", "eax", "profile_record"),
    0x00413C49: ("runtime_profile_table_56a3d8", "ecx", "edx", "runtime_copy"),
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


def _signed32(value: int) -> int:
    value &= 0xFFFFFFFF
    return value - 0x100000000 if value & 0x80000000 else value


def _read_if_available(memory, reader: str, address: int, size: int):
    if not memory.readable(address, size):
        return None
    return getattr(memory, reader)(address)


def _ground_motion_local_profile(memory, player: int) -> dict:
    """Reproduce B010's table lookup without naming the table's semantics."""

    mode = _read_if_available(
        memory, "u32", GROUND_MOTION_PROFILE_GLOBALS["mode"], 4)
    selector = _read_if_available(
        memory, "u32", GROUND_MOTION_PROFILE_GLOBALS["player_selector"], 4)
    player_index = _read_if_available(memory, "u32", player + 0x2CC4, 4)
    lookup_index = None
    if player_index is not None:
        lookup_index = player_index
        if mode == 7 and selector is not None:
            lookup_index = (player_index ^ selector) & 0xFFFFFFFF

    table_address = None
    value = None
    # The startup copy at 0x00413c10 materializes eight entries. Do not read
    # beyond that recovered table merely because a corrupted player index is
    # present in memory.
    if lookup_index is not None and lookup_index < 8:
        table_address = GROUND_MOTION_PROFILE_GLOBALS["profile_table"] + lookup_index * 4
        value = _read_if_available(memory, "s32", table_address, 4)
    return {
        "mode": mode,
        "player_selector": selector,
        "player_index": player_index,
        "lookup_index": lookup_index,
        "table_address": (
            f"0x{table_address:08x}" if table_address is not None else None
        ),
        "value": value,
    }


def _ground_motion_context(player: int, memory) -> dict:
    fields = {
        "physics_state": memory.u32(player + 0x30B8),
        "pending_transition": memory.u32(player + 0x2DD8),
        "physics_lock": memory.u32(player + 0x2DD4),
        "ground_mode_lock": memory.u32(player + 0x2DF8),
        "cooldown": memory.u32(player + 0x2F2C),
        "speed_threshold": memory.u32(player + 0x2DC8),
        "steering_state": memory.u32(player + 0x2E7C),
        "brake_state": memory.u32(player + 0x2E78),
        "event_pending": memory.u32(player + 0x30A8),
        "event_code": memory.u32(player + 0x30AC),
    }
    animation_state = memory.u16(player + 0xF6)
    animation_frame = memory.u16(player + 0xF4)
    controller = _read_if_available(memory, "ptr", player + 0x2CCC, 4)
    controller_slots = None
    controller_axes = None
    profile_slot_10 = None
    if controller is not None and memory.valid(controller):
        controller_slots = {
            f"0x{offset:02x}": _action_state(memory, controller + offset)
            for offset in GROUND_MOTION_PROFILE_OFFSETS
            if memory.readable(controller + offset, ACTION_STATE_SIZE)
        }
        profile_slot_10 = _read_if_available(memory, "u8", controller + 0x10, 1)
        controller_axes = {
            "vertical_0x68": _read_if_available(memory, "s8", controller + 0x68, 1),
            "horizontal_0x69": _read_if_available(memory, "s8", controller + 0x69, 1),
        }

    local_profile = _ground_motion_local_profile(memory, player)
    profile_gate = None
    if profile_slot_10 is not None and local_profile["value"] is not None:
        profile_gate = bool(profile_slot_10 or local_profile["value"])

    fields.update({
        "animation_state": animation_state,
        "animation_frame": animation_frame,
        "animation_finished": memory.u8(player + 0x107),
        "animation_rate": memory.u32(player + 0x108),
        "turn_target": memory.s32(player + 0x3144),
        "turn_target_mirror": memory.s32(player + 0x3148),
        "lean_horizontal": memory.s8(player + 0x31A1),
        "lean_deadband": memory.s8(player + 0x31A2),
    })
    return {
        "player": f"0x{player:08x}",
        "controller": (
            f"0x{controller:08x}" if controller is not None else None
        ),
        "controller_profile_slots": controller_slots,
        "controller_axes": controller_axes,
        "profile_slot_0x10_active": profile_slot_10,
        "local_profile_lookup": local_profile,
        "profile_gate": profile_gate,
        "fields": fields,
        "velocity_raw": list(memory.u32_vec3(player + 0x4C)),
        "correction_before_raw": list(memory.u32_vec3(player + 0x58)),
        "basis_30f4_raw": list(memory.u32_vec3(player + 0x30F4)),
        "surface_vector_3118_raw": list(memory.u32_vec3(player + 0x3118)),
        "surface_transform_xy_raw": memory.u32(player + 0x3128),
        "surface_transform_z_raw": memory.u32(player + 0x312C),
        "retail_predicates": {
            "outer_physics_gate_open": (
                fields["physics_lock"] == 0 and fields["ground_mode_lock"] == 0
            ),
            "ordinary_state": fields["physics_state"] == 0,
            "correction_gate_open": (
                fields["brake_state"] == 0 or fields["steering_state"] == 0
            ),
            "cooldown_open": fields["cooldown"] == 0,
            "state_not_two": fields["physics_state"] != 2,
            "pending_transition_clear": fields["pending_transition"] == 0,
        },
        **_input_observation(memory),
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


class GroundMotionProducerProbe(CountingBreakpoint):
    """Capture the complete raw input set observed at 0x0049b010."""

    ADDRESS = GROUND_MOTION_PRODUCER

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(self.ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.this_ptr()
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        record = {
            "type": "ground_motion_producer",
            "function": "Skater_UpdateAcceleration",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            **_ground_motion_context(player, ctx.memory),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class GroundMotionWriterProbe(CountingBreakpoint):
    """Record an exact B010 store and the value in its source register."""

    def __init__(self, address: int, spec: tuple, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.spec = spec
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        # Every B010 interior store keeps the player in ESI.  Filtering against
        # the generated Player pointer avoids counting a second skater when a
        # two-player session is running.
        player = ctx.register("esi")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False

        field_offset, field_name, register, branch = self.spec
        register_value = None if register is None else ctx.register(register)
        before = ctx.memory.u32(player + field_offset)
        record = {
            "type": "ground_motion_writer",
            "function": "Skater_UpdateAcceleration",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "writer_group": branch,
            "field": field_name,
            "field_offset": f"0x{field_offset:04x}",
            "field_before_raw": before,
            "field_before_s32": _signed32(before),
            "source_register": register,
            "source_register_raw": (
                None if register_value is None else register_value & 0xFFFFFFFF
            ),
            "source_register_s32": (
                None if register_value is None else _signed32(register_value)
            ),
            "animation_state": ctx.memory.u16(player + 0xF6),
            "animation_frame": ctx.memory.u16(player + 0xF4),
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "cooldown": ctx.memory.u32(player + 0x2F2C),
            "speed_threshold": ctx.memory.u32(player + 0x2DC8),
            "basis_30f4_raw": list(ctx.memory.u32_vec3(player + 0x30F4)),
            "correction_before_raw": list(ctx.memory.u32_vec3(player + 0x58)),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


GROUND_MOTION_CONTROL_IMMEDIATES = {
    0x0049B1D5: 0x14,
    0x0049B1DF: 1,
    0x0049B20F: 0x14000,
    0x0049B21B: 0x14,
    0x0049B339: 0x22,
    0x0049B34B: 0,
    0x0049B427: 0x14,
    0x0049B457: 0x14000,
    0x0049B461: 1,
}


class GroundMotionControlWriterProbe(CountingBreakpoint):
    """Record B010 cooldown, threshold, animation, and event stores."""

    def __init__(self, address: int, spec: tuple, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.spec = spec
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.register("esi")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False

        field_offset, field_name, register, operation = self.spec
        register_value = None if register is None else ctx.register(register)
        immediate = GROUND_MOTION_CONTROL_IMMEDIATES.get(ctx.eip)
        value = immediate if immediate is not None else register_value
        before = ctx.memory.u32(player + field_offset)
        record = {
            "type": "ground_motion_control_writer",
            "function": "Skater_UpdateAcceleration",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "operation": operation,
            "field": field_name,
            "field_offset": f"0x{field_offset:04x}",
            "field_before_raw": before,
            "field_before_s32": _signed32(before),
            "source_register": register,
            "source_register_raw": (
                None if register_value is None else register_value & 0xFFFFFFFF
            ),
            "store_value_raw": None if value is None else value & 0xFFFFFFFF,
            "store_value_s32": None if value is None else _signed32(value),
            "animation_state": ctx.memory.u16(player + 0xF6),
            "animation_frame": ctx.memory.u16(player + 0xF4),
            "physics_state": ctx.memory.u32(player + 0x30B8),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class GroundMotionRandomProbe(CountingBreakpoint):
    """Capture B010's raw 0x0048f3a0 result at each return-site use."""

    def __init__(
        self,
        address: int,
        purpose: str,
        count: int | None = None,
        writer=None,
        player_register: str = "esi",
    ):
        super().__init__(address, count=count, internal=True)
        self.purpose = purpose
        self.writer = writer
        self.player_register = player_register

    def on_count(self, ctx: Context) -> bool:
        player = ctx.register(self.player_register)
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        value = ctx.register("eax")
        record = {
            "type": "ground_motion_random_input",
            "function": "Skater_UpdateAcceleration",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "purpose": self.purpose,
            "raw_roll": value & 0xFFFFFFFF,
            "roll_s32": _signed32(value),
            "animation_state": ctx.memory.u16(player + 0xF6),
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "cooldown": ctx.memory.u32(player + 0x2F2C),
            "speed_threshold_before": ctx.memory.u32(player + 0x2DC8),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class SharedRandomServiceReturnProbe(TonyBreakpoint):
    """Complete one direct 0x0048f3a0 observation at its caller return."""

    def __init__(self, owner, address: int, record: dict):
        self.owner = owner
        self.record = record
        super().__init__(address, internal=True, temporary=True)

    def on_hit(self, ctx: Context) -> None:
        self.enabled = False
        self.record["return_value_raw"] = ctx.register("eax") & 0xFFFFFFFF
        self.record["return_value_s32"] = _signed32(ctx.register("eax"))
        self.owner._complete(self, self.record)


class SharedRandomServiceProbe(TonyBreakpoint):
    """Trace every invocation of the shared 0x0048f3a0 service.

    The probe is deliberately non-invasive.  It observes the thiscall object,
    selector argument, caller, and return value; it does not replace the
    result.  ``frame_provider`` lets recording use the controller's canonical
    physics-frame number even when the optional interactive frame clock is not
    armed.
    """

    def __init__(
        self,
        count: int | None = None,
        writer=None,
        frame_provider=None,
    ):
        self.remaining = count
        self.hits = 0
        self.writer = writer
        self.frame_provider = frame_provider
        self._frame = None
        self._ordinal = 0
        self._returns = []
        super().__init__(SHARED_RANDOM_SERVICE, internal=True)

    def _current_frame(self, ctx: Context) -> int:
        if self.frame_provider is not None:
            frame = self.frame_provider()
            if frame is not None:
                return int(frame)
        return ctx.frame

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)

    def on_hit(self, ctx: Context) -> None:
        if self.remaining is not None and self.remaining <= 0:
            self.enabled = False
            return

        return_address = ctx.return_address()
        if not return_address:
            return
        frame = self._current_frame(ctx)
        if frame != self._frame:
            self._frame = frame
            self._ordinal = 0
        ordinal = self._ordinal
        self._ordinal += 1
        selector = ctx.arg(0)
        record = {
            "type": "shared_random_call",
            "function": "FUN_0048f3a0",
            "address": f"0x{SHARED_RANDOM_SERVICE:08x}",
            "frame": frame,
            "ordinal_within_frame": ordinal,
            "caller": f"0x{return_address - 5:08x}",
            "return_address": f"0x{return_address:08x}",
            "this": f"0x{ctx.this_ptr():08x}",
            "argument_raw": selector & 0xFFFFFFFF,
            "argument_s32": _signed32(selector),
            # No PRNG state address has been established.  Explicit nulls make
            # this a useful negative result instead of silently claiming that
            # the caller-local object or a guessed global is RNG state.
            "state_before": None,
            "state_after": None,
            "state_status": "not-established",
        }
        return_probe = SharedRandomServiceReturnProbe(self, return_address, record)
        self._returns.append(return_probe)
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self.enabled = False

    def _complete(self, return_probe, record: dict) -> None:
        if return_probe in self._returns:
            self._returns.remove(return_probe)
        self.hits += 1
        self._emit(record)

    def disable_pending_returns(self) -> None:
        """Stop return probes if the owning trace is closed mid-call."""
        for return_probe in self._returns:
            return_probe.enabled = False
        self._returns.clear()


class OllieRandomProbe(CountingBreakpoint):
    """Capture raw shared-RNG results consumed by retail ollie prephysics."""

    def __init__(self, address: int, purpose: str, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.purpose = purpose
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.register("ebp")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        value = ctx.register("eax")
        record = {
            "type": "ollie_random_input",
            "function": "Skater_PrePhysicsOllie",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "purpose": self.purpose,
            "raw_roll": value & 0xFFFFFFFF,
            "roll_s32": _signed32(value),
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "charge": ctx.memory.u32(player + 0x2DE8),
            "slope_metric": ctx.memory.s32(player + 0x3110),
            "horizontal_speed_metric": ctx.memory.s32(player + 0x2F30),
            "height_delta_metric": (
                ctx.memory.s32(player + 0x2F48) -
                ctx.memory.s32(player + 0x2F4C)
            ) >> 12,
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class VelocityDampingRandomProbe(CountingBreakpoint):
    """Capture shared-RNG results consumed by the post-dispatch damping pass."""

    def __init__(self, address: int, purpose: str, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.purpose = purpose
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.register("esi")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        value = ctx.register("eax")
        record = {
            "type": "velocity_damping_random_input",
            "function": "Skater_DampVelocity",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "purpose": self.purpose,
            "raw_roll": value & 0xFFFFFFFF,
            "roll_s32": _signed32(value),
            "physics_state": ctx.memory.u32(player + 0x30B8),
            "response_velocity": list(ctx.memory.u32_vec3(player + 0x4C)),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class VelocityDampingComponentProbe(CountingBreakpoint):
    """Capture the raw fixed-point component decay producer result."""

    def __init__(self, address: int, purpose: str, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.purpose = purpose
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.register("esi")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        value = ctx.register("eax")
        record = {
            "type": "velocity_damping_component_input",
            "function": "Skater_DampVelocity",
            "eip": f"0x{ctx.eip:08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "purpose": self.purpose,
            "raw_value": value & 0xFFFFFFFF,
            "value_s32": _signed32(value),
            "response_velocity": list(ctx.memory.u32_vec3(player + 0x4C)),
            "retail_scale": ctx.memory.u32(0x0056865C),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class AirCorrectionProbe(CountingBreakpoint):
    """Capture vectors applied to the player's temporary correction."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(AIR_CORRECTION_APPLY_ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        destination = ctx.register("ecx")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(destination):
            return False
        target_offset = destination - current
        if target_offset not in (0x4C, 0x58):
            return False
        source = ctx.memory.u32(ctx.esp + 4)
        operand_raw = (
            list(ctx.memory.u32_vec3(source))
            if ctx.memory.readable(source, 0x0C)
            else None
        )
        record = {
            "type": "air_correction_input",
            "function": "Skater_DoPhysicsInAir",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{current:08x}",
            "target_offset": target_offset,
            "source": f"0x{source:08x}",
            "operand_raw": operand_raw,
            "operand_s32": (
                [_signed32(value) for value in operand_raw]
                if operand_raw is not None
                else None
            ),
            "correction_before": list(ctx.memory.u32_vec3(destination)),
            "physics_state": ctx.memory.u32(current + 0x30B8),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class ResponseCorrectionProbe(CountingBreakpoint):
    """Capture vectors folded into persistent collision response."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(RESPONSE_CORRECTION_ADD_ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        destination = ctx.register("ecx")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(destination) or destination != current + 0x4C:
            return False
        source = ctx.memory.u32(ctx.esp + 4)
        operand_raw = (
            list(ctx.memory.u32_vec3(source))
            if ctx.memory.readable(source, 0x0C)
            else None
        )
        record = {
            "type": "response_correction_input",
            "function": "Skater_PhysicsFrame",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{current:08x}",
            "source": f"0x{source:08x}",
            "operand_raw": operand_raw,
            "operand_s32": (
                [_signed32(value) for value in operand_raw]
                if operand_raw is not None
                else None
            ),
            "response_before": list(ctx.memory.u32_vec3(destination)),
            "physics_state": ctx.memory.u32(current + 0x30B8),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


class MotionCorrectionProbe(CountingBreakpoint):
    """Capture the completed outer-frame +0x58 correction producer."""

    def __init__(self, count: int | None = None, writer=None):
        super().__init__(MOTION_CORRECTION_SOURCE_ADDRESS, count=count, internal=True)
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        player = ctx.register("ebp")
        current = ctx.memory.ptr(GLOBALS["Player"])
        if not ctx.memory.valid(player) or player != current:
            return False
        correction_raw = list(ctx.memory.u32_vec3(player + 0x58))
        record = {
            "type": "motion_correction_input",
            "function": "Skater_PhysicsFrame",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "correction_raw": correction_raw,
            "correction_s32": [_signed32(value) for value in correction_raw],
            "physics_state": ctx.memory.u32(player + 0x30B8),
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        return True


GROUND_MOTION_PROFILE_SOURCE_OFFSETS = {
    0x00487D27: 0x24C,
    0x00487D45: 0x248,
}


class GroundMotionProfileWriterProbe(CountingBreakpoint):
    """Trace the profile source flags into B010's indexed runtime gate."""

    def __init__(self, address: int, spec: tuple, count: int | None = None, writer=None):
        super().__init__(address, count=count, internal=True)
        self.spec = spec
        self.writer = writer

    def on_count(self, ctx: Context) -> bool:
        table_name, index_register, value_register, operation = self.spec
        index = ctx.register(index_register)
        value = ctx.register(value_register)
        record = {
            "type": "ground_motion_profile_writer",
            "eip": f"0x{ctx.eip:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "frame": ctx.frame,
            "table": table_name,
            "index_register": index_register,
            "index": index,
            "value_register": value_register,
            "value_raw": value & 0xFFFFFFFF,
            "value_s32": _signed32(value),
            "operation": operation,
        }
        source_offset = GROUND_MOTION_PROFILE_SOURCE_OFFSETS.get(ctx.eip)
        if source_offset is not None:
            profile_object = ctx.register("ebx")
            record["profile_object"] = f"0x{profile_object:08x}"
            source = profile_object + source_offset
            record["source_record_address"] = f"0x{source:08x}"
            if ctx.memory.readable(source, 4):
                record["source_record_value"] = ctx.memory.u32(source)
        if table_name == "source_profile_table_55fc2c":
            record["destination_address"] = f"0x{0x0055FC2C + index * 4:08x}"
        elif table_name == "source_profile_table_55fc34":
            record["destination_address"] = f"0x{0x0055FC34 + index * 4:08x}"
        elif table_name == "runtime_profile_table_56a3d8":
            record["destination_address"] = f"0x{0x0056A3D8 + index * 4:08x}"
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
            "from": view.physics_state,
            "to": ctx.arg(0),
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
            "from": view.physics_state,
            "to": ctx.register("ebp"),
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
