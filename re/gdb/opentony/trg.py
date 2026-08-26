"""Bounded runtime observations for TRG-created type-192 objects."""

from __future__ import annotations

import gdb

from .breakpoint import Context, TonyBreakpoint


TYPE192_HANDLER = 0x004A1060
OBJECT_COMMAND_RUNNER = 0x00401520
TYPE192_VTABLE = 0x005194F8
TYPE192_RETURNS = (
    0x004A10A7,
    0x004A10F4,
    0x004A114E,
    0x004A1169,
    0x004A11A3,
    0x004A11CE,
    0x004A11F9,
    0x004A1224,
    0x004A1232,
)

COMMAND_NAMES = {
    0x2123: "byte_to_16e",
    0x2124: "model_index",
    0x2127: "vec_70",
    0x2128: "resolved_vec_4c",
    0x212F: "model_checksum",
    0x2133: "angles_14",
    0x2136: "vec_76",
}


def _u16_vec(address: int, memory) -> list[int]:
    return [memory.u16(address + offset) for offset in (0, 2, 4)]


def _u32_vec(address: int, memory) -> list[int]:
    return [memory.u32(address + offset) for offset in (0, 4, 8)]


def _object_snapshot(address: int, memory) -> dict | None:
    if not address or not memory.readable(address, 0x180):
        return None
    cursor = memory.u32(address + 0x17C)
    return {
        "address": f"0x{address:08x}",
        "flags": memory.u16(address + 0x04),
        "position_raw": _u32_vec(address + 0x08, memory),
        "angles_u16": _u16_vec(address + 0x14, memory),
        "model_index": memory.u16(address + 0x1A),
        "region": memory.u8(address + 0x1F),
        "byte_16e": memory.u8(address + 0x16E),
        "resolved_vec_4c": _u32_vec(address + 0x4C, memory),
        "vec_70": _u16_vec(address + 0x70, memory),
        "vec_76": _u16_vec(address + 0x76, memory),
        "cursor": f"0x{cursor:08x}" if cursor else None,
    }


def _cursor_preview(address: int, memory, size: int = 16) -> str | None:
    if not address or not memory.readable(address, size):
        return None
    return memory.bytes(address, size).hex()


class Type192CommandProbe:
    """Pair the type-192 command handler with its internal return sites."""

    def __init__(self, count: int | None = None, writer=None):
        self.remaining = count
        self.writer = writer
        self.hits = 0
        self.runner_hits = 0
        self.runner_limit = count * 4 if count is not None else None
        self._entry = _Type192Entry(self)
        self._runner = _Type192Runner(self)
        self._returns = [_Type192Return(self, address) for address in TYPE192_RETURNS]
        self._active: dict[str, object] | None = None

    @property
    def breakpoints(self):
        return (self._runner, self._entry, *self._returns)

    def _emit(self, record: dict) -> None:
        if self.writer is None:
            TonyBreakpoint.emit(record)
        else:
            self.writer.event(record)

    def begin(self, ctx: Context) -> None:
        # The handler copies ECX to ESI immediately after entry; at the entry
        # breakpoint ECX is the authoritative this pointer.
        object_address = ctx.this_ptr()
        before = _object_snapshot(object_address, ctx.memory)
        if before is None:
            return
        command = ctx.arg(0)
        cursor = int(before["cursor"], 16) if before["cursor"] else 0
        self._active = {
            "object": object_address,
            "command": command,
            "frame": ctx.frame,
            "caller": ctx.caller(),
            "before": before,
            "cursor_before": cursor,
            "stream_before": _cursor_preview(cursor, ctx.memory),
        }

    def runner(self, ctx: Context) -> None:
        object_address = ctx.this_ptr()
        if not object_address or not ctx.memory.readable(object_address, 4):
            return
        if ctx.memory.u32(object_address) != TYPE192_VTABLE:
            return
        stream = ctx.arg(0)
        command = ctx.memory.u16(stream) if stream and ctx.memory.readable(stream, 2) else None
        record = {
            "type": "trg_type192_runner",
            "function": "TriggerObjectCommandRunner",
            "address": f"0x{OBJECT_COMMAND_RUNNER:08x}",
            "frame": ctx.frame,
            "caller": f"0x{ctx.caller():08x}",
            "object": f"0x{object_address:08x}",
            "stream": f"0x{stream:08x}" if stream else None,
            "command_raw": command,
            "command": command & 0xFFFF if command is not None else None,
            "command_name": COMMAND_NAMES.get(command & 0xFFFF)
            if command is not None
            else None,
            "object_before": _object_snapshot(object_address, ctx.memory),
        }
        self._emit(record)
        self.runner_hits += 1
        if self.remaining is not None and self.remaining <= 0:
            self._runner.enabled = False
        elif self.runner_limit is not None and self.runner_hits >= self.runner_limit:
            self._runner.enabled = False

    def finish(self, ctx: Context) -> None:
        active = self._active
        self._active = None
        if active is None:
            return
        object_address = int(active["object"])
        after = _object_snapshot(object_address, ctx.memory)
        if after is None:
            return
        command = int(active["command"])
        cursor_after = int(after["cursor"], 16) if after["cursor"] else 0
        record = {
            "type": "trg_type192_command",
            "function": "TriggerObject192Command",
            "address": f"0x{TYPE192_HANDLER:08x}",
            "return_pc": f"0x{ctx.eip:08x}",
            "caller": f"0x{int(active['caller']):08x}",
            "frame": active["frame"],
            "object": f"0x{object_address:08x}",
            "command_raw": command,
            "command": command & 0xFFFF,
            "command_name": COMMAND_NAMES.get(command & 0xFFFF),
            "cursor_before": f"0x{int(active['cursor_before']):08x}",
            "cursor_after": f"0x{cursor_after:08x}",
            "cursor_delta": cursor_after - int(active["cursor_before"]),
            "stream_before": active["stream_before"],
            "stream_after": _cursor_preview(cursor_after, ctx.memory),
            "object_before": active["before"],
            "object_after": after,
            "return_value": ctx.register("eax"),
        }
        self._emit(record)
        self.hits += 1
        if self.remaining is not None:
            self.remaining -= 1
            if self.remaining <= 0:
                self._runner.enabled = False
                self._entry.enabled = False
                for breakpoint in self._returns:
                    breakpoint.enabled = False


class _Type192Entry(TonyBreakpoint):
    def __init__(self, owner: Type192CommandProbe):
        self.owner = owner
        super().__init__(TYPE192_HANDLER, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.begin(ctx)


class _Type192Runner(TonyBreakpoint):
    def __init__(self, owner: Type192CommandProbe):
        self.owner = owner
        super().__init__(OBJECT_COMMAND_RUNNER, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.runner(ctx)


class _Type192Return(TonyBreakpoint):
    def __init__(self, owner: Type192CommandProbe, address: int):
        self.owner = owner
        super().__init__(address, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.owner.finish(ctx)
