"""Little-endian typed access to the inferior process memory."""

from __future__ import annotations

import struct
from typing import NamedTuple

import gdb


class Float32Bits(NamedTuple):
    value: float
    bits: int


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
