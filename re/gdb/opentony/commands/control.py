"""Input, animation, and action-sequence control commands."""

from __future__ import annotations

import json
import shlex
from pathlib import Path

import gdb

from ..action import ActionMaskSequenceProbe
from ..breakpoint import CountingBreakpoint, TonyBreakpoint
from ..memory import mem
from .common import (
    argv,
    integer,
    key_loop_breakpoints,
    runtime_breakpoints,
    trace_writer,
    write,
)
from .diagnostics import _action_state
from .knowledge import ACTION_STATE_BASE, ACTION_STATE_RECORDS, GLOBALS, THPS2_ADDRESSES


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
        write(f"player sampling complete: {self.sample} samples -> {self.path}")


class TonyPlayerSample(gdb.Command):
    """tony-player-sample COUNT FILE [--force] -- collect raw frame-tick snapshots."""

    def __init__(self):
        super().__init__("tony-player-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-player-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-player-sample COUNT FILE [--force]")
        count = integer(values[0])
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
        TonyPlayerSampleBreakpoint(count, path, writer=trace_writer())
        write(f"player sampling armed for {count} level-loop hits -> {path}")


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
        write(f"input sampling complete: {self.sample} samples -> {self.path}")


class TonyInputSample(gdb.Command):
    """tony-input-sample COUNT FILE [--force] -- collect post-poll input state."""

    def __init__(self):
        super().__init__("tony-input-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-input-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-input-sample COUNT FILE [--force]")
        count = integer(values[0])
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
        TonyInputSampleBreakpoint(count, path, writer=trace_writer())
        write(f"input sampling armed for {count} post-poll hits -> {path}")


class TonyActionEdgeBreakpoint(TonyBreakpoint):
    """Inject one named action after the live mask is built.

    The delay is measured in the shared physics frames, not breakpoint hits.
    That keeps a requested edge out of shell/level-loading input updates when
    the breakpoint is armed before the playable loop exists.
    """

    ACTION_MASK_ADDRESS = GLOBALS["ActionMask"]
    AFTER_ACTION_MASK_BUILD = 0x00489A15

    def __init__(self, action: str, delay=0, hold=1, writer=None):
        try:
            _address, action_mask = ACTION_STATE_RECORDS[action]
        except KeyError as exc:
            raise gdb.GdbError(f"unknown action {action!r}") from exc
        super().__init__(self.AFTER_ACTION_MASK_BUILD, internal=True)
        self.action = action
        self.action_mask = action_mask
        self.delay = delay
        self.hold = hold
        self.injected = False
        self.writer = writer

    def on_hit(self, ctx):
        if ctx.frame < self.delay:
            return
        if self.hold <= 0:
            self.enabled = False
            return
        # 0x00489a15 is the instruction immediately after the poll/build
        # call and immediately before the action-state records consume the
        # returned mask.  This produces one held action frame; the next live
        # poll supplies the release/zero mask normally.
        mem.write(self.ACTION_MASK_ADDRESS, self.action_mask.to_bytes(2, "little"))
        record = {
            "type": f"{self.action}_edge_injection" if not self.injected else f"{self.action}_hold_update",
            "frame": ctx.frame,
            "eip": f"0x{ctx.eip:08x}",
            "action_mask_address": f"0x{self.ACTION_MASK_ADDRESS:08x}",
            "action": self.action,
            "action_mask": self.action_mask,
            "action_state_address": f"0x{ACTION_STATE_BASE:08x}",
            "action_state_bit": self.action_mask,
            "hold_updates": self.hold,
        }
        if self.writer is None:
            self.emit(record)
        else:
            self.writer.event(record)
        self.hold -= 1
        self.injected = True
        if self.hold <= 0:
            self.enabled = False


