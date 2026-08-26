"""Compatibility commands built on the reusable GDB primitives."""

from __future__ import annotations

import json
import shlex
from pathlib import Path

import gdb

from .action import ActionMaskSequenceProbe
from .breakpoint import CountingBreakpoint, TonyBreakpoint
from .camera import (
    ActorSubmissionProbe,
    CameraCollisionProbe,
    CameraCollisionResultProbe,
    CameraEffectProbe,
    CameraPointSelectProbe,
    CameraPointStateProbe,
    CameraTimingProbe,
    CameraPositionTransformProbe,
    CameraProbe,
    GeometrySubmissionProbe,
    GeometryRasterReturnProbe,
    ViewProjectionProbe,
    ViewProjectionPerturbProbe,
)
from .frame import FrameBreakpoint, frame_clock
from .knowledge import BUILD_SHA256, GLOBALS, known_function_addresses
from .memory import mem
from .physics import PhysicsProbe, PlayerDiffProbe
from .position import POSITION_COMMIT_CALLS, PositionCommitBreakpoint
from .snapshot import format_diff, snapshots
from .trace import JsonlWriter
from .watchpoint import watchpoints

THPS2_BUILD_SHA256 = BUILD_SHA256
THPS2_ADDRESSES = dict(known_function_addresses())

THPS2_LEVELS = {
    "hangar": 0,
    "warehouse": 12,
}

ACTION_STATE_BASE = GLOBALS["InputActionStates"]
ACTION_STATE_RECORDS = {
    "left": (ACTION_STATE_BASE + 0x80, 0x8000),
    "right": (ACTION_STATE_BASE + 0x90, 0x2000),
    "up": (ACTION_STATE_BASE + 0xA0, 0x1000),
    "down": (ACTION_STATE_BASE + 0xB0, 0x4000),
}

_trace_writer = None
_WATCH_DEFAULT_LIMIT = 256


def _argv(arg: str, usage: str) -> list[str]:
    try:
        values = shlex.split(arg)
    except ValueError as exc:
        raise gdb.GdbError(f"invalid arguments: {exc}") from exc
    if not values:
        raise gdb.GdbError(f"usage: {usage}")
    return values


def _integer(value: str) -> int:
    try:
        # parse_and_eval also accepts useful GDB expressions such as $eip.
        return int(gdb.parse_and_eval(value))
    except (gdb.error, TypeError, ValueError) as exc:
        raise gdb.GdbError(f"expected an address or integer expression, got {value!r}") from exc


def _write(text: str) -> None:
    gdb.write(text + "\n")


def _snapshot_address(value: str) -> int:
    try:
        return _integer(value)
    except gdb.GdbError:
        if value.casefold() == "player":
            from .player import PlayerView

            player = PlayerView.current()
            if player is not None:
                return player.address
        raise


_PLAYER_WATCH_FIELDS = {
    "position.x": 0x08,
    "position.y": 0x0C,
    "position.z": 0x10,
    "position_history.x": 0xBC,
    "position_history.y": 0xC0,
    "position_history.z": 0xC4,
    "physics_state": 0x30B8,
    "unknown_state": 0x30C4,
}


