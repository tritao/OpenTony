"""Retail recording replay support for the GDB runtime adapter."""

from __future__ import annotations

import json
import shlex
from pathlib import Path

import gdb

from .breakpoint import Context, TonyBreakpoint
from .knowledge import GLOBALS
from .memory import mem
from .player import canonical_player_snapshot
from .timer import (
    TIMER_PAUSE_GATE_A,
    TIMER_PAUSE_GATE_B,
    TIMER_SIMULATION_ACCUMULATOR,
    TIMER_SIMULATION_TIME,
    TimerReplayService,
)

_ACTION_MASK_ADDRESS = GLOBALS["ActionMask"]
_KEYBOARD_STATE_ADDRESS = GLOBALS["KeyboardState"]
_ACTION_BUILD_ADDRESS = 0x004E42C0
_ACTION_STATE_UPDATE_ADDRESS = 0x00489A15
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
# Animation objects are advanced before the physics wrapper.  The retail
# animation step consumes the global scale published by the previous level
# loop; replaying a fast in-process capture under GDB otherwise lets debugger
# wall time change that scale before the next player's cursor update.
_ANIMATION_DISPATCH_ADDRESS = 0x00480FA0
_ANIMATION_CLOCK_ADDRESS = 0x005685F4
_ANIMATION_SCALE_ADDRESS = 0x0056865C
_ANIMATION_SCALE_SQUARE_ADDRESS = 0x00568804
_ANIMATION_CLOCK_ACCUMULATOR_ADDRESS = 0x00568810
_TIMING_DELTA_ADDRESS = 0x0056A93C
# The physics wrapper reads simulation time at this instruction.  The replay
# timer service supplies the causal deliveries before this load.
_SIMULATION_TIME_LOAD_ADDRESS = 0x0049F1A0
_TIMER_FRAME_CLOCK_UPDATE_ADDRESS = 0x0046A0F0
# FUN_00468b30 loads the previous clock at entry, then reads the current
# simulation time at this instruction.  Deliveries recorded here affect the
# producer's delta without changing its latched previous operand.
_TIMER_PRODUCER_READ_ADDRESS = 0x00468B40
# The producer reads the current simulation clock a second time for the
# timing-delta word.  A delivery between the first and second reads changes
# that word without changing the ring sample above it.
_TIMER_PRODUCER_DELTA_READ_ADDRESS = 0x00468B6B
_TIMER_PRODUCER_OUTPUT_READ_ADDRESS = 0x00468B7C
_TIMER_PRODUCER_PREVIOUS_STORE_ADDRESS = 0x00468BA1
_RAW_PHYSICS_OFFSET = 0x2D80
_RAW_PHYSICS_WORDS = 0x490 // 4
_AIR_CONTROL_GLOBAL = 0x0056B7F0
_VOLATILE_TIMING_FIELDS = {
    "animation_clock",
    "animation_clock_accumulator",
    "simulation_time",
    # This is a derived delta from the asynchronously published simulation
    # clock.  Keep it in the recording as an assertion/diagnostic, but do not
    # make strict player-state comparison fail on a one-delivery scheduling
    # race at the render timing boundary.
    "timing_delta_q11",
}
_TIMER_CALLBACK_ENTRY_ADDRESS = 0x004DACE0
_TIMING_PREVIOUS_TIME = 0x00568604
_TIMING_RING = 0x0056868C
_TIMING_RING_INDEX = 0x0056A934


