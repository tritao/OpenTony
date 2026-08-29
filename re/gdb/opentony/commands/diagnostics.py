"""Memory, snapshot, watchpoint, and trace commands."""

from __future__ import annotations

import shlex
from pathlib import Path

import gdb

from ..breakpoint import TonyBreakpoint
from ..frame import FrameBreakpoint, frame_clock
from ..memory import mem
from ..player import PlayerView
from ..snapshot import format_diff, snapshots
from ..trace import JsonlWriter
from ..watchpoint import watchpoints
from .common import (
    argv,
    integer,
    runtime_breakpoints,
    set_trace_writer,
    trace_writer,
    write,
)
from .knowledge import PLAYER_WATCH_FIELDS, THPS2_ADDRESSES, THPS2_BUILD_SHA256

_WATCH_DEFAULT_LIMIT = 256


def _snapshot_address(value: str) -> int:
    try:
        return integer(value)
    except gdb.GdbError:
        if value.casefold() == "player":
            player = PlayerView.current()
            if player is not None:
                return player.address
        raise


def _watch_address(value: str) -> tuple[int, str]:
    """Resolve a raw address or a small candidate PlayerView expression."""

    lowered = value.casefold()
    if lowered == "player" or lowered.startswith(("player+", "player.")):
        player = PlayerView.current()
        if player is None:
            raise gdb.GdbError("the generated Player pointer is null or unreadable")
        if lowered == "player":
            return player.address, value
        if lowered.startswith("player+"):
            try:
                offset = int(value[7:], 0)
            except ValueError as exc:
                raise gdb.GdbError(f"invalid player offset {value!r}") from exc
            if offset < 0:
                raise gdb.GdbError("player watch offset must be non-negative")
            return player.address + offset, value
        field = lowered[7:]
        try:
            return player.address + PLAYER_WATCH_FIELDS[field], value
        except KeyError as exc:
            fields = ", ".join(sorted(PLAYER_WATCH_FIELDS))
            raise gdb.GdbError(f"unknown PlayerView field {field!r}; choose one of: {fields}") from exc
    return integer(value), value


def _action_state(address: int) -> dict:
    raw = mem.bytes(address, 0x10)
    return {
        "address": f"0x{address:08x}",
        "raw": raw.hex(),
        "byte0": raw[0],
        "byte1": raw[1],
        "u32_4": int.from_bytes(raw[4:8], "little"),
        "u32_8": int.from_bytes(raw[8:12], "little"),
        "u32_c": int.from_bytes(raw[12:16], "little"),
    }


