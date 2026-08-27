"""Retail recording replay support for the GDB runtime adapter."""

from __future__ import annotations

import json
import shlex
from pathlib import Path

import gdb

from .breakpoint import Context, TonyBreakpoint
from .knowledge import GLOBALS
from .memory import mem

_ACTION_MASK_ADDRESS = GLOBALS["ActionMask"]
_KEYBOARD_STATE_ADDRESS = GLOBALS["KeyboardState"]
_ACTION_BUILD_ADDRESS = 0x004E42C0
_RAW_AXIS_ADDRESS = 0x0056AFBD
_NORMALIZED_AXIS_ADDRESS = 0x0056B140
# The post-input boundary is also the replay activation gate.  It is kept as
# a separate hook because the held-key injection must happen earlier, at the
# retail action-mask builder.
_INPUT_INJECTION_ADDRESS = 0x00469DE0
# The recording boundary is the wrapper that owns one complete player physics
# update.  ``physics_dispatch`` is an inner state switch and can be entered
# more than once without advancing the recording frame.
_PHYSICS_FRAME_ADDRESS = 0x0049E680


def _signed32(value: int) -> int:
    return value - 0x100000000 if value & 0x80000000 else value


def _signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def _vec(memory_address: int) -> dict | None:
    if not mem.readable(memory_address, 0x0C):
        return None
    raw = list(mem.u32_vec3(memory_address))
    return {
        "raw": raw,
        "signed": [_signed32(value) for value in raw],
    }


def _short_vec(memory_address: int) -> dict | None:
    if not mem.readable(memory_address, 6):
        return None
    raw = [mem.u16(memory_address + offset) for offset in (0, 2, 4)]
    return {
        "raw": raw,
        "signed": [_signed16(value) for value in raw],
    }


def _snapshot(player: int) -> dict:
    physics_state = mem.u32(player + 0x30B8)
    return {
        "player_address": f"0x{player:08x}",
        "physics_state": physics_state,
        "physics": {
            "state_raw": physics_state,
            "previous_state_raw": mem.u32(player + 0x30C0),
            "auxiliary_state_raw": mem.u32(player + 0x30C4),
        },
        "position": _vec(player + 0x08),
        "position_history": _vec(player + 0xBC),
        "response_velocity": _vec(player + 0x4C),
        "correction": _vec(player + 0x58),
        "air_motion": _vec(player + 0x310C),
        "turn": {
            "accumulator_raw": mem.u32(player + 0x3144),
            "mirror_raw": mem.u32(player + 0x3148),
        },
        "basis": {
            "forward_raw": _vec(player + 0x30F4),
            "up_raw": _vec(player + 0x3100),
            "air_raw": _vec(player + 0x310C),
        },
        "orientation": {
            "row_0": _short_vec(player + 0x2E58),
            "row_1": _short_vec(player + 0x2E5E),
            "row_2": _short_vec(player + 0x2E64),
        },
        "animation": {
            "id_raw": mem.u16(player + 0xF6),
            "frame_raw": mem.s16(player + 0xF4),
            "fraction_raw": mem.u16(player + 0x104),
            "rate_raw": mem.u32(player + 0x108),
            "mode_raw": mem.u8(player + 0xF8),
            "direction_raw": mem.s8(player + 0x100),
            "endpoint_raw": mem.s8(player + 0x101),
            "alternate_endpoint_raw": mem.s8(player + 0x102),
            "finished_raw": mem.u8(player + 0x107),
        },
    }


def _without_address(value):
    if isinstance(value, dict):
        return {
            key: _without_address(item)
            for key, item in value.items()
            if key != "player_address"
        }
    if isinstance(value, list):
        return [_without_address(item) for item in value]
    return value


def _first_difference(expected, actual, path=()):
    if isinstance(expected, dict) and isinstance(actual, dict):
        keys = sorted(set(expected) | set(actual))
        for key in keys:
            if key not in expected:
                return path + (key,), "<missing>", actual[key]
            if key not in actual:
                return path + (key,), expected[key], "<missing>"
            difference = _first_difference(expected[key], actual[key], path + (key,))
            if difference is not None:
                return difference
        return None
    if isinstance(expected, list) and isinstance(actual, list):
        if len(expected) != len(actual):
            return path + ("length",), len(expected), len(actual)
        for index, (left, right) in enumerate(zip(expected, actual)):
            difference = _first_difference(left, right, path + (index,))
            if difference is not None:
                return difference
        return None
    if expected != actual:
        return path, expected, actual
    return None


