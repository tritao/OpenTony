"""Supported view of the THPS2 player object."""

from __future__ import annotations

from .knowledge import GLOBALS
from .memory import FixedVec3, Memory, mem


class PlayerView:
    """Expose the currently supported representation of the player fields.

    Unknown offsets should still be inspected through ``Memory.word32``. This
    view intentionally exposes only representations supported by runtime
    evidence.
    """

    POSITION_OFFSET = 0x08
    POSITION_HISTORY_OFFSET = 0xBC
    PHYSICS_STATE_OFFSET = 0x30B8
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
    def position_history(self) -> FixedVec3:
        return self.memory.fixed_vec3(self.address + self.POSITION_HISTORY_OFFSET)

    @property
    def position_history_raw(self) -> tuple[int, int, int]:
        return self.memory.u32_vec3(self.address + self.POSITION_HISTORY_OFFSET)

    @property
    def physics_state(self) -> int:
        return self.memory.u32(self.address + self.PHYSICS_STATE_OFFSET)

    @property
    def unknown_state(self) -> int:
        return self.memory.u32(self.address + self.UNKNOWN_STATE_OFFSET)