def _watch_address(value: str) -> tuple[int, str]:
    """Resolve a raw address or a small candidate PlayerView expression."""

    lowered = value.casefold()
    if lowered == "player" or lowered.startswith(("player+", "player.")):
        from .player import PlayerView

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
            return player.address + _PLAYER_WATCH_FIELDS[field], value
        except KeyError as exc:
            fields = ", ".join(sorted(_PLAYER_WATCH_FIELDS))
            raise gdb.GdbError(f"unknown PlayerView field {field!r}; choose one of: {fields}") from exc
    return _integer(value), value


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
        values = _argv(arg, f"{self.command_name} ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError(f"usage: {self.command_name} ADDRESS")
        address = _integer(values[0])
        value = self.reader(address)
        _write(f"0x{address:08x}: {self.label} {value} (0x{value:x})")


class TonyReadFloat(gdb.Command):
    """Read a little-endian float32 from inferior memory."""

    def __init__(self):
        super().__init__("tony-readf", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-readf ADDRESS")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-readf ADDRESS")
        address = _integer(values[0])
        value = mem.f32(address)
        _write(f"0x{address:08x}: float32 {value!r}")


def _hexdump(address: int, data: bytes, width: int = 16) -> str:
    lines = []
    for offset in range(0, len(data), width):
        chunk = data[offset:offset + width]
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
        values = _argv(arg, "tony-hexdump ADDRESS LENGTH [WIDTH]")
        if len(values) not in (2, 3):
            raise gdb.GdbError("usage: tony-hexdump ADDRESS LENGTH [WIDTH]")
        address = _integer(values[0])
        length = _integer(values[1])
        width = _integer(values[2]) if len(values) == 3 else 16
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
        values = _argv(arg, "tony-dump ADDRESS LENGTH FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-dump ADDRESS LENGTH FILE [--force]")
        address = _integer(values[0])
        length = _integer(values[1])
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
        _write(f"wrote {length} bytes from 0x{address:08x} to {path}")


class TonySnapshot(gdb.Command):
    """tony-snapshot NAME ADDRESS SIZE [--force] -- capture an in-session raw snapshot."""

    def __init__(self):
        super().__init__("tony-snapshot", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-snapshot NAME ADDRESS SIZE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 3:
            raise gdb.GdbError("usage: tony-snapshot NAME ADDRESS SIZE [--force]")
        name = values[0]
        address = _snapshot_address(values[1])
        size = _integer(values[2])
        snapshot = snapshots.capture(name, address, size, overwrite=force)
        _write(f"captured snapshot {name} at 0x{snapshot.address:08x} ({snapshot.size} bytes)")


class TonyDiff(gdb.Command):
    """tony-diff BEFORE AFTER -- show changed raw words and heuristic interpretations."""

    def __init__(self):
        super().__init__("tony-diff", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-diff BEFORE AFTER")
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-diff BEFORE AFTER")
        before, after = values
        entries = snapshots.diff(before, after)
        _write(f"{before} -> {after}: {len(entries)} changed words")
        for line in format_diff(entries):
            _write(line)
        if not entries:
            _write("no changes")


class TonyModules(gdb.Command):
    """tony-modules -- show loaded module ranges from GDB/Wine."""

    def __init__(self):
        super().__init__("tony-modules", gdb.COMMAND_FILES)

    def invoke(self, arg, from_tty):
        if arg.strip():
            raise gdb.GdbError("usage: tony-modules")
        main = gdb.current_progspace().filename
        if main:
            _write(f"main: {main}")
        output = gdb.execute("info sharedlibrary", to_string=True)
        gdb.write(output)


def _set_breakpoint(address: int, temporary: bool = False) -> None:
    breakpoint = TonyBreakpoint(address, temporary=temporary)
    kind = "temporary breakpoint" if temporary else "breakpoint"
    _write(f"set {kind} {breakpoint.number} at 0x{address:08x}")


class TonyBreakpointCommand(gdb.Command):
    """tony-bp ADDRESS [temporary] -- set an address breakpoint."""

    def __init__(self):
        super().__init__("tony-bp", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-bp ADDRESS [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp ADDRESS [temporary]")
        _set_breakpoint(_integer(values[0]), len(values) == 2)


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
            _write(f"THPS2 build SHA-256: {THPS2_BUILD_SHA256}")
            for name, (address, description, evidence) in THPS2_ADDRESSES.items():
                _write(f"{name:<16} 0x{address:08x}  {description} [{evidence}]")
            return
        name = values[0]
        try:
            address, description, evidence = THPS2_ADDRESSES[name]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {name!r}; run tony-thps2") from exc
        _write(f"{name}: 0x{address:08x}  {description} [{evidence}]")


class TonyTHPS2Breakpoint(gdb.Command):
    """tony-bp-thps2 NAME [temporary] -- break at a known THPS2 address."""

    def __init__(self):
        super().__init__("tony-bp-thps2", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-bp-thps2 NAME [temporary]")
        if len(values) not in (1, 2) or (len(values) == 2 and values[1] != "temporary"):
            raise gdb.GdbError("usage: tony-bp-thps2 NAME [temporary]")
        try:
            address = THPS2_ADDRESSES[values[0]][0]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown THPS2 address {values[0]!r}; run tony-thps2") from exc
        _set_breakpoint(address, len(values) == 2)


class TonySkipMovieBreakpoint(TonyBreakpoint):
    """Return immediately from the blocking startup-movie routine."""

    def __init__(self):
        address = THPS2_ADDRESSES["movie_play"][0]
        super().__init__(address, internal=True)

    def on_hit(self, ctx):
        return_address = ctx.return_address()
        gdb.execute(f"set $eip = 0x{return_address:x}")
        gdb.execute(f"set $esp = 0x{ctx.esp + 4:x}")
        _write(f"skipped movie playback; returning to 0x{return_address:08x}")


_movie_skip_breakpoint = None


class TonySkipMovies(gdb.Command):
    """tony-skip-movies -- bypass blocking logo and intro movie playback."""

    def __init__(self):
        super().__init__("tony-skip-movies", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        global _movie_skip_breakpoint
        if arg.strip():
            raise gdb.GdbError("usage: tony-skip-movies")
        if _movie_skip_breakpoint is not None and _movie_skip_breakpoint.is_valid():
            _write("startup movie bypass is already enabled")
            return
        _movie_skip_breakpoint = TonySkipMovieBreakpoint()
        address = THPS2_ADDRESSES["movie_play"][0]
        _write(f"startup movie bypass enabled at 0x{address:08x}")


class TonyForceLevelBreakpoint(CountingBreakpoint):
    """Replace the next Front_LaunchGameLevel level argument."""

    def __init__(self, level: int, label: str):
        address = THPS2_ADDRESSES["launch_level"][0]
        super().__init__(address, count=1, internal=True, temporary=True, should_stop=True)
        self.level = level
        self.label = label

    def on_count(self, ctx):
        original = ctx.arg(0)
        mem.write_u32(ctx.esp + 4, self.level)
        _write(f"forced launch level {original} -> {self.level} ({self.label})")
        return True


class TonyForceLevel(gdb.Command):
    """tony-force-level NAME|INDEX -- replace the next level launch argument."""

    def __init__(self):
        super().__init__("tony-force-level", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-force-level NAME|INDEX")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-force-level NAME|INDEX")
        value = values[0].casefold()
        if value in THPS2_LEVELS:
            level = THPS2_LEVELS[value]
            label = value
        else:
            level = _integer(values[0])
            label = f"level-{level}"
        if not 0 <= level < 13:
            raise gdb.GdbError("THPS2 level index must be between 0 and 12")
        TonyForceLevelBreakpoint(level, label)
        _write(f"next level launch will use {level} ({label})")


class TonyPlayerSampleBreakpoint(CountingBreakpoint):
    """Collect raw player-object snapshots at level-loop entry."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["frame_tick"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        level = mem.u32(GLOBALS["CurrentLevel"])
        if player == 0 or level > 12:
            return False
        registers = {name: ctx.register(name) for name in ("eax", "ecx", "edx", "ebx", "ebp", "esi", "edi")}
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "level": level,
            "player": player,
            "owner_header": player - 0x30 if player >= 0x30 else None,
            "registers": registers,
            "owner_bytes": mem.bytes(player - 0x30, 0x30).hex() if player >= 0x30 else None,
            "player_bytes": mem.bytes(player, 0x200).hex() if player else None,
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write player sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "player_sample", **record})
        self.sample += 1
        return True

    def on_complete(self):
        _write(f"player sampling complete: {self.sample} samples -> {self.path}")


class TonyPlayerSample(gdb.Command):
    """tony-player-sample COUNT FILE [--force] -- collect raw frame-tick snapshots."""

    def __init__(self):
        super().__init__("tony-player-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-player-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-player-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare player sample {path}: {exc}") from exc
        TonyPlayerSampleBreakpoint(count, path, writer=_trace_writer)
        _write(f"player sampling armed for {count} level-loop hits -> {path}")


class TonyInputSampleBreakpoint(CountingBreakpoint):
    """Collect action and raw-keyboard state after the input update."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["gameplay_update"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        level = mem.u32(GLOBALS["CurrentLevel"])
        action_word = mem.u16(GLOBALS["ActionMask"])
        action_words = mem.u32(GLOBALS["ActionMask"])
        keyboard_state = mem.bytes(GLOBALS["KeyboardState"], 0x100)
        action_states = {
            name: _action_state(address) for name, (address, _bit) in ACTION_STATE_RECORDS.items()
        }
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "level": level,
            "action_mask": action_word,
            "action_words": action_words,
            "held_keys": [code for code, value in enumerate(keyboard_state) if value & 0x80],
            "keyboard_state": keyboard_state.hex(),
            "action_states": action_states,
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write input sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "input_sample", **record})
        self.sample += 1
        return True

    def on_complete(self):
        _write(f"input sampling complete: {self.sample} samples -> {self.path}")


class TonyInputSample(gdb.Command):
    """tony-input-sample COUNT FILE [--force] -- collect post-poll input state."""

    def __init__(self):
        super().__init__("tony-input-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-input-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-input-sample COUNT FILE [--force]")
        count = _integer(values[0])
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        path = Path(values[1]).expanduser()
        if path.exists() and not force:
            raise gdb.GdbError(f"refusing to overwrite {path}; add --force if intended")
        try:
            path.parent.mkdir(parents=True, exist_ok=True)
            if force:
                path.unlink(missing_ok=True)
        except OSError as exc:
            raise gdb.GdbError(f"could not prepare input sample {path}: {exc}") from exc
        TonyInputSampleBreakpoint(count, path, writer=_trace_writer)
        _write(f"input sampling armed for {count} post-poll hits -> {path}")


class TonyActionSequence(gdb.Command):
    """tony-action-sequence MASK... -- inject raw action masks at publish."""

    def __init__(self):
        super().__init__("tony-action-sequence", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = [value for value in arg.replace(",", " ").split() if value]
        if not values:
            raise gdb.GdbError(
                "usage: tony-action-sequence MASK [MASK ...]"
            )
        masks = [_integer(value) for value in values]
        probe = ActionMaskSequenceProbe(masks, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        _write(
            f"action mask sequence armed for {len(masks)} publishes "
            f"at 0x{probe.address:08x}"
        )


def _watch_limit(values: list[str], usage: str, *, default_limit: int | None) -> tuple[list[str], int | None]:
    limit = default_limit
    if "--limit" in values:
        if values.count("--limit") != 1:
            raise gdb.GdbError(f"usage: {usage}")
        index = values.index("--limit")
        if index + 1 >= len(values):
            raise gdb.GdbError(f"usage: {usage}")
        limit = _integer(values[index + 1])
        values[index:index + 2] = []
        if limit <= 0:
            raise gdb.GdbError("LIMIT must be positive")
    return values, limit


def _watch_arguments(arg: str, usage: str, *, default_limit: int | None) -> tuple[int, str, int, int | None]:
    values, limit = _watch_limit(_argv(arg, usage), usage, default_limit=default_limit)
    if len(values) not in (1, 2):
        raise gdb.GdbError(f"usage: {usage}")
    address, label = _watch_address(values[0])
    size = _integer(values[1]) if len(values) == 2 else 4
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
        writer=_trace_writer,
    )
    mode = "one-shot " if once else ""
    number = getattr(watchpoint, "number", "?")
    limit_text = "" if once else f"; limit {limit} events"
    _write(
        f"{mode}write watchpoint {number} armed at {label}"
        f" (0x{address:08x}, {size} bytes{limit_text})"
    )
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
        values, limit = _watch_limit(_argv(arg, usage), usage, default_limit=_WATCH_DEFAULT_LIMIT)
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
            _write("no active OpenTony watchpoints")
            return
        _write(
            f"{len(active)} active OpenTony watchpoint(s); "
            f"{watchpoints.available()} hardware slot(s) available"
        )
        for watchpoint in active:
            number = getattr(watchpoint, "number", "?")
            mode = "one-shot" if watchpoint.once else f"limit {watchpoint.limit}"
            _write(
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
        _write(f"disabled {len(active)} OpenTony hardware watchpoint(s)")


_runtime_breakpoints = []


class TonyTraceOpen(gdb.Command):
    """tony-trace-open FILE EXPERIMENT [--force] -- start a JSONL runtime trace."""

    def __init__(self):
        super().__init__("tony-trace-open", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        global _trace_writer
        values = _argv(arg, "tony-trace-open FILE EXPERIMENT [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-trace-open FILE EXPERIMENT [--force]")
        if _trace_writer is not None:
            raise gdb.GdbError("a runtime trace is already open; run tony-trace-close first")
        writer = JsonlWriter(values[0], values[1], overwrite=force)
        try:
            writer.open()
        except OSError as exc:
            raise gdb.GdbError(str(exc)) from exc
        _trace_writer = writer
        _write(f"runtime trace opened: {writer.path}")


class TonyTraceClose(gdb.Command):
    """tony-trace-close -- write the trace footer and close the active writer."""

    def __init__(self):
        super().__init__("tony-trace-close", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        global _trace_writer
        if arg.strip():
            raise gdb.GdbError("usage: tony-trace-close")
        if _trace_writer is None:
            _write("no runtime trace is open")
            return
        writer = _trace_writer
        for breakpoint in _runtime_breakpoints:
            if getattr(breakpoint, "writer", None) is writer:
                breakpoint.enabled = False
        watchpoints.disable_writer(writer)
        try:
            writer.close()
        except OSError as exc:
            raise gdb.GdbError(str(exc)) from exc
        _trace_writer = None
        _write(f"runtime trace closed: {writer.path}")


class TonyFrameClock(gdb.Command):
    """tony-frame-clock FUNCTION -- tick the shared clock at a chosen function."""

    def __init__(self):
        super().__init__("tony-frame-clock", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-frame-clock FUNCTION")
        if len(values) != 1:
            raise gdb.GdbError("usage: tony-frame-clock FUNCTION")
        frame_clock.reset()
        try:
            breakpoint = FrameBreakpoint(
                values[0], internal=True, writer=_trace_writer)
        except KeyError as exc:
            raise gdb.GdbError(f"unknown function {values[0]!r}; run tony-thps2") from exc
        _runtime_breakpoints.append(breakpoint)
        _write(f"frame clock armed at {values[0]} (frame starts at 0)")


class TonyPhysicsProbe(gdb.Command):
    """tony-physics-probe [COUNT] -- emit conservative dispatcher observations."""

    def __init__(self):
        super().__init__("tony-physics-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-physics-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PhysicsProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"physics probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraProbe(gdb.Command):
    """tony-camera-probe [COUNT] -- sample the per-frame camera update."""

    def __init__(self):
        super().__init__("tony-camera-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraTimingProbe(gdb.Command):
    """tony-camera-timing-probe [COUNT] -- sample the Q8 camera-rate producer."""

    def __init__(self):
        super().__init__("tony-camera-timing-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-timing-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-timing-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraTimingProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera timing probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraPointSelectProbe(gdb.Command):
    """tony-camera-point-probe [COUNT] -- sample point/mode producer input."""

    def __init__(self):
        super().__init__("tony-camera-point-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-point-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-point-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPointSelectProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera point-select probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraPointStateProbe(gdb.Command):
    """tony-camera-point-state-probe [COUNT] -- sample post-selector state."""

    def __init__(self):
        super().__init__("tony-camera-point-state-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-point-state-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-point-state-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPointStateProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera point-state probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraEffectsProbe(gdb.Command):
    """tony-camera-effects-probe [COUNT] -- sample the effect producer boundary."""

    def __init__(self):
        super().__init__("tony-camera-effects-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-effects-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-effects-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraEffectProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera effects probe armed {limit} at 0x{probe.address:08x}")


class TonyViewProjectionProbe(gdb.Command):
    """tony-view-probe [COUNT] -- sample raw view/projection preparation state."""

    def __init__(self):
        super().__init__("tony-view-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-view-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-view-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ViewProjectionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"view projection probe armed {limit} at 0x{probe.address:08x}")


class TonyViewProjectionPerturb(gdb.Command):
    """tony-view-perturb [COUNT] [--freeze] -- alternate view word 6."""

    def __init__(self):
        super().__init__("tony-view-perturb", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-view-perturb [COUNT] [--freeze]") if arg.strip() else []
        freeze = "--freeze" in values
        values = [value for value in values if value != "--freeze"]
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-view-perturb [COUNT] [--freeze]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ViewProjectionPerturbProbe(
            count, writer=_trace_writer, freeze_input=freeze)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        suffix = "; baseline view input frozen" if freeze else ""
        _write(
            f"view projection perturb probe armed {limit} at 0x{probe.address:08x}"
            f"{suffix}")


class TonyCameraPositionProbe(gdb.Command):
    """tony-camera-position-probe [COUNT] -- sample final camera transform inputs."""

    def __init__(self):
        super().__init__("tony-camera-position-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-position-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-position-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraPositionTransformProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"camera position transform probe armed {limit} at 0x{probe.address:08x}")


class TonyCameraCollisionProbe(gdb.Command):
    """tony-camera-collision-probe [COUNT] -- sample camera world queries."""

    def __init__(self):
        super().__init__("tony-camera-collision-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-camera-collision-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-camera-collision-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = CameraCollisionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        result_probe = CameraCollisionResultProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(result_probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(
            f"camera collision probes armed {limit} at "
            f"0x{probe.address:08x}/0x{result_probe.address:08x}")


class TonyActorSubmissionProbe(gdb.Command):
    """tony-actor-probe [COUNT] -- sample the actor/model submission pointer."""

    def __init__(self):
        super().__init__("tony-actor-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-actor-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-actor-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = ActorSubmissionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"actor submission probe armed {limit} at 0x{probe.address:08x}")


class TonyGeometrySubmissionProbe(gdb.Command):
    """tony-geometry-probe [COUNT] -- sample the raw geometry handoff."""

    def __init__(self):
        super().__init__("tony-geometry-probe", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-geometry-probe [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-geometry-probe [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = GeometrySubmissionProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        raster_probe = GeometryRasterReturnProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(raster_probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(
            f"geometry submission/raster probes armed {limit} at "
            f"0x{probe.address:08x}/0x{raster_probe.address:08x}"
        )


class TonyPlayerDiff(gdb.Command):
    """tony-player-diff [COUNT] -- log changed player words at physics dispatch."""

    def __init__(self):
        super().__init__("tony-player-diff", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-player-diff [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-player-diff [COUNT]")
        count = _integer(values[0]) if values else None
        if count is not None and count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        probe = PlayerDiffProbe(count, writer=_trace_writer)
        _runtime_breakpoints.append(probe)
        limit = "until disabled" if count is None else f"for {count} observations"
        _write(f"player diff probe armed {limit} at 0x{probe.address:08x}")


class TonyPositionCommitProbe(gdb.Command):
    """tony-position-commit [COUNT] -- log stable callers of position commit."""

    DEFAULT_COUNT = 16

    def __init__(self):
        super().__init__("tony-position-commit", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = _argv(arg, "tony-position-commit [COUNT]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-position-commit [COUNT]")
        count = _integer(values[0]) if values else self.DEFAULT_COUNT
        if count <= 0:
            raise gdb.GdbError("COUNT must be positive")
        for address, label in POSITION_COMMIT_CALLS:
            breakpoint = PositionCommitBreakpoint(address, label, count, writer=_trace_writer)
            _runtime_breakpoints.append(breakpoint)
        _write(
            f"position-commit probe armed for {count} observations per caller "
            f"({len(POSITION_COMMIT_CALLS)} callsites)"
        )


_registered = False


def register_commands() -> None:
    global _registered
    if _registered:
        return
    TonyReadInteger("tony-read8", mem.u8, "uint8")
    TonyReadInteger("tony-read16", mem.u16, "uint16")
    TonyReadInteger("tony-read32", mem.u32, "uint32")
    TonyReadFloat()
    TonyHexdump()
    TonyDump()
    TonySnapshot()
    TonyDiff()
    TonyModules()
    TonyBreakpointCommand()
    TonyAddresses()
    TonyTHPS2Breakpoint()
    TonySkipMovies()
    TonyForceLevel()
    TonyPlayerSample()
    TonyInputSample()
    TonyActionSequence()
    TonyWatch()
    TonyWatchOnce()
    TonyWatchBatch()
    TonyWatchLog()
    TonyWatchClear()
    TonyTraceOpen()
    TonyTraceClose()
    TonyFrameClock()
    TonyPhysicsProbe()
    TonyCameraProbe()
    TonyCameraTimingProbe()
    TonyCameraPointSelectProbe()
    TonyCameraPointStateProbe()
    TonyCameraEffectsProbe()
    TonyViewProjectionProbe()
    TonyViewProjectionPerturb()
    TonyCameraPositionProbe()
    TonyCameraCollisionProbe()
    TonyActorSubmissionProbe()
    TonyGeometrySubmissionProbe()
    TonyPlayerDiff()
    TonyPositionCommitProbe()
    _registered = True
    _write(
        "OpenTony GDB helpers loaded: tony-read8, tony-read16, tony-read32, tony-readf, "
        "tony-hexdump, tony-dump, tony-snapshot, tony-diff, tony-modules, tony-bp, "
        "tony-thps2, tony-bp-thps2, "
        "tony-skip-movies, tony-force-level, tony-player-sample, tony-input-sample, "
        "tony-action-sequence, "
        "tony-watch, tony-watch-once, tony-watch-batch, tony-watch-log, tony-watch-clear, "
        "tony-trace-open, tony-trace-close, tony-frame-clock, tony-physics-probe, "
        "tony-camera-probe, tony-camera-timing-probe, tony-camera-point-probe, "
        "tony-camera-point-state-probe, "
        "tony-camera-effects-probe, "
        "tony-view-probe, tony-view-perturb, "
        "tony-camera-position-probe, tony-camera-collision-probe, "
        "tony-actor-probe, tony-geometry-probe, tony-player-diff, tony-position-commit"
    )
