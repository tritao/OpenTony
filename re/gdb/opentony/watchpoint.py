"""Managed write watchpoints with structured old/new value logging."""

from __future__ import annotations

import json

import gdb

from .breakpoint import Context
from .knowledge import function_name_at
from .memory import Memory, decode_word32, mem


def _typed_values(data: bytes) -> dict[str, int | float] | None:
    if len(data) != 4:
        return None
    word = decode_word32(data)
    return word._asdict()


def _caller_address() -> int | None:
    try:
        frame = gdb.newest_frame()
        caller = frame.older() if frame is not None else None
        return int(caller.pc()) if caller is not None else None
    except (gdb.error, AttributeError, TypeError, ValueError):
        return None


class TonyWatchpoint(gdb.Breakpoint):
    """Hardware write watchpoint with bounded logging and safe auto-continue."""

    def __init__(
        self,
        address: int,
        *,
        size: int = 4,
        label: str | None = None,
        once: bool = False,
        limit: int | None = None,
        memory: Memory | None = None,
        writer=None,
    ):
        if size not in (1, 2, 4):
            raise gdb.GdbError("watchpoint size must be 1, 2, or 4 bytes")
        if limit is not None and limit <= 0:
            raise gdb.GdbError("watchpoint event limit must be positive")
        self.address = address
        self.size = size
        self.label = label or f"0x{address:08x}"
        self.once = once
        self.limit = limit
        self.events = 0
        self.latched = False
        self.memory = memory or mem
        self.writer = writer
        self.previous = self.memory.bytes(address, size)
        expression = self._expression(address, size)
        super().__init__(expression, gdb.BP_WATCHPOINT, wp_class=gdb.WP_WRITE, internal=True)

    @staticmethod
    def _expression(address: int, size: int) -> str:
        types = {1: "unsigned char", 2: "unsigned short", 4: "unsigned int"}
        return f"*({types[size]}*)0x{address:x}"

    def record(self, ctx: Context) -> dict:
        current = self.memory.bytes(self.address, self.size)
        before = self.previous
        self.previous = current
        record = {
            "type": "watchpoint",
            "frame": ctx.frame,
            "address": f"0x{self.address:08x}",
            "label": self.label,
            "size": self.size,
            "old_raw": before.hex(),
            "new_raw": current.hex(),
            "eip": f"0x{ctx.eip:08x}",
            "function": function_name_at(ctx.eip),
        }
        caller = _caller_address()
        if caller is not None:
            record["caller"] = f"0x{caller:08x}"
        before_typed = _typed_values(before)
        after_typed = _typed_values(current)
        if before_typed is not None:
            record["old"] = before_typed
            record["new"] = after_typed
        return record

    def stop(self):
        # WineDbg's GDB proxy is unstable when a live hardware watchpoint is
        # removed as part of handling its own trap.  Keep the debug register
        # installed and simply stop recording once the requested bound is
        # reached.  The explicit clear command can disable it while stopped.
        if self.latched:
            return False
        record = self.record(Context.capture(self.memory))
        if self.writer is None:
            gdb.write(json.dumps(record, sort_keys=True) + "\n")
        else:
            self.writer.event(record)
        self.events += 1
        if self.once or (self.limit is not None and self.events >= self.limit):
            self.latched = True
        return False


class WatchpointManager:
    HARDWARE_SLOTS = 4

    def __init__(self):
        self.watchpoints: list[TonyWatchpoint] = []

    def available(self) -> int:
        return max(0, self.HARDWARE_SLOTS - len(self.active()))

    def arm(
        self,
        address: int,
        *,
        size: int = 4,
        label: str | None = None,
        once: bool = False,
        limit: int | None = None,
        memory: Memory | None = None,
        writer=None,
    ):
        if self.available() == 0:
            raise gdb.GdbError(
                "all four OpenTony hardware watchpoint slots are in use; "
                "disable an active watch or run this experiment in batches"
            )
        watchpoint = TonyWatchpoint(
            address,
            size=size,
            label=label,
            once=once,
            limit=limit,
            memory=memory,
            writer=writer,
        )
        self.watchpoints.append(watchpoint)
        return watchpoint

    def active(self) -> list[TonyWatchpoint]:
        return [
            watchpoint
            for watchpoint in self.watchpoints
            if watchpoint.is_valid() and watchpoint.enabled
        ]

    def disable_writer(self, writer) -> None:
        for watchpoint in self.watchpoints:
            if watchpoint.writer is writer:
                watchpoint.enabled = False


watchpoints = WatchpointManager()
