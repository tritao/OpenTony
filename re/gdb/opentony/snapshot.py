"""Raw object snapshots and differential views for GDB experiments."""

from __future__ import annotations

import struct
from dataclasses import dataclass

import gdb

from .memory import Memory, mem


@dataclass(frozen=True)
class Snapshot:
    name: str
    address: int
    data: bytes

    @property
    def size(self) -> int:
        return len(self.data)


@dataclass(frozen=True)
class DiffEntry:
    offset: int
    before: bytes
    after: bytes

    @property
    def before_u32(self) -> int | None:
        return struct.unpack("<I", self.before)[0] if len(self.before) == 4 else None

    @property
    def after_u32(self) -> int | None:
        return struct.unpack("<I", self.after)[0] if len(self.after) == 4 else None

    @property
    def before_f32(self) -> float | None:
        return struct.unpack("<f", self.before)[0] if len(self.before) == 4 else None

    @property
    def after_f32(self) -> float | None:
        return struct.unpack("<f", self.after)[0] if len(self.after) == 4 else None


class SnapshotStore:
    def __init__(self):
        self._snapshots: dict[str, Snapshot] = {}

    def capture(
        self,
        name: str,
        address: int,
        size: int,
        *,
        memory: Memory | None = None,
        overwrite: bool = False,
    ) -> Snapshot:
        if not name:
            raise gdb.GdbError("snapshot name must not be empty")
        if size <= 0:
            raise gdb.GdbError("snapshot size must be positive")
        if name in self._snapshots and not overwrite:
            raise gdb.GdbError(f"snapshot {name!r} already exists; add --force if intended")
        snapshot = Snapshot(name, address, (memory or mem).bytes(address, size))
        self._snapshots[name] = snapshot
        return snapshot

    def get(self, name: str) -> Snapshot:
        try:
            return self._snapshots[name]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown snapshot {name!r}") from exc

    def diff(self, before: str, after: str) -> list[DiffEntry]:
        left = self.get(before)
        right = self.get(after)
        if left.size != right.size:
            raise gdb.GdbError(f"snapshot sizes differ: {left.size} versus {right.size}")
        entries = []
        for offset in range(0, left.size, 4):
            before_bytes = left.data[offset:offset + 4]
            after_bytes = right.data[offset:offset + 4]
            if before_bytes != after_bytes:
                entries.append(DiffEntry(offset, before_bytes, after_bytes))
        return entries


snapshots = SnapshotStore()


def format_diff(entries: list[DiffEntry]) -> list[str]:
    """Format raw changes first, with heuristic interpretations beside them."""

    lines = []
    for entry in entries:
        line = f"+0x{entry.offset:04x}  {entry.before.hex()} -> {entry.after.hex()}"
        if entry.before_u32 is not None:
            line += (
                f"  | u32 {entry.before_u32} -> {entry.after_u32};"
                f" f32 {entry.before_f32!r} -> {entry.after_f32!r}"
            )
        lines.append(line)
    return lines
