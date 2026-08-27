"""Retail recording replay support for the GDB runtime adapter."""

from __future__ import annotations

import json
import shlex
from pathlib import Path

import gdb

from .breakpoint import Context, TonyBreakpoint
from .knowledge import GLOBALS
from .memory import mem
from .timer import TimerReplayService
from .timing import TIMING_FIELDS, animation_timing_record, timing_raw_value

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
# The physics wrapper publishes simulation time into the player's persistent
# frame state at this instruction.  At the store breakpoint EDX already holds
# the value loaded by retail, so replacing EDX is narrower than racing the
# timer callback by rewriting the global earlier in the wrapper.
_SIMULATION_TIME_STORE_ADDRESS = 0x0049F1A9
_ANIMATION_CLOCK_STORE_ADDRESS = 0x0049F169
_LANDING_FRAME_STORE_ADDRESS = 0x00499255
_LAUNCH_FRAME_STORE_ADDRESS = 0x0049AF14
_AIR_MOTION_X_STORE_ADDRESS = 0x00491985
_PLAYER_FRAME_COUNTER_STORE_ADDRESS = 0x0049E92B
_ANIMATION_STATE_TIMESTAMP_STORE_ADDRESS = 0x0049C1E4
# The first store's source register is not reused for control flow.  The
# second store is followed by a comparison against DL, so changing EDX there
# would alter the branch that computes brake_mode rather than merely restoring
# the recorded timestamp.
_PLAYER_ANIMATION_TIMESTAMP_STORES = (0x0049392B,)
_RAW_PHYSICS_OFFSET = 0x2D80
_RAW_PHYSICS_WORDS = 0x490 // 4
_SIMULATION_TIME_PLAYER_WORD = (0x2F44 - _RAW_PHYSICS_OFFSET) // 4
_ANIMATION_CLOCK_PLAYER_WORD = (0x2DE4 - _RAW_PHYSICS_OFFSET) // 4
_ANIMATION_TIMESTAMP_PLAYER_WORD = (0x3060 - _RAW_PHYSICS_OFFSET) // 4
_LANDING_FRAME_PLAYER_WORD = (0x2D98 - _RAW_PHYSICS_OFFSET) // 4
_LAUNCH_FRAME_PLAYER_WORD = (0x2F34 - _RAW_PHYSICS_OFFSET) // 4
_AIR_MOTION_X_PLAYER_WORD = (0x310C - _RAW_PHYSICS_OFFSET) // 4
_PLAYER_FRAME_COUNTER_PLAYER_WORD = (0x2D8C - _RAW_PHYSICS_OFFSET) // 4
_ANIMATION_STATE_TIMESTAMP_PLAYER_WORD = (0x2E28 - _RAW_PHYSICS_OFFSET) // 4
_AIR_CONTROL_GLOBAL = 0x0056B7F0
_VOLATILE_TIMING_FIELDS = {
    "animation_clock",
    "animation_clock_accumulator",
    "simulation_time",
}
# The callback breakpoint is intentionally kept on the already validated
# final integer store.  At that point the callback's state transition is
# complete, but the pending store can still be supplied with the deterministic
# modeled result without tracing Wine's timer thread entry.
_TIMER_CALLBACK_STORE_ADDRESS = 0x004DAD68

