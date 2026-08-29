"""THPS2-specific addresses, identities, and recovered field knowledge."""

from __future__ import annotations

from ..knowledge import BUILD_SHA256, GLOBALS, known_function_addresses

THPS2_BUILD_SHA256 = BUILD_SHA256
THPS2_ADDRESSES = dict(known_function_addresses())
THPS2_ADDRESSES["physics_frame"] = (
    0x0049E680,
    "per-frame gameplay skater action, collision, state, and position-update wrapper",
    "re/evidence/functions/physics.md",
)

THPS2_LEVELS = {
    "hangar": 0,
    "warehouse": 12,
}

ACTION_STATE_BASE = GLOBALS["InputActionStates"]
ACTION_STATE_RECORDS = {
    "jump": (ACTION_STATE_BASE, 0x0010),
    "grind": (ACTION_STATE_BASE + 0x10, 0x0080),
    "grab": (ACTION_STATE_BASE + 0x20, 0x0020),
    "kick": (ACTION_STATE_BASE + 0x30, 0x0040),
    "spinleft": (ACTION_STATE_BASE + 0x40, 0x0004),
    "nollie": (ACTION_STATE_BASE + 0x50, 0x0001),
    "spinright": (ACTION_STATE_BASE + 0x60, 0x0008),
    "switch": (ACTION_STATE_BASE + 0x70, 0x0002),
    "left": (ACTION_STATE_BASE + 0x80, 0x8000),
    "right": (ACTION_STATE_BASE + 0x90, 0x2000),
    "up": (ACTION_STATE_BASE + 0xA0, 0x1000),
    "down": (ACTION_STATE_BASE + 0xB0, 0x4000),
}

RECORDING_RAW_AXIS_ADDRESS = 0x0056AFBD
RECORDING_NORMALIZED_AXIS_ADDRESS = 0x0056B140
RECORDING_TIMING_PREVIOUS_TIME = 0x00568604
RECORDING_TIMING_RING_INDEX = 0x0056A934
RECORDING_TIMING_DELTA = 0x0056A93C
RECORDING_TIMING_PREVIOUS_STORE = 0x00468BA1

PLAYER_WATCH_FIELDS = {
    "position.x": 0x08,
    "position.y": 0x0C,
    "position.z": 0x10,
    "position_history.x": 0xBC,
    "position_history.y": 0xC0,
    "position_history.z": 0xC4,
    "physics_state": 0x30B8,
    "previous_physics_state": 0x30C0,
    "unknown_state": 0x30C4,
}
