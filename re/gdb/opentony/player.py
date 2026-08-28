"""Supported view of the THPS2 player object."""

from __future__ import annotations

import struct

from .knowledge import GLOBALS
from .memory import FixedVec3, Memory, mem
from .timing import animation_timing_record

# The established player-diff span is 0x3208 bytes.  The canonical recording
# also carries the raw physics region at +0x2d80, whose 0x490-byte extent ends
# at +0x3210, so use the larger bound for the single capture read.
PLAYER_STATE_BLOB_SIZE = 0x3210
RAW_PHYSICS_OFFSET = 0x2D80
RAW_PHYSICS_SIZE = 0x490


class PlayerView:
    """Expose the currently supported representation of the player fields.

    Unknown offsets should still be inspected through ``Memory.word32``. This
    view intentionally exposes only representations supported by runtime
    evidence.
    """

    POSITION_OFFSET = 0x08
    VECTOR_4C_OFFSET = 0x4C
    POSITION_HISTORY_OFFSET = 0xBC
    PHYSICS_STATE_OFFSET = 0x30B8
    PREVIOUS_PHYSICS_STATE_OFFSET = 0x30C0
    UNKNOWN_STATE_OFFSET = 0x30C4

    def __init__(self, address: int, memory: Memory | None = None):
        self.address = address
        self.memory = memory or mem

    @classmethod
    def current(cls, memory: Memory | None = None) -> PlayerView | None:
        memory = memory or mem
        address = memory.ptr(GLOBALS["Player"])
        if not memory.valid(address):
            return None
        return cls(address, memory)

    @property
    def position(self) -> FixedVec3:
        return self.memory.fixed_vec3(self.address + self.POSITION_OFFSET)

    @property
    def position_raw(self) -> tuple[int, int, int]:
        return self.memory.u32_vec3(self.address + self.POSITION_OFFSET)

    @property
    def vector_4c(self) -> FixedVec3:
        """The contiguous collision/platform response vector at player + 0x4c."""

        return self.memory.fixed_vec3(self.address + self.VECTOR_4C_OFFSET)

    @property
    def vector_4c_raw(self) -> tuple[int, int, int]:
        return self.memory.u32_vec3(self.address + self.VECTOR_4C_OFFSET)

    @property
    def position_history(self) -> FixedVec3:
        return self.memory.fixed_vec3(self.address + self.POSITION_HISTORY_OFFSET)

    @property
    def position_history_raw(self) -> tuple[int, int, int]:
        return self.memory.u32_vec3(self.address + self.POSITION_HISTORY_OFFSET)

    @property
    def physics_state(self) -> int:
        return self.memory.u32(self.address + self.PHYSICS_STATE_OFFSET)

    @property
    def previous_physics_state(self) -> int:
        return self.memory.u32(self.address + self.PREVIOUS_PHYSICS_STATE_OFFSET)

    @property
    def unknown_state(self) -> int:
        return self.memory.u32(self.address + self.UNKNOWN_STATE_OFFSET)


def _blob_u8(blob: bytes, offset: int) -> int:
    return blob[offset]


def _blob_u16(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<H", blob, offset)[0]


def _blob_s16(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<h", blob, offset)[0]


def _blob_u32(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def _blob_s32(value: int) -> int:
    return value - (1 << 32) if value & 0x80000000 else value


def _blob_vec(blob: bytes, offset: int) -> dict | None:
    if offset < 0 or offset + 0x0C > len(blob):
        return None
    raw = list(struct.unpack_from("<3I", blob, offset))
    return {
        "raw": raw,
        "signed": [_blob_s32(value) for value in raw],
    }


def _blob_short_vec(blob: bytes, offset: int) -> dict | None:
    if offset < 0 or offset + 6 > len(blob):
        return None
    raw = [_blob_u16(blob, offset + delta) for delta in (0, 2, 4)]
    return {
        "raw": raw,
        "signed": [
            value - (1 << 16) if value & 0x8000 else value
            for value in raw
        ],
    }


def canonical_player_snapshot(
    player: int,
    memory: Memory | None = None,
) -> dict:
    """Capture the canonical player state with one inferior-memory read.

    The returned schema intentionally matches the long-standing recording and
    retail-replay snapshot schema.  Only the transport changes: all player
    fields, including the raw physics words, are decoded from a local blob so
    each frame does not turn into hundreds of GDB memory round trips.
    """

    memory = memory or mem
    blob = memory.bytes(player, PLAYER_STATE_BLOB_SIZE)
    physics_state = _blob_u32(blob, 0x30B8)
    return {
        "player_address": f"0x{player:08x}",
        "timing": animation_timing_record(memory),
        "raw_physics_words": [
            _blob_u32(blob, RAW_PHYSICS_OFFSET + index)
            for index in range(0, RAW_PHYSICS_SIZE, 4)
        ],
        "physics_state": physics_state,
        "physics": {
            "state_raw": physics_state,
            "previous_state_raw": _blob_u32(blob, 0x30C0),
            "auxiliary_state_raw": _blob_u32(blob, 0x30C4),
            "air_control_enabled": bool(memory.u32(0x0056B7F0)),
        },
        "position": _blob_vec(blob, 0x08),
        "position_history": _blob_vec(blob, 0xBC),
        "response_velocity": _blob_vec(blob, 0x4C),
        "correction": _blob_vec(blob, 0x58),
        "air_motion": _blob_vec(blob, 0x310C),
        "turn": {
            "accumulator_raw": _blob_u32(blob, 0x3144),
            "mirror_raw": _blob_u32(blob, 0x3148),
        },
        "basis": {
            "forward_raw": _blob_vec(blob, 0x30F4),
            "up_raw": _blob_vec(blob, 0x3100),
            "air_raw": _blob_vec(blob, 0x310C),
        },
        "orientation": {
            "row_0": _blob_short_vec(blob, 0x2E58),
            "row_1": _blob_short_vec(blob, 0x2E5E),
            "row_2": _blob_short_vec(blob, 0x2E64),
        },
        "animation": {
            "id_raw": _blob_u16(blob, 0xF6),
            "frame_raw": _blob_s16(blob, 0xF4),
            "fraction_raw": _blob_u16(blob, 0x104),
            "rate_raw": _blob_u32(blob, 0x108),
            "mode_raw": _blob_u8(blob, 0xF8),
            "direction_raw": struct.unpack_from("<b", blob, 0x100)[0],
            "endpoint_raw": struct.unpack_from("<b", blob, 0x101)[0],
            "alternate_endpoint_raw": struct.unpack_from("<b", blob, 0x102)[0],
            "finished_raw": _blob_u8(blob, 0x107),
        },
    }