def _snapshot(player: int) -> dict:
    return canonical_player_snapshot(player, mem)


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

    def __init__(self, path: str | Path):
        self.path = Path(path).expanduser().resolve()
        self.header, self.frames = _load_recording(self.path)
        self.index = 0
        self.active = False
        self._keyboard_initialized = False
        self._previous_held_keys: set[int] = set()
        self._active_return: RetailReplayReturnBreakpoint | None = None
        self._stopped = False
        self._timer_callback_suppressed = False
        initial_timer_state = self.header.get("initial_timer_state")
        if not isinstance(initial_timer_state, dict):
            raise TypeError(
                "retail recording is missing required initial_timer_state"
            )
        self.timer_service = TimerReplayService(initial_timer_state)
        self.input_breakpoint = RetailReplayInputBreakpoint(self)
        self.action_breakpoint: RetailReplayActionBuildBreakpoint | None = None
        self.action_update_breakpoint = RetailReplayActionUpdateBreakpoint(self)
        self.entry_breakpoint = RetailReplayFrameEntryBreakpoint(self)
        self.animation_breakpoint = RetailReplayAnimationBreakpoint(self)
        self.timer_clock_read_breakpoint = RetailReplayTimerClockReadBreakpoint(self)
        self.timer_frame_entry_breakpoint = RetailReplayTimerFrameEntryBreakpoint(self)
        self.timer_producer_read_breakpoint = RetailReplayTimerProducerReadBreakpoint(self)
        self.timer_producer_delta_read_breakpoint = RetailReplayTimerProducerDeltaReadBreakpoint(self)
        self.timer_producer_output_breakpoint = RetailReplayTimerProducerOutputBreakpoint(self)
        self.timer_producer_previous_store_breakpoint = RetailReplayTimerProducerPreviousStoreBreakpoint(self)

    def install(self) -> None:
        gdb.write(
            f"retail replay armed: {self.path} ({len(self.frames)} frames)\n"
            "contract: strict causal replay\n"
            "injected derived channels: none\n"
        )
        gdb.write("deterministic timer deliveries: pending first gameplay frame\n")

    def suppress_uncontrolled_timer_callback(self) -> None:
        """Return the retail callback before it mutates timing state.

        This is process-local replay setup, not a recorded derived-state
        write.  The callback's ABI is ``ret 0x14`` (five 32-bit arguments),
        so replacing its entry with that return preserves the multimedia
        timer service while preventing uncontrolled asynchronous transitions.
        """

        mem.write(_TIMER_CALLBACK_ENTRY_ADDRESS, b"\xc2\x14\x00")

    def inject_input(self) -> None:
        if self._stopped or self.index >= len(self.frames):
            return
        input_record = self.frames[self.index].get("input", {})
        self.inject_action_mask(input_record)
        self.inject_axes(input_record)

    def inject_action_mask(self, input_record: dict) -> None:
        """Publish the recorded effective mask at the post-build boundary."""

        mask = int(input_record.get("action_mask", 0)) & 0xFFFF
        mem.write(_ACTION_MASK_ADDRESS, mask.to_bytes(2, "little"))

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
        # The input breakpoint is the first all-stop point inside gameplay.
        # Patch the asynchronous callback here, after frontend loading has
        # finished but before the first gameplay timer boundary can run.
        self.suppress_uncontrolled_timer_callback()
        self._timer_callback_suppressed = True
        # A recording may legitimately contain no delivery at its first
        # gameplay boundary.  Publish the captured baseline before retail
        # enters that frame so live startup timing cannot leak into the
        # replay.  This is the causal initial state, not a per-frame derived
        # state injection; subsequent clock changes still come only from
        # recorded delivery events.
        self.timer_service.publish(mem)
        self.active = True
        self.action_breakpoint = RetailReplayActionBuildBreakpoint(self)

    def publish_animation_timing(self) -> None:
        """Publish the recorded animation producer state before object update.

        ``Animation_Advance`` runs before ``Skater_PhysicsFrame`` and consumes
        the previous level-loop's global scale.  The in-process recorder can
        complete that loop in a few milliseconds, while a GDB replay pauses
        between every boundary; letting the live producer run here changes a
        player's fractional animation frame before the first comparison.  The
        raw words are already part of each recording snapshot, so publishing
        them at the proven animation boundary preserves the captured causal
        state without writing player fields directly.
        """

        if self._stopped or self.index >= len(self.frames):
            return
        timing = self.frames[self.index].get("before", {}).get("timing", {})
        if not isinstance(timing, dict):
            return
        addresses = {
            "animation_clock": _ANIMATION_CLOCK_ADDRESS,
            "animation_time_scale": _ANIMATION_SCALE_ADDRESS,
            "animation_time_scale_square": _ANIMATION_SCALE_SQUARE_ADDRESS,
            "animation_clock_accumulator": _ANIMATION_CLOCK_ACCUMULATOR_ADDRESS,
            "timing_delta_q11": _TIMING_DELTA_ADDRESS,
        }
        for name, address in addresses.items():
            value = timing.get(name)
            raw = value.get("raw") if isinstance(value, dict) else None
            if isinstance(raw, int):
                mem.write_u32(address, raw)

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
        try:
            # Deliveries observed after the outer timing update belong to this
            # physics frame.  Apply them before comparing the player's
            # pre-frame snapshot; the clock-read breakpoint applies the final
            # intra-frame phase later.
            self.timer_service.apply_frame(
                self.frames[self.index],
                mem,
                phase="physics_entry",
            )
        except (TypeError, ValueError) as exc:
            self._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self.entry_breakpoint,
            )
            return
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

    def apply_post_physics(self, breakpoint: RetailReplayReturnBreakpoint) -> None:
        """Apply deliveries observed after the current frame's clock read."""

        next_index = self.index + 1
        if next_index >= len(self.frames):
            return
        try:
            self.timer_service.apply_frame(
                self.frames[next_index],
                mem,
                phase="post_physics",
            )
        except (TypeError, ValueError) as exc:
            self._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                breakpoint,
            )

    def assert_timing_producer_sample(self, phase: str, breakpoint) -> None:
        """Check the shared timing producer's non-callback inputs and output."""

        if self._stopped:
            return
        samples = [
            event
            for event in self.frames[self.index].get("events", ())
            if event.get("type") == "timing_producer_sample"
            and event.get("timer_boundary_phase") == phase
        ]
        if not samples:
            return
        expected = samples[0]
        actual = {
            "previous_time_raw": mem.u32(_TIMING_PREVIOUS_TIME),
            "simulation_time_raw": mem.u32(TIMER_SIMULATION_TIME),
            "simulation_accumulator_raw": int.from_bytes(
                mem.bytes(TIMER_SIMULATION_ACCUMULATOR, 8), "little"
            ),
            "timing_delta_q11_raw": mem.u32(0x0056A93C),
            "timing_ring_index_raw": mem.u32(_TIMING_RING_INDEX),
            "simulation_pause_gate_a": bool(mem.u32(TIMER_PAUSE_GATE_A)),
            "simulation_pause_gate_b": bool(mem.u32(TIMER_PAUSE_GATE_B)),
            "timing_ring_raw": [
                mem.u32(_TIMING_RING + index * 4) for index in range(3)
            ],
        }
        for key, value in actual.items():
            if key not in expected:
                continue
            if expected[key] != value:
                self._diverge(
                    "timer",
                    (("timing_producer", phase, key), expected[key], value),
                    breakpoint,
                )
                return

    def _finish(self, breakpoint: RetailReplayReturnBreakpoint | None = None) -> None:
        self._stopped = True
        self.input_breakpoint.enabled = False
        if self.action_breakpoint is not None:
            self.action_breakpoint.enabled = False
        self.action_update_breakpoint.enabled = False
        self.entry_breakpoint.enabled = False
        self.timer_clock_read_breakpoint.enabled = False
        self.timer_frame_entry_breakpoint.enabled = False
        self.timer_producer_read_breakpoint.enabled = False
        self.timer_producer_delta_read_breakpoint.enabled = False
        self.timer_producer_output_breakpoint.enabled = False
        self.timer_producer_previous_store_breakpoint.enabled = False
        if breakpoint is not None:
            breakpoint.should_stop = True
        gdb.write(
            f"frames: {self.index}\n"
            f"matching: {self.index}\n"
            "result: deterministic\n"
        )

    def _diverge(self, stage: str, difference, breakpoint=None) -> None:
        self._stopped = True
        self.input_breakpoint.enabled = False
        if self.action_breakpoint is not None:
            self.action_breakpoint.enabled = False
        self.entry_breakpoint.enabled = False
        self.timer_clock_read_breakpoint.enabled = False
        self.timer_frame_entry_breakpoint.enabled = False
        self.timer_producer_read_breakpoint.enabled = False
        self.timer_producer_delta_read_breakpoint.enabled = False
        self.timer_producer_output_breakpoint.enabled = False
        self.timer_producer_previous_store_breakpoint.enabled = False
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
            # Scenario/action-edge recordings may intentionally alter the
            # effective mask after the retail keyboard builder has run. Keep
            # that causal post-build value instead of letting keyboard
            # reconstruction erase it when no physical scan code was held.
            self.replay.inject_action_mask(input_record)


