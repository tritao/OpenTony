"""Conservative candidate view of the THPS2 player object."""

from __future__ import annotations

from .knowledge import GLOBALS
from .memory import Memory, mem


class PlayerView:
    """Expose raw player words and competing candidate interpretations.

    The object layout is still provisional.  In particular, callers must not
    treat the float32 view as established merely because it is convenient to
    consume.  The ``*_raw`` properties are the authoritative observations.
    """

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

    def _vec3_words(self, offset: int):
        return tuple(self.memory.word32(self.address + offset + index * 4) for index in range(3))

    @property
    def position_raw(self) -> tuple[int, int, int]:
        """The exact three little-endian words, represented as unsigned u32."""

        return tuple(word.u32 for word in self._vec3_words(self.POSITION_OFFSET))

    @property
    def position_signed(self) -> tuple[int, int, int]:
        return tuple(word.s32 for word in self._vec3_words(self.POSITION_OFFSET))

    @property
    def position_fixed16(self) -> tuple[float, float, float]:
        return tuple(word.fixed16 for word in self._vec3_words(self.POSITION_OFFSET))

    @property
    def position_float(self) -> tuple[float, float, float]:
        return tuple(word.f32 for word in self._vec3_words(self.POSITION_OFFSET))

    @property
    def velocity_raw(self) -> tuple[int, int, int]:
        return tuple(word.u32 for word in self._vec3_words(self.VELOCITY_OFFSET))

    @property
    def velocity_signed(self) -> tuple[int, int, int]:
        return tuple(word.s32 for word in self._vec3_words(self.VELOCITY_OFFSET))

    @property
    def velocity_fixed16(self) -> tuple[float, float, float]:
        return tuple(word.fixed16 for word in self._vec3_words(self.VELOCITY_OFFSET))

    @property
    def velocity_float(self) -> tuple[float, float, float]:
        return tuple(word.f32 for word in self._vec3_words(self.VELOCITY_OFFSET))

    def _scalar(self, offset: int):
        return self.memory.word32(self.address + offset)

    @property
    def physics_state_raw(self) -> int:
        return self._scalar(self.PHYSICS_STATE_OFFSET).u32

    @property
    def physics_state_signed(self) -> int:
        return self._scalar(self.PHYSICS_STATE_OFFSET).s32

    @property
    def physics_state_fixed16(self) -> float:
        return self._scalar(self.PHYSICS_STATE_OFFSET).fixed16

    @property
    def physics_state_float(self) -> float:
        return self._scalar(self.PHYSICS_STATE_OFFSET).f32

    @property
    def physics_state(self) -> int:
        """Compatibility alias for the raw state word."""

        return self.physics_state_raw

    @property
    def unknown_state_raw(self) -> int:
        return self._scalar(self.UNKNOWN_STATE_OFFSET).u32

    @property
    def unknown_state_signed(self) -> int:
        return self._scalar(self.UNKNOWN_STATE_OFFSET).s32

    @property
    def unknown_state_fixed16(self) -> float:
        return self._scalar(self.UNKNOWN_STATE_OFFSET).fixed16

    @property
    def unknown_state_float(self) -> float:
        return self._scalar(self.UNKNOWN_STATE_OFFSET).f32

    @property
    def unknown_state(self) -> int:
        """Compatibility alias for the raw state word."""

        return self.unknown_state_raw