class TonyActionEdge(gdb.Command):
    """tony-action-edge ACTION [DELAY [HOLD]] -- inject a bounded action."""

    def __init__(self):
        super().__init__("tony-action-edge", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-action-edge ACTION [DELAY [HOLD]]")
        if len(values) not in (1, 2, 3):
            raise gdb.GdbError("usage: tony-action-edge ACTION [DELAY [HOLD]]")
        action = values[0].casefold()
        delay = integer(values[1]) if len(values) > 1 else 0
        hold = integer(values[2]) if len(values) > 2 else 1
        if delay < 0:
            raise gdb.GdbError("DELAY must not be negative")
        if hold <= 0:
            raise gdb.GdbError("HOLD must be positive")
        breakpoint = TonyActionEdgeBreakpoint(action, delay=delay, hold=hold, writer=trace_writer())
        runtime_breakpoints.append(breakpoint)
        write(
            f"one-shot {action.upper()} edge armed at 0x{breakpoint.address:08x}; "
            f"mask 0x{breakpoint.action_mask:04x}; "
            f"delay {delay} physics frames, hold {hold} input updates"
        )


class TonyJumpEdge(gdb.Command):
    """Compatibility alias for tony-action-edge jump [DELAY [HOLD]]."""

    def __init__(self):
        super().__init__("tony-jump-edge", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        try:
            values = shlex.split(arg)
        except ValueError as exc:
            raise gdb.GdbError(f"invalid arguments: {exc}") from exc
        if len(values) > 2:
            raise gdb.GdbError("usage: tony-jump-edge [DELAY [HOLD]]")
        delay = integer(values[0]) if values else 0
        hold = integer(values[1]) if len(values) > 1 else 1
        if delay < 0:
            raise gdb.GdbError("DELAY must not be negative")
        if hold <= 0:
            raise gdb.GdbError("HOLD must be positive")
        breakpoint = TonyActionEdgeBreakpoint("jump", delay=delay, hold=hold, writer=trace_writer())
        runtime_breakpoints.append(breakpoint)
        write(
            f"one-shot JUMP edge armed at 0x{breakpoint.address:08x}; "
            f"mask 0x{breakpoint.action_mask:04x}; delay {delay} physics frames, "
            f"hold {hold} input updates"
        )


class TonyKeyLoopBreakpoint(CountingBreakpoint):
    """Drive one DirectInput scan-code byte with a repeatable press/release loop."""

    def __init__(
        self,
        scan_code: int,
        press_ticks: int,
        release_ticks: int,
        cycles: int,
        on_complete=None,
    ):
        period = press_ticks + release_ticks
        super().__init__(THPS2_ADDRESSES["input_state"][0], count=period * cycles, internal=True)
        self.key_address = GLOBALS["KeyboardState"] + scan_code
        self.scan_code = scan_code
        self.press_ticks = press_ticks
        self.release_ticks = release_ticks
        self.phase = 0
        self.on_complete_callback = on_complete

    def on_count(self, ctx):
        del ctx
        value = 0x80 if self.phase < self.press_ticks else 0
        mem.write(self.key_address, bytes((value,)))
        self.phase = (self.phase + 1) % (self.press_ticks + self.release_ticks)
        if self.remaining == 1:
            mem.write(self.key_address, b"\0")
        return True

    def on_complete(self):
        mem.write(self.key_address, b"\0")
        write(f"keyboard loop complete for scan code {self.scan_code}")
        if self.on_complete_callback is not None:
            self.on_complete_callback()


class TonyKeyLoop(gdb.Command):
    """tony-key-loop SCAN PRESS RELEASE CYCLES -- synthesize repeated key edges."""

    def __init__(self):
        super().__init__("tony-key-loop", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-key-loop SCAN PRESS RELEASE CYCLES")
        if len(values) != 4:
            raise gdb.GdbError("usage: tony-key-loop SCAN PRESS RELEASE CYCLES")
        scan_code, press_ticks, release_ticks, cycles = (integer(value) for value in values)
        if not 0 <= scan_code < 0x100:
            raise gdb.GdbError("SCAN must be a DirectInput scan code between 0 and 255")
        if press_ticks <= 0 or release_ticks <= 0 or cycles <= 0:
            raise gdb.GdbError("PRESS, RELEASE, and CYCLES must be positive")
        breakpoint = TonyKeyLoopBreakpoint(scan_code, press_ticks, release_ticks, cycles)
        key_loop_breakpoints.append(breakpoint)
        write(
            f"keyboard loop armed for scan code {scan_code}: "
            f"{press_ticks} pressed / {release_ticks} released ticks, {cycles} cycles"
        )


class TonyKeyClear(gdb.Command):
    """tony-key-clear [SCAN] -- disable synthesized key loops and release keys."""

    def __init__(self):
        super().__init__("tony-key-clear", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-key-clear [SCAN]") if arg.strip() else []
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-key-clear [SCAN]")
        scan_code = integer(values[0]) if values else None
        if scan_code is not None and not 0 <= scan_code < 0x100:
            raise gdb.GdbError("SCAN must be a DirectInput scan code between 0 and 255")
        cleared = 0
        for breakpoint in key_loop_breakpoints:
            if scan_code is not None and breakpoint.scan_code != scan_code:
                continue
            breakpoint.enabled = False
            mem.write(breakpoint.key_address, b"\0")
            cleared += 1
        suffix = "" if scan_code is None else f" for scan code {scan_code}"
        write(f"disabled {cleared} synthesized keyboard loop(s){suffix}")


class TonyAnimationSampleBreakpoint(CountingBreakpoint):
    """Collect the live player's animation cursor before its object update."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["animation_dispatch"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        if player == 0 or ctx.this_ptr() != player:
            return False
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "animation": {
                "id": mem.u16(player + 0xF6),
                "frame": mem.s16(player + 0xF4),
                "fraction": mem.u16(player + 0x104),
                "rate": mem.u32(player + 0x108),
                "mode": mem.u8(player + 0xF8),
                "direction": mem.s8(player + 0x100),
                "endpoint": mem.s8(player + 0x101),
                "alternate_endpoint": mem.s8(player + 0x102),
                "finished": mem.u8(player + 0x107),
                "old_frame": mem.s16(player + 0x10C),
                "new_frame": mem.s16(player + 0x10E),
                "old_anim": mem.u16(player + 0x110),
                "old_anim_dir": mem.s8(player + 0x112),
            },
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write animation sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "animation_sample", **record})
        self.sample += 1
        return True

    def on_complete(self):
        self.should_stop = True
        write(f"animation sampling complete: {self.sample} samples -> {self.path}")


class TonyAnimationSample(gdb.Command):
    """tony-animation-sample COUNT FILE [--force] -- sample the player cursor."""

    def __init__(self):
        super().__init__("tony-animation-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-animation-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-animation-sample COUNT FILE [--force]")
        count = integer(values[0])
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
            raise gdb.GdbError(f"could not prepare animation sample {path}: {exc}") from exc
        breakpoint = TonyAnimationSampleBreakpoint(count, path, writer=trace_writer())
        runtime_breakpoints.append(breakpoint)
        write(f"animation sampling armed for {count} player updates -> {path}")


class TonyAnimationRequestBreakpoint(CountingBreakpoint):
    """Collect calls into CSuper::RunAnim for the generated player."""

    def __init__(self, count: int | None, path: Path | None, writer=None):
        super().__init__(THPS2_ADDRESSES["animation_run"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        if player == 0 or ctx.this_ptr() != player:
            return False
        wrapper_return = ctx.caller()
        # RunAnim is reached through one of the four fixed-arity request
        # wrappers.  Recover the wrapper's caller from below its saved
        # registers/arguments so a live request can be tied to a selector.
        outer_stack_offset = {
            0x00490414: 0x18,
            0x00490447: 0x18,
            0x0049047A: 0x18,
            0x004904AD: 0x18,
        }.get(wrapper_return)
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "caller": f"0x{wrapper_return:08x}",
            "outer_caller": (
                f"0x{mem.u32(ctx.esp + outer_stack_offset):08x}" if outer_stack_offset is not None else None
            ),
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "request": {
                "animation": ctx.arg(0),
                "start": ctx.arg(1),
                "end": ctx.arg(2),
                "alternate_endpoint": ctx.arg(3),
            },
            "before": {
                "animation": mem.u16(player + 0xF6),
                "frame": mem.s16(player + 0xF4),
                "fraction": mem.u16(player + 0x104),
                "rate": mem.u32(player + 0x108),
                "mode": mem.u8(player + 0xF8),
                "direction": mem.s8(player + 0x100),
                "endpoint": mem.s8(player + 0x101),
                "alternate_endpoint": mem.s8(player + 0x102),
                "finished": mem.u8(player + 0x107),
                "old_frame": mem.s16(player + 0x10C),
                "new_frame": mem.s16(player + 0x10E),
                "old_anim": mem.u16(player + 0x110),
                "old_anim_dir": mem.s8(player + 0x112),
            },
        }
        if self.path is not None:
            try:
                with self.path.open("a", encoding="utf-8") as stream:
                    stream.write(json.dumps(record, sort_keys=True) + "\n")
            except OSError as exc:
                raise gdb.GdbError(f"could not write animation request sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "animation_request", **record})
        self.sample += 1
        return True

    def on_complete(self):
        self.should_stop = True
        write(f"animation request sampling complete: {self.sample} samples -> {self.path}")


class TonyAnimationRequestSample(gdb.Command):
    """tony-animation-request-sample COUNT FILE [--force] -- sample RunAnim calls."""

    def __init__(self):
        super().__init__("tony-animation-request-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-animation-request-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-animation-request-sample COUNT FILE [--force]")
        count = integer(values[0])
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
            raise gdb.GdbError(f"could not prepare animation request sample {path}: {exc}") from exc
        breakpoint = TonyAnimationRequestBreakpoint(count, path, writer=trace_writer())
        runtime_breakpoints.append(breakpoint)
        write(f"animation request sampling armed for {count} player requests -> {path}")


class TonyAnimationSelectorBreakpoint(CountingBreakpoint):
    """Collect the input/state gate seen by the steering animation selector."""

    def __init__(self, count: int, path: Path, writer=None):
        super().__init__(THPS2_ADDRESSES["animation_selector"][0], count=count, internal=True)
        self.path = path
        self.sample = 0
        self.writer = writer

    def on_count(self, ctx):
        player = mem.u32(GLOBALS["Player"])
        if player == 0 or ctx.this_ptr() != player:
            return False
        record = {
            "sample": self.sample,
            "eip": ctx.eip,
            "frame": ctx.frame,
            "player": f"0x{player:08x}",
            "caller": f"0x{ctx.caller():08x}",
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "selector_state": {
                "current_animation": mem.u16(player + 0xF6),
                "current_frame": mem.s16(player + 0xF4),
                "steering_value": mem.s32(player + 0x3148),
                "transition_gate": mem.u32(player + 0x2DD8),
                "object_flags": mem.u32(player + 0xD8),
                "speed_branch": mem.s8(player + 0x31A2),
                "physics_state": mem.u32(player + 0x30B8),
            },
        }
        try:
            with self.path.open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, sort_keys=True) + "\n")
        except OSError as exc:
            raise gdb.GdbError(f"could not write animation selector sample {self.path}: {exc}") from exc
        if self.writer is not None:
            self.writer.event({"type": "animation_selector", **record})
        self.sample += 1
        return True

    def on_complete(self):
        self.should_stop = True
        write(f"animation selector sampling complete: {self.sample} samples -> {self.path}")


class TonyAnimationSelectorSample(gdb.Command):
    """tony-animation-selector-sample COUNT FILE [--force] -- sample steering selector state."""

    def __init__(self):
        super().__init__("tony-animation-selector-sample", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = argv(arg, "tony-animation-selector-sample COUNT FILE [--force]")
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) != 2:
            raise gdb.GdbError("usage: tony-animation-selector-sample COUNT FILE [--force]")
        count = integer(values[0])
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
            raise gdb.GdbError(f"could not prepare animation selector sample {path}: {exc}") from exc
        breakpoint = TonyAnimationSelectorBreakpoint(count, path, writer=trace_writer())
        runtime_breakpoints.append(breakpoint)
        write(f"animation selector sampling armed for {count} player hits -> {path}")


class TonyActionSequence(gdb.Command):
    """tony-action-sequence MASK... -- inject raw action masks at publish."""

    def __init__(self):
        super().__init__("tony-action-sequence", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        values = [value for value in arg.replace(",", " ").split() if value]
        if not values:
            raise gdb.GdbError("usage: tony-action-sequence MASK [MASK ...]")
        masks = [integer(value) for value in values]
        probe = ActionMaskSequenceProbe(masks, writer=trace_writer())
        runtime_breakpoints.append(probe)
        write(f"action mask sequence armed for {len(masks)} publishes at 0x{probe.address:08x}")