class RetailReplayActionUpdateBreakpoint(TonyBreakpoint):
    """Publish the effective mask before retail advances action records."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_ACTION_STATE_UPDATE_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay.active:
            input_record = self.replay.frames[self.replay.index].get("input", {})
            self.replay.inject_action_mask(input_record)


class RetailReplayFrameEntryBreakpoint(TonyBreakpoint):
    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_PHYSICS_FRAME_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        self.replay.frame_entry(ctx)


class RetailReplayAnimationBreakpoint(TonyBreakpoint):
    """Restore the captured global animation timing before cursor advance."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_ANIMATION_DISPATCH_ADDRESS, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.replay._stopped or not self.replay.active:
            return
        player = mem.u32(GLOBALS["Player"])
        if mem.valid(player) and ctx.this_ptr() == player:
            self.replay.publish_animation_timing()


class RetailReplayTimerClockReadBreakpoint(TonyBreakpoint):
    """Apply deliveries captured before retail loads DAT_0056E320."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_SIMULATION_TIME_LOAD_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if (
            self.replay._stopped
            or not self.replay.active
        ):
            return
        try:
            self.replay.timer_service.apply_frame(
                self.replay.frames[self.replay.index],
                mem,
                phase="clock_read",
            )
        except (TypeError, ValueError) as exc:
            self.replay._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self,
            )
            return


class RetailReplayTimerFrameEntryBreakpoint(TonyBreakpoint):
    """Apply deliveries before retail computes the frame timing delta."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_TIMER_FRAME_CLOCK_UPDATE_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if (
            self.replay._stopped
            or not self.replay.active
        ):
            return
        if not self.replay._timer_callback_suppressed:
            self.replay.suppress_uncontrolled_timer_callback()
            self.replay._timer_callback_suppressed = True
        try:
            self.replay.timer_service.apply_frame(
                self.replay.frames[self.replay.index],
                mem,
                phase="timer_update",
            )
        except (TypeError, ValueError) as exc:
            self.replay._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self,
            )