class TonyReadInteger(gdb.Command):
    def __init__(self, name: str, reader, label: str):
        super().__init__(name, gdb.COMMAND_DATA)
        self.command_name = name
        self.reader = reader
        self.label = label

    def invoke(self, arg, from_tty):
        values = argv(arg, f"{self.command_name} ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError(f"usage: {self.command_name} ADDRESS")
        address = integer(values[0])
        value = self.reader(address)
        write(f"0x{address:08x}: {self.label} {value} (0x{value:x})")


class TonyReadFloat(gdb.Command):
    """Read a little-endian float32 from inferior memory."""

    def __init__(self):
        super().__init__("tony-readf", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-readf ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-readf ADDRESS")
        address = integer(values[0])
        value = mem.f32(address)
        write(f"0x{address:08x}: float32 {value!r}")


def _hexdump(address: int, data: bytes, width: int = 16) -> str:
    lines = []
    for offset in range(0, len(data), width):
        chunk = data[offset : offset + width]
        hex_part = " ".join(f"{byte:02x}" for byte in chunk)
        hex_part = f"{hex_part:<{width * 3 - 1}}"
        ascii_part = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
        lines.append(f"0x{address + offset:08x}:  {hex_part}  |{ascii_part}|")
    return "\n".join(lines)


class TonyHexdump(gdb.Command):
    """tony-hexdump ADDRESS LENGTH [WIDTH] -- print inferior memory."""

    def __init__(self):
        super().__init__("tony-hexdump", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-hexdump ADDRESS LENGTH [WIDTH]")
        if len(values) not in (2, 3):
            raise gdb.GdbError("usage: tony-hexdump ADDRESS LENGTH [WIDTH]")
        address = integer(values[0])
        length = integer(values[1])
        width = integer(values[2]) if len(values) == 3 else 16
        if length < 0 or width <= 0 or width > 64:
            raise gdb.GdbError("LENGTH must be non-negative and WIDTH must be between 1 and 64")
        output = _hexdump(address, mem.bytes(address, length), width)
        if output:
            gdb.write(output + "\n")


class TonyDump(gdb.Command):
    """tony-dump ADDRESS LENGTH FILE [--force] -- save raw inferior memory."""

    def __init__(self):
        super().__init__("tony-dump", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-dump ADDRESS LENGTH FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-dump ADDRESS LENGTH FILE [--force]")
        address = integer(values[0])
        length = integer(values[1])
        if length < 0:
            raise gdb.GdbError("LENGTH must be non-negative")
        path = Path(values[2]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(mem.bytes(address, length))
        except OSError as exc:
            raise gdb.GdbError(f"could not write {path}: {exc}") from exc
        write(f"wrote {length} bytes from 0x{address:08x} to {path}")


class TonySnapshot(gdb.Command):
    """tony-snapshot NAME ADDRESS SIZE [--force] -- capture an in-session raw snapshot."""

    def __init__(self):
        super().__init__("tony-snapshot", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-snapshot NAME ADDRESS SIZE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-snapshot NAME ADDRESS SIZE [--force]")
        name = values[0]
        address = _snapshot_address(values[1])
        size = integer(values[2])
        snapshot = snapshots.capture(name, address, size, overwrite=force)
        write(f"captured snapshot {name} at 0x{snapshot.address:08x} ({snapshot.size} bytes)")


class TonyDiff(gdb.Command):
    """tony-diff BEFORE AFTER -- show changed raw words and heuristic interpretations."""

    def __init__(self):
        super().__init__("tony-diff", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-diff BEFORE AFTER")
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-diff BEFORE AFTER")
        before, after = values
        entries = snapshots.diff(before, after)
        write(f"{before} -> {after}: {len(entries)} changed words")
        for line in format_diff(entries):
            write(line)
        if not entries:
            write("no changes")


class TonyModules(gdb.Command):
    """tony-modules -- show loaded module ranges from GDB/Wine."""

    def __init__(self):
        super().__init__("tony-modules", gdb.COMMAND_FILES)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-modules")
        main = gdb.current_progspace().filename
        if main:
            write(f"main: {main}")
        output = gdb.execute("info sharedlibrary", to_string=True)
        gdb.write(output)


def _set_breakpoint(address: int, temporary: bool = False) -> None:
    breakpoint = TonyBreakpoint(address, temporary=temporary)
    kind = "temporary breakpoint" if temporary else "breakpoint"
    write(f"set {kind} {breakpoint.number} at 0x{address:08x}")


class TonyBreakpointCommand(gdb.Command):
    """tony-bp ADDRESS [temporary] -- set an address breakpoint."""

    def __init__(self):
        super().__init__("tony-bp", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-bp ADDRESS [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp ADDRESS [temporary]")
        _set_breakpoint(integer(values[0]), len(values) == 2)


class TonyAddresses(gdb.Command):
    """tony-thps2 [NAME] -- show known addresses for this THPS2 build."""

    def __init__(self):
        super().__init__("tony-thps2", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-thps2 [NAME]")
        if not values:
            write(f"THPS2 build SHA-256: {THPS2_BUILD_SHA256}")
            for name, (address, description, evidence) in THPS2_ADDRESSES.items():
                write(f"{name:<16} 0x{address:08x}  {description} [{evidence}]")
            return
        name = values[0]
        try:
            address, description, evidence = THPS2_ADDRESSES[name]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {name!r}; run tony-thps2") from exc
        write(f"{name}: 0x{address:08x}  {description} [{evidence}]")


class TonyTHPS2Breakpoint(gdb.Command):
    """tony-bp-thps2 NAME [temporary] -- break at a known THPS2 address."""

    def __init__(self):
        super().__init__("tony-bp-thps2", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-bp-thps2 NAME [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp-thps2 NAME [temporary]")
        try:
            address = THPS2_ADDRESSES[values[0]][0]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {values[0]!r}; run tony-thps2") from exc
        _set_breakpoint(address, len(values) == 2)


def _watch_limit(values: list[str], usage: str, *, default_limit: int | None) -> tuple[list[str], int | None]:
    limit = default_limit
    if "--limit" in values:
        if values.count("--limit") != 1:
            raise gdb.GdbError(f"usage: {usage}")
        index = values.index("--limit")
        if index + 1 >= len(values):
            raise gdb.GdbError(f"usage: {usage}")
        limit = integer(values[index + 1])
        values[index : index + 2] = []
        if limit <= 0:
            raise gdb.GdbError("LIMIT must be positive")
    return values, limit


def _watch_arguments(arg: str, usage: str, *, default_limit: int | None) -> tuple[int, str, int, int | None]:
    values, limit = _watch_limit(argv(arg, usage), usage, default_limit=default_limit)
    if len(values) not in (1, 2):
        raise gdb.GdbError(f"usage: {usage}")
    address, label = _watch_address(values[0])
    size = integer(values[1]) if len(values) == 2 else 4
    if size not in (1, 2, 4):
        raise gdb.GdbError("SIZE must be 1, 2, or 4 bytes")
    return address, label, size, limit


def _arm_resolved_watchpoint(
    address: int,
    label: str,
    size: int,
    *,
    once: bool,
    limit: int | None,
):
    watchpoint = watchpoints.arm(
        address,
        size=size,
        label=label,
        once=once,
        limit=limit,
        writer=trace_writer(),
    )
    mode = "one-shot " if once else ""
    number = getattr(watchpoint, "number", "?")
    limit_text = "" if once else f"; limit {limit} events"
    write(f"{mode}write watchpoint {number} armed at {label} (0x{address:08x}, {size} bytes{limit_text})")
    return watchpoint


def _arm_watchpoint(arg: str, *, once: bool, command: str):
    address, label, size, limit = _watch_arguments(
        arg,
        f"{command} ADDRESS [SIZE] [--limit COUNT]",
        default_limit=None if once else _WATCH_DEFAULT_LIMIT,
    )
    return _arm_resolved_watchpoint(address, label, size, once=once, limit=limit)


class TonyWatch(gdb.Command):
    """tony-watch ADDRESS [SIZE] [--limit COUNT] -- bounded write logging."""

    def __init__(self):
        super().__init__("tony-watch", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        _arm_watchpoint(arg, once=False, command="tony-watch")


class TonyWatchOnce(gdb.Command):
    """tony-watch-once ADDRESS [SIZE] -- record the next write and continue."""

    def __init__(self):
        super().__init__("tony-watch-once", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        _arm_watchpoint(arg, once=True, command="tony-watch-once")


class TonyWatchBatch(gdb.Command):
    """tony-watch-batch ADDRESS... [--limit COUNT] -- arm a safe four-watch group."""

    def __init__(self):
        super().__init__("tony-watch-batch", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        usage = "tony-watch-batch ADDRESS... [--limit COUNT]"
        values, limit = _watch_limit(argv(arg, usage), usage, default_limit=_WATCH_DEFAULT_LIMIT)
        if not values:
            raise gdb.GdbError(f"usage: {usage}")
        available = watchpoints.available()
        if len(values) > available:
            raise gdb.GdbError(
                f"watch batch needs {len(values)} hardware slots, but only {available} are available; "
                "split the experiment into groups of four"
            )
        for value in values:
            address, label = _watch_address(value)
            _arm_resolved_watchpoint(address, label, 4, once=False, limit=limit)


class TonyWatchLog(gdb.Command):
    """tony-watch-log [ADDRESS [SIZE] [--limit COUNT]] -- list or arm bounded watches."""

    def __init__(self):
        super().__init__("tony-watch-log", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        if arg.strip():
            _arm_watchpoint(arg, once=False, command="tony-watch-log")
            return
        active = watchpoints.active()
        if not active:
            write("no active OpenTony watchpoints")
            return
        write(
            f"{len(active)} active OpenTony watchpoint(s); "
            f"{watchpoints.available()} hardware slot(s) available"
        )
        for watchpoint in active:
            number = getattr(watchpoint, "number", "?")
            mode = "one-shot" if watchpoint.once else f"limit {watchpoint.limit}"
            write(
                f"  {number}: {watchpoint.label} @ 0x{watchpoint.address:08x}"
                f" ({watchpoint.size} bytes; {mode}{'; latched' if watchpoint.latched else ''})"
            )


class TonyWatchClear(gdb.Command):
    """tony-watch-clear -- disable all managed hardware watchpoints while stopped."""

    def __init__(self):
        super().__init__("tony-watch-clear", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-watch-clear")
        active = watchpoints.active()
        for watchpoint in active:
            watchpoint.enabled = False
        write(f"disabled {len(active)} OpenTony hardware watchpoint(s)")


class TonyTraceOpen(gdb.Command):
    """tony-trace-open FILE EXPERIMENT [--force] -- start a JSONL runtime trace."""

    def __init__(self):
        super().__init__("tony-trace-open", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-trace-open FILE EXPERIMENT [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-trace-open FILE EXPERIMENT [--force]")
        if trace_writer() is not None:
            raise gdb.GdbError("a runtime trace is already open; run tony-trace-close first")
        writer = JsonlWriter(values[0], values[1], overwrite=force)
        try:
            writer.open()
        except OSError as exc:
            raise gdb.GdbError(str(exc)) from exc
        set_trace_writer(writer)
        write(f"runtime trace opened: {writer.path}")


class TonyTraceClose(gdb.Command):
    """tony-trace-close -- write the trace footer and close the active writer."""

    def __init__(self):
        super().__init__("tony-trace-close", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-trace-close")
        if trace_writer() is None:
            write("no runtime trace is open")
            return
        writer = trace_writer()
        for breakpoint in runtime_breakpoints:
            if getattr(breakpoint, "writer", None) is writer:
                disable_pending_returns = getattr(breakpoint, "disable_pending_returns", None)
                if disable_pending_returns is not None:
                    disable_pending_returns()
                breakpoint.enabled = False
        watchpoints.disable_writer(writer)
        try:
            writer.close()
        except OSError as exc:
            raise gdb.GdbError(str(exc)) from exc
        set_trace_writer(None)
        write(f"runtime trace closed: {writer.path}")


class TonyFrameClock(gdb.Command):
    """tony-frame-clock FUNCTION -- tick the shared clock at a chosen function."""

    def __init__(self):
        super().__init__("tony-frame-clock", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-frame-clock FUNCTION")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-frame-clock FUNCTION")
        frame_clock.reset()
        try:
            breakpoint = FrameBreakpoint(values[0], internal=True, writer=trace_writer())
        except KeyError as exc:
            raise gdb.GdbError(f"unknown function {values[0]!r}; run tony-thps2") from exc
        runtime_breakpoints.append(breakpoint)
        write(f"frame clock armed at {values[0]} (frame starts at 0)")
