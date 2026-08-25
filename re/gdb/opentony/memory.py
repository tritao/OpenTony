"""Little-endian typed access to the inferior process memory."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import NamedTuple

import gdb


class Float32Bits(NamedTuple):
    value: float
    bits: int


class Word32(NamedTuple):
    """The useful candidate interpretations of one raw 32-bit word."""

    u32: int
    s32: int
    fixed16: float
    f32: float


@dataclass(frozen=True, slots=True)
class Fixed16:
    """One 32-bit word represented as signed 16.16 fixed point."""

    raw: int

    @property
    def signed(self) -> int:
        return self.raw - (1 << 32) if self.raw & 0x80000000 else self.raw

    @property
    def value(self) -> float:
        return self.signed / 65536.0


class FixedVec3(NamedTuple):
    x: Fixed16
    y: Fixed16
    z: Fixed16

    @property
    def raw(self) -> tuple[int, int, int]:
        return self.x.raw, self.y.raw, self.z.raw

    @property
    def signed(self) -> tuple[int, int, int]:
        return self.x.signed, self.y.signed, self.z.signed

    @property
    def values(self) -> tuple[float, float, float]:
        return self.x.value, self.y.value, self.z.value


class Vec3(NamedTuple):
    x: float
    y: float
    z: float


class Memory:
    """Typed memory access for the 32-bit THPS2 process."""

    pointer_size = 4

    def __init__(self, inferior=None):
        self._inferior = inferior

    @property
    def inferior(self):
        return self._inferior if self._inferior is not None else gdb.selected_inferior()

    def bytes(self, address: int, size: int) -> bytes:
        if address < 0:
            raise gdb.GdbError("address must be non-negative")
        if size < 0:
            raise gdb.GdbError("size must be non-negative")
        try:
            return bytes(self.inferior.read_memory(address, size))
        except gdb.error as exc:
            raise gdb.GdbError(f"could not read {size} bytes at 0x{address:08x}: {exc}") from exc

    def write(self, address: int, data: bytes | bytearray | memoryview) -> None:
        if address < 0:
            raise gdb.GdbError("address must be non-negative")
        try:
            self.inferior.write_memory(address, bytes(data))
        except gdb.error as exc:
            raise gdb.GdbError(f"could not write {len(data)} bytes at 0x{address:08x}: {exc}") from exc

    def _unpack(self, fmt: str, address: int):
        size = struct.calcsize(fmt)
        return struct.unpack(fmt, self.bytes(address, size))[0]

    def u8(self, address: int) -> int:
        return self._unpack("<B", address)

    def u16(self, address: int) -> int:
        return self._unpack("<H", address)

    def u32(self, address: int) -> int:
        return self._unpack("<I", address)

    def s8(self, address: int) -> int:
        return self._unpack("<b", address)

    def s16(self, address: int) -> int:
        return self._unpack("<h", address)

    def s32(self, address: int) -> int:
        return self._unpack("<i", address)

    def f32(self, address: int) -> float:
        return self._unpack("<f", address)

    def fixed16(self, address: int) -> float:
        return self.s32(address) / 65536.0

    def u32_vec3(self, address: int) -> tuple[int, int, int]:
        return struct.unpack("<3I", self.bytes(address, 12))

    def fixed_vec3(self, address: int) -> FixedVec3:
        return FixedVec3(*(Fixed16(raw) for raw in self.u32_vec3(address)))

    def word32(self, address: int) -> Word32:
        return decode_word32(self.bytes(address, 4))

    def f32_bits(self, address: int) -> Float32Bits:
        raw = self.bytes(address, 4)
        bits = struct.unpack("<I", raw)[0]
        value = struct.unpack("<f", raw)[0]
        return Float32Bits(value=value, bits=bits)

    def ptr(self, address: int) -> int:
        return self.u32(address)

    def vec3(self, address: int) -> Vec3:
        return Vec3(*struct.unpack("<3f", self.bytes(address, 12)))

    def cstring(self, address: int, max_size: int = 4096, encoding: str = "ascii") -> str:
        if max_size <= 0:
            raise gdb.GdbError("max_size must be positive")
        raw = self.bytes(address, max_size)
        end = raw.find(b"\0")
        if end < 0:
            raise gdb.GdbError(f"unterminated string at 0x{address:08x} within {max_size} bytes")
        return raw[:end].decode(encoding, errors="replace")

    def write_u8(self, address: int, value: int) -> None:
        self.write(address, struct.pack("<B", value))

    def write_u16(self, address: int, value: int) -> None:
        self.write(address, struct.pack("<H", value))

    def write_u32(self, address: int, value: int) -> None:
        self.write(address, struct.pack("<I", value))

    def write_s32(self, address: int, value: int) -> None:
        self.write(address, struct.pack("<i", value))

    def write_f32(self, address: int, value: float) -> None:
        self.write(address, struct.pack("<f", value))

    def write_vec3(self, address: int, value: Vec3 | tuple[float, float, float]) -> None:
        self.write(address, struct.pack("<3f", *value))

    def readable(self, address: int, size: int = 1) -> bool:
        if address < 0 or size < 0:
            return False
        try:
            self.inferior.read_memory(address, size)
        except gdb.error:
            return False
        return True

    def valid(self, pointer: int) -> bool:
        return pointer != 0 and self.readable(pointer)


mem = Memory()


def decode_word32(data: bytes) -> Word32:
    """Decode one word without selecting a semantic representation."""

    if len(data) != 4:
        raise ValueError(f"expected exactly 4 bytes, got {len(data)}")
    u32 = struct.unpack("<I", data)[0]
    s32 = struct.unpack("<i", data)[0]
    f32 = struct.unpack("<f", data)[0]
    return Word32(u32=u32, s32=s32, fixed16=s32 / 65536.0, f32=f32)