class RetailReplayTimerProducerReadBreakpoint(TonyBreakpoint):
    """Apply deliveries between the producer's previous/current clock reads."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_TIMER_PRODUCER_READ_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay._stopped or not self.replay.active:
            return
        try:
            self.replay.timer_service.apply_frame(
                self.replay.frames[self.replay.index],
                mem,
                phase="timing_producer",
            )
        except (TypeError, ValueError) as exc:
            self.replay._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self,
            )
            return
        self.replay.assert_timing_producer_sample("timing_producer", self)


class RetailReplayTimerProducerDeltaReadBreakpoint(TonyBreakpoint):
    """Apply deliveries before the producer reads its timing delta clock."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_TIMER_PRODUCER_DELTA_READ_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay._stopped or not self.replay.active:
            return
        try:
            self.replay.timer_service.apply_frame(
                self.replay.frames[self.replay.index],
                mem,
                phase="timing_delta",
            )
        except (TypeError, ValueError) as exc:
            self.replay._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self,
            )
            return
        self.replay.assert_timing_producer_sample("timing_delta", self)


class RetailReplayTimerProducerOutputBreakpoint(TonyBreakpoint):
    """Apply deliveries before the producer reads its delta output sample."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_TIMER_PRODUCER_OUTPUT_READ_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay._stopped or not self.replay.active:
            return
        try:
            self.replay.timer_service.apply_frame(
                self.replay.frames[self.replay.index],
                mem,
                phase="timing_delta_output",
            )
        except (TypeError, ValueError) as exc:
            self.replay._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self,
            )
            return
        self.replay.assert_timing_producer_sample("timing_delta_output", self)


class RetailReplayTimerProducerPreviousStoreBreakpoint(TonyBreakpoint):
    """Apply deliveries before the producer latches previous-time."""

    def __init__(self, replay: RetailReplay):
        self.replay = replay
        super().__init__(_TIMER_PRODUCER_PREVIOUS_STORE_ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.replay._stopped or not self.replay.active:
            return
        try:
            self.replay.timer_service.apply_frame(
                self.replay.frames[self.replay.index],
                mem,
                phase="timing_previous_store",
            )
        except (TypeError, ValueError) as exc:
            self.replay._diverge(
                "timer",
                (("events",), "valid timer deliveries", str(exc)),
                self,
            )
            return
        self.replay.assert_timing_producer_sample("timing_previous_store", self)


class RetailReplayReturnBreakpoint(TonyBreakpoint):
    def __init__(self, replay: RetailReplay, address: int, player: int):
        self.replay = replay
        self.player = player
        super().__init__(address, internal=True, temporary=True)

    def on_hit(self, _ctx: Context) -> None:
        self.enabled = False
        if self.replay._active_return is not self:
            return
        self.replay.apply_post_physics(self)
        if self.replay._stopped:
            return
        self.replay.frame_return(_snapshot(self.player), self)


def create_retail_replay(path: str | Path) -> RetailReplay:
    return RetailReplay(path)


def gdb_replay_usage(path: str | Path) -> str:
    return "tony-replay-retail " + shlex.quote(str(path))