REPLAY_MODES = ("assisted", "strict")
_DERIVED_REPLAY_CHANNELS = (
    "simulation_time_store",
    "animation_clock_store",
    "landing_frame_store",
    "launch_frame_store",
    "air_motion_x_store",
    "player_frame_counter_store",
    "animation_state_timestamp_store",
    "animation_timestamp_store",
)


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
        "timing": animation_timing_record(mem),
        "raw_physics_words": [
            mem.u32(player + _RAW_PHYSICS_OFFSET + index * 4)
            for index in range(_RAW_PHYSICS_WORDS)
        ],
        "physics_state": physics_state,
        "physics": {
            "state_raw": physics_state,
            "previous_state_raw": mem.u32(player + 0x30C0),
            "auxiliary_state_raw": mem.u32(player + 0x30C4),
            "air_control_enabled": bool(mem.u32(_AIR_CONTROL_GLOBAL)),
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


def _comparison_snapshot(value):
    """Drop process-clock observations that are not gameplay state."""

    result = _without_address(value)
    timing = result.get("timing") if isinstance(result, dict) else None
    if isinstance(timing, dict):
        for name in _VOLATILE_TIMING_FIELDS:
            timing.pop(name, None)
    return result


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
    if records[0].get("capture_schema_version") != 2:
        raise gdb.GdbError(f"unsupported retail recording capture schema: {path}")
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

    def __init__(self, path: str | Path, *, mode: str = "assisted"):
        if mode not in REPLAY_MODES:
            raise ValueError(
                f"unsupported retail replay mode {mode!r}; "
                f"choose one of {', '.join(REPLAY_MODES)}"
            )
        self.path = Path(path).expanduser().resolve()
        self.header, self.frames = _load_recording(self.path)
        self.mode = mode
        self.index = 0
        self.active = False
        self._keyboard_initialized = False
        self._previous_held_keys: set[int] = set()
        self._active_return: RetailReplayReturnBreakpoint | None = None
        self._stopped = False
        initial_timer_state = TimerReplayService.initial_from_recording(
            self.header, self.frames
        )
        self.timer_service = TimerReplayService(initial_timer_state)
        self.input_breakpoint = RetailReplayInputBreakpoint(self)
        self.action_breakpoint: RetailReplayActionBuildBreakpoint | None = None
        self.entry_breakpoint = RetailReplayFrameEntryBreakpoint(self)
        self.simulation_time_breakpoint = RetailReplaySimulationTimeBreakpoint(self)
        self.animation_clock_breakpoint = RetailReplayAnimationClockBreakpoint(self)
        self.landing_frame_breakpoint = RetailReplayLandingFrameBreakpoint(self)
        self.launch_frame_breakpoint = RetailReplayLaunchFrameBreakpoint(self)
        self.air_motion_x_breakpoint = RetailReplayAirMotionXBreakpoint(self)
        self.player_frame_counter_breakpoint = RetailReplayPlayerFrameCounterBreakpoint(self)
        self.animation_state_timestamp_breakpoint = RetailReplayAnimationStateTimestampBreakpoint(self)
        self.animation_timestamp_breakpoints = [
            RetailReplayAnimationTimestampBreakpoint(self, address)
            for address in _PLAYER_ANIMATION_TIMESTAMP_STORES
        ]
        self.timer_callback_breakpoint = RetailReplayTimerCallbackBreakpoint(self)
        self.timer_callback_breakpoint.enabled = (
            self.mode == "strict" and self.timer_service.available
        )
        self._derived_breakpoints = (
            self.simulation_time_breakpoint,
            self.animation_clock_breakpoint,
            self.landing_frame_breakpoint,
            self.launch_frame_breakpoint,
            self.air_motion_x_breakpoint,
            self.player_frame_counter_breakpoint,
            self.animation_state_timestamp_breakpoint,
            *self.animation_timestamp_breakpoints,
        )
        if self.mode == "strict":
            for breakpoint in self._derived_breakpoints:
                breakpoint.enabled = False

    def install(self) -> None:
        gdb.write(
            f"retail replay armed: {self.path} ({len(self.frames)} frames)\n"
            f"mode: {self.mode}\n"
        )
        if self.mode == "assisted":
            gdb.write(
                "injected derived channels: "
                + ", ".join(_DERIVED_REPLAY_CHANNELS)
                + "\n"
            )
        else:
            gdb.write("injected derived channels: none\n")
        if self.mode == "strict":
            if self.timer_service.available:
                gdb.write(
                    "deterministic timer deliveries: enabled\n"
                )
            else:
                gdb.write(
                    "deterministic timer deliveries: unavailable "
                    "(recording has no initial timer state)\n"
                )

    def inject_input(self) -> None:
        if self._stopped or self.index >= len(self.frames):
            return
        input_record = self.frames[self.index].get("input", {})
        mask = int(input_record.get("action_mask", 0)) & 0xFFFF
        mem.write(_ACTION_MASK_ADDRESS, mask.to_bytes(2, "little"))
        self.inject_axes(input_record)
        self.inject_timing()

    def inject_timing(self) -> None:
        """Restore the recorded globals before the next player frame."""

        if self.mode == "strict":
            return
        if self.index >= len(self.frames):
            return
        expected = self.frames[self.index].get("before", {})
        timing = expected.get("timing") if isinstance(expected, dict) else None
        if not isinstance(timing, dict):
            return
        # These values are consumed by the physics wrapper itself.  The
        # integer animation clock/accumulator are produced later in the outer
        # loop; overwriting them here changes the order being measured.
        for name in (
            "animation_time_scale",
            "animation_time_scale_square",
            "simulation_time",
            "timing_delta_q11",
        ):
            address = TIMING_FIELDS[name]
            value = timing_raw_value(timing, name)
            if value is not None:
                mem.write_u32(address, value)

    def inject_after_timing(self) -> None:
        """Restore timing globals captured after the completed frame."""

        if self.mode == "strict":
            return
        if self.index >= len(self.frames):
            return
        expected = self.frames[self.index].get("after", {})
        timing = expected.get("timing") if isinstance(expected, dict) else None
        if not isinstance(timing, dict):
            return
        value = timing_raw_value(timing, "simulation_time")
        if value is not None:
            mem.write_u32(TIMING_FIELDS["simulation_time"], value)

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
        if self.mode == "strict" and self.timer_service.available:
            try:
                self.timer_service.apply_frame(self.frames[self.index], mem)
            except (TypeError, ValueError) as exc:
                self._diverge(
                    "timer",
                    (("events",), "valid timer deliveries", str(exc)),
                    self.entry_breakpoint,
                )
                return
        self.inject_timing()
        if self.index == 0 or self.index % 16 == 0:
            gdb.write(f"retail replay frame {self.index}/{len(self.frames)}\n")
        expected = _comparison_snapshot(self.frames[self.index].get("before", {}))
        actual = _comparison_snapshot(_snapshot(player))
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
        expected = _comparison_snapshot(self.frames[self.index].get("after", {}))
        actual = _comparison_snapshot(after)
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
        self.timer_callback_breakpoint.enabled = False
        self.simulation_time_breakpoint.enabled = False
        self.animation_clock_breakpoint.enabled = False
        self.landing_frame_breakpoint.enabled = False
        self.launch_frame_breakpoint.enabled = False
        self.air_motion_x_breakpoint.enabled = False
        self.player_frame_counter_breakpoint.enabled = False
        self.animation_state_timestamp_breakpoint.enabled = False
        for timestamp_breakpoint in self.animation_timestamp_breakpoints:
            timestamp_breakpoint.enabled = False
        if breakpoint is not None:
            breakpoint.should_stop = True
        gdb.write(
            f"frames: {self.index}\n"
            f"matching: {self.index}\n"
            f"mode: {self.mode}\n"
            "result: deterministic\n"
        )

    def _diverge(self, stage: str, difference, breakpoint=None) -> None:
        self._stopped = True
        self.input_breakpoint.enabled = False
        if self.action_breakpoint is not None:
            self.action_breakpoint.enabled = False
        self.entry_breakpoint.enabled = False
        self.timer_callback_breakpoint.enabled = False
        self.simulation_time_breakpoint.enabled = False
        self.animation_clock_breakpoint.enabled = False
        self.landing_frame_breakpoint.enabled = False
        self.launch_frame_breakpoint.enabled = False
        self.air_motion_x_breakpoint.enabled = False
        self.player_frame_counter_breakpoint.enabled = False
        self.animation_state_timestamp_breakpoint.enabled = False
        for timestamp_breakpoint in self.animation_timestamp_breakpoints:
            timestamp_breakpoint.enabled = False
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
            f"mode: {self.mode}\n"
            "result: divergent\n"
        )

    def suppress_live_timer_callback(self, ctx: Context) -> None:
        """Neutralize one uncontrolled Wine callback delivery.

        The replay service has already applied the recorded deliveries at the
        frame boundary.  A real multimedia callback may still fire on Wine's
        timer thread; restore the modeled state immediately before its final
        integer store and let that store execute with the modeled EAX value.
        """

        if self._stopped or self.mode != "strict":
            return
        self.timer_service.suppress_live_callback(
            ctx.memory,
            result_register_setter=lambda value: gdb.execute(
                f"set $eax = {value & 0xFFFFFFFF}"
            ),
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


class RetailReplayTimerCallbackBreakpoint(TonyBreakpoint):
    """Gate the final store of Wine's asynchronous timer callback."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_TIMER_CALLBACK_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.replay.suppress_live_timer_callback(ctx)


class RetailReplaySimulationTimeBreakpoint(TonyBreakpoint):
    """Supply the recorded simulation time to the exact player-state store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_SIMULATION_TIME_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("ebp")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _SIMULATION_TIME_PLAYER_WORD:
            return
        value = raw_words[_SIMULATION_TIME_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $edx = {value & 0xFFFFFFFF}")


class RetailReplayAnimationClockBreakpoint(TonyBreakpoint):
    """Supply the recorded animation clock to its exact player-state store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_ANIMATION_CLOCK_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("ebp")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _ANIMATION_CLOCK_PLAYER_WORD:
            return
        value = raw_words[_ANIMATION_CLOCK_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $eax = {value & 0xFFFFFFFF}")


class RetailReplayLandingFrameBreakpoint(TonyBreakpoint):
    """Supply the recorded landing frame to its exact player-state store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_LANDING_FRAME_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("ebp")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _LANDING_FRAME_PLAYER_WORD:
            return
        value = raw_words[_LANDING_FRAME_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $edx = {value & 0xFFFFFFFF}")


class RetailReplayAnimationTimestampBreakpoint(TonyBreakpoint):
    """Supply the recorded animation timestamp to its player-state store."""

    def __init__(self, replay: RetailReplay, address: int):
        self.replay = replay
        super().__init__(address, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _ANIMATION_TIMESTAMP_PLAYER_WORD:
            return
        value = raw_words[_ANIMATION_TIMESTAMP_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $edx = {value & 0xFFFFFFFF}")


class RetailReplayLaunchFrameBreakpoint(TonyBreakpoint):
    """Supply the recorded launch frame to its exact player-state store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_LAUNCH_FRAME_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("ebp")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _LAUNCH_FRAME_PLAYER_WORD:
            return
        value = raw_words[_LAUNCH_FRAME_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $edx = {value & 0xFFFFFFFF}")


class RetailReplayAirMotionXBreakpoint(TonyBreakpoint):
    """Supply the recorded air-motion X component to its exact store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_AIR_MOTION_X_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("esi")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _AIR_MOTION_X_PLAYER_WORD:
            return
        value = raw_words[_AIR_MOTION_X_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $edx = {value & 0xFFFFFFFF}")


class RetailReplayPlayerFrameCounterBreakpoint(TonyBreakpoint):
    """Supply the recorded player-frame counter to its conditional store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_PLAYER_FRAME_COUNTER_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("ebp")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _PLAYER_FRAME_COUNTER_PLAYER_WORD:
            return
        value = raw_words[_PLAYER_FRAME_COUNTER_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $eax = {value & 0xFFFFFFFF}")


class RetailReplayAnimationStateTimestampBreakpoint(TonyBreakpoint):
    """Supply the recorded animation-state timestamp to its exact store."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_ANIMATION_STATE_TIMESTAMP_STORE_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or self.replay.index >= len(self.replay.frames):
            return
        player = ctx.register("edi")
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        expected = self.replay.frames[self.replay.index].get("after", {})
        raw_words = expected.get("raw_physics_words") if isinstance(expected, dict) else None
        if not isinstance(raw_words, list) or len(raw_words) <= _ANIMATION_STATE_TIMESTAMP_PLAYER_WORD:
            return
        value = raw_words[_ANIMATION_STATE_TIMESTAMP_PLAYER_WORD]
        if isinstance(value, int):
            gdb.execute(f"set $ecx = {value & 0xFFFFFFFF}")


class RetailReplayReturnBreakpoint(TonyBreakpoint):
    def __init__(self, replay: RetailReplay, address: int, player: int):
        self.replay = replay
        self.player = player
        super().__init__(address, internal=True, temporary=True)

    def on_hit(self, _ctx: Context) -> None:
        self.enabled = False
        if self.replay._active_return is not self:
            return
        self.replay.inject_after_timing()
        self.replay.frame_return(_snapshot(self.player), self)


def create_retail_replay(path: str | Path, *, mode: str = "assisted") -> RetailReplay:
    return RetailReplay(path, mode=mode)


def gdb_replay_usage(path: str | Path, *, mode: str = "assisted") -> str:
    return "tony-replay-retail " + shlex.quote(str(path)) + " --mode " + shlex.quote(mode)
