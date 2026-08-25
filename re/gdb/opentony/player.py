"""Conservative candidate view of the THPS2 player object."""

from __future__ import annotations

from .knowledge import GLOBALS
from .memory import Memory, Vec3, mem


class PlayerView:
    """Expose only fields that currently have explicit candidate semantics."""

    POSITION_OFFSET = 0x08
    VELOCITY_OFFSET = 0xBC
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
    def candidate_position(self) -> Vec3:
        return self.memory.vec3(self.address + self.POSITION_OFFSET)

    @property
    def candidate_velocity(self) -> Vec3:
        return self.memory.vec3(self.address + self.VELOCITY_OFFSET)

    @property
    def physics_state(self) -> int:
        return self.memory.u32(self.address + self.PHYSICS_STATE_OFFSET)

    @property
    def unknown_state(self) -> int:
        return self.memory.u32(self.address + self.UNKNOWN_STATE_OFFSET)