def _load_recording(path: Path) -> tuple[dict, list[dict]]:
    try:
        records = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
    except (OSError, json.JSONDecodeError) as exc:
        raise gdb.GdbError(f"could not read retail recording {path}: {exc}") from exc
    if not records or records[0].get("type") != "header":
        raise gdb.GdbError(f"retail recording has no header: {path}")
    if records[0].get("format") != "opentony-retail-recording-v1":
        raise gdb.GdbError(f"unsupported retail recording format: {path}")
    frames = [record for record in records if record.get("type") == "frame"]
    footer = records[-1] if records else {}
    if footer.get("type") != "end" or not footer.get("complete"):
        raise gdb.GdbError(f"retail recording is not complete: {path}")
    if footer.get("frames") != len(frames):
        raise gdb.GdbError(f"retail recording footer frame count is inconsistent: {path}")
    return records[0], frames


def _axis_bytes(input_record: dict) -> tuple[bytes | None, bytes | None]:
    raw = input_record.get("raw_axes") or {}
    devices = raw.get("device_bytes") or []
    if len(devices) != 4:
        raw_bytes = None
    else:
        raw_bytes = bytes(int(item["raw"]) & 0xFF for item in devices)
    normalized = input_record.get("normalized_axes") or raw.get("normalized")
    if not isinstance(normalized, dict):
        normalized_bytes = None
    else:
        normalized_bytes = bytes(
            (
                int(normalized.get("horizontal", 0)) & 0xFF,
                int(normalized.get("vertical", 0)) & 0xFF,
            )
        )
    return raw_bytes, normalized_bytes


class RetailReplay:
    """Inject recorded retail input and compare canonical player frames."""

    def __init__(self, path: str | Path):
        self.path = Path(path).expanduser().resolve()
        self.header, self.frames = _load_recording(self.path)
        self.index = 0
        self.active = False
        self._keyboard_initialized = False
        self._previous_held_keys: set[int] = set()
        self._active_return: RetailReplayReturnBreakpoint | None = None
        self._stopped = False
        self.input_breakpoint = RetailReplayInputBreakpoint(self)
        self.action_breakpoint: RetailReplayActionBuildBreakpoint | None = None
        self.entry_breakpoint = RetailReplayFrameEntryBreakpoint(self)

    def install(self) -> None:
        gdb.write(
            f"retail replay armed: {self.path} ({len(self.frames)} frames)\n"
        )

    def inject_input(self) -> None:
        if self._stopped or self.index >= len(self.frames):
            return
        input_record = self.frames[self.index].get("input", {})
        mask = int(input_record.get("action_mask", 0)) & 0xFFFF
        mem.write(_ACTION_MASK_ADDRESS, mask.to_bytes(2, "little"))
        self.inject_axes(input_record)

    def inject_axes(self, input_record: dict) -> None:
        raw_bytes, normalized_bytes = _axis_bytes(input_record)
        if raw_bytes is not None:
            mem.write(_RAW_AXIS_ADDRESS, raw_bytes)
        if normalized_bytes is not None:
            mem.write(_NORMALIZED_AXIS_ADDRESS, normalized_bytes)

    def inject_keyboard(self, input_record: dict) -> None:
        """Apply the recorded held-key set before retail builds its action mask."""

        if not self._keyboard_initialized:
            keyboard_state = input_record.get("keyboard_state")
            if isinstance(keyboard_state, str) and len(keyboard_state) == 0x200:
                try:
                    keyboard = bytearray.fromhex(keyboard_state)
                except ValueError:
                    keyboard = None
                if keyboard is not None:
                    hotkey = int(self.header.get("hotkey_scan_code", 0x58))
                    if 0 <= hotkey < len(keyboard):
                        keyboard[hotkey] = 0
                    mem.write(_KEYBOARD_STATE_ADDRESS, bytes(keyboard))
            self._keyboard_initialized = True

        held = {
            int(code)
            for code in input_record.get("held_scan_codes", ())
            if isinstance(code, int) and 0 <= code < 0x100
        }
        for scan_code in sorted(self._previous_held_keys - held):
            mem.write_u8(_KEYBOARD_STATE_ADDRESS + scan_code, 0)
        for scan_code in sorted(held):
            mem.write_u8(_KEYBOARD_STATE_ADDRESS + scan_code, 0x80)
        self._previous_held_keys = held

    def activate(self) -> None:
        if self.active:
            return
        self.active = True
        self.action_breakpoint = RetailReplayActionBuildBreakpoint(self)

    def frame_entry(self, ctx: Context) -> None:
        if self._stopped:
            return
        player = ctx.this_ptr()
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        if self.index >= len(self.frames):
            self._finish()
            return
        if self.index == 0 or self.index % 16 == 0:
            gdb.write(f"retail replay frame {self.index}/{len(self.frames)}\n")
        actual = _without_address(_snapshot(player))
        expected = _without_address(self.frames[self.index].get("before", {}))
        difference = _first_difference(expected, actual)
        if difference is not None:
            self._diverge("before", difference, self.entry_breakpoint)
            return
        return_address = ctx.return_address()
        if not return_address:
            self._diverge(
                "before",
                (("return_address",), "valid", 0),
                self.entry_breakpoint,
            )
            return
        self._active_return = RetailReplayReturnBreakpoint(self, return_address, player)

    def frame_return(self, after: dict, breakpoint: RetailReplayReturnBreakpoint) -> None:
        if self._stopped or self._active_return is not breakpoint:
            return
        self._active_return = None
        expected = _without_address(self.frames[self.index].get("after", {}))
        actual = _without_address(after)
        difference = _first_difference(expected, actual)
        if difference is not None:
            self._diverge("after", difference, breakpoint)
            return
        self.index += 1
        if self.index >= len(self.frames):
            self._finish(breakpoint)

    def _finish(self, breakpoint: RetailReplayReturnBreakpoint | None = None) -> None:
        self._stopped = True
        self.input_breakpoint.enabled = False
        if self.action_breakpoint is not None:
            self.action_breakpoint.enabled = False
        self.entry_breakpoint.enabled = False
        if breakpoint is not None:
            breakpoint.should_stop = True
        gdb.write(
            f"frames: {self.index}\nmatching: {self.index}\nresult: deterministic\n"
        )

    def _diverge(self, stage: str, difference, breakpoint=None) -> None:
        self._stopped = True
        self.input_breakpoint.enabled = False
        if self.action_breakpoint is not None:
            self.action_breakpoint.enabled = False
        self.entry_breakpoint.enabled = False
        if breakpoint is not None:
            breakpoint.should_stop = True
        path, expected, actual = difference
        formatted_path = ".".join(str(part) for part in path) or "<root>"
        gdb.write(
            f"first divergence: frame {self.index}\n"
            f"stage: {stage}\n"
            f"field: {formatted_path}\n"
            f"original = {expected!r}\n"
            f"replay   = {actual!r}\n"
            f"frames: {self.index}\n"
            f"matching: {self.index}\n"
            "result: divergent\n"
        )


class RetailReplayInputBreakpoint(TonyBreakpoint):
    def __init__(self, replay: RetailReplay):
        self.replay = replay
        self.hits = 0
        super().__init__(_INPUT_INJECTION_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        self.hits += 1
        if self.hits == 1 or self.hits % 100 == 0:
            gdb.write(f"retail replay input {self.hits}\n")
        self.replay.activate()
        self.replay.inject_input()


class RetailReplayActionBuildBreakpoint(TonyBreakpoint):
    """Inject held keys immediately before PCInput_BuildActionMask runs."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_ACTION_BUILD_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay.active:
            input_record = self.replay.frames[self.replay.index].get("input", {})
            self.replay.inject_keyboard(input_record)
            self.replay.inject_axes(input_record)


class RetailReplayFrameEntryBreakpoint(TonyBreakpoint):
    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_PHYSICS_FRAME_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.replay.frame_entry(ctx)


class RetailReplayReturnBreakpoint(TonyBreakpoint):
    def __init__(self, replay: RetailReplay, address: int, player: int):
        self.replay = replay
        self.player = player
        super().__init__(address, internal=True, temporary=True)

    def on_hit(self, _ctx: Context) -> None:
        self.enabled = False
        if self.replay._active_return is not self:
            return
        self.replay.frame_return(_snapshot(self.player), self)


def create_retail_replay(path: str | Path) -> RetailReplay:
    return RetailReplay(path)


def gdb_replay_usage(path: str | Path) -> str:
    return "tony-replay-retail " + shlex.quote(str(path))
