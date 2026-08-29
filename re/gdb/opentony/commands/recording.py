"""GDB recording lifecycle and causal capture commands."""

from __future__ import annotations

import json
import os
import struct

import gdb

from ..breakpoint import Context, TonyBreakpoint
from ..memory import mem
from ..player import canonical_player_snapshot
from ..recording import RecordingController, RecordingError
from ..timer import (
    TIMER_PAUSE_GATE_A,
    TIMER_PAUSE_GATE_B,
    TIMER_PUBLIC_ACCUMULATOR,
    TIMER_PUBLIC_TICK,
    TIMER_SIMULATION_ACCUMULATOR,
    TIMER_SIMULATION_TIME,
    TIMER_STATE_ADDRESS,
    infer_completed_timer_deliveries,
    infer_timer_delivery_count,
)
from ..watchpoint import watchpoints
from .common import (
    argv,
    integer,
    runtime_breakpoints,
    write,
)
from .knowledge import (
    GLOBALS,
    RECORDING_NORMALIZED_AXIS_ADDRESS,
    RECORDING_RAW_AXIS_ADDRESS,
    RECORDING_TIMING_DELTA,
    RECORDING_TIMING_PREVIOUS_STORE,
    RECORDING_TIMING_PREVIOUS_TIME,
    RECORDING_TIMING_RING_INDEX,
    THPS2_ADDRESSES,
    THPS2_BUILD_SHA256,
    THPS2_LEVELS,
)
from .probes import PROBE_FAMILIES, build_probe_family

_recording_controller = None
_recording_event_sink = None
_recording_forensic_families: set[str] = set()
_recording_forensic_breakpoints = []
_recording_timer_initial_state = None
_recording_timer_recording_id = None
_recording_timer_sampler = None


class _RecordingTimerBoundarySampler:
    """Turn callback-owned counter deltas into replay input events.

    The callback runs on Wine's multimedia-timer thread.  A debugger software
    breakpoint at its final store races GDB's remote step-over machinery, so
    recording samples the callback's atomic millisecond counter instead.  A
    delivery increments that counter by the active interval exactly once.
    """

    def __init__(self):
        self._accumulated_ms: int | None = None
        self._interval_ms: int | None = None
        self._boundary_state: dict | None = None
        self._logical_accumulated_ms: int | None = None
        self._pending_deliveries = 0

    @property
    def initialized(self) -> bool:
        return self._accumulated_ms is not None and self._interval_ms is not None

    def reset(self, timer_state: dict | None) -> None:
        if not isinstance(timer_state, dict):
            self._accumulated_ms = None
            self._interval_ms = None
            self._boundary_state = None
            self._logical_accumulated_ms = None
            self._pending_deliveries = 0
            return
        accumulated = timer_state.get("accumulated_ms")
        interval = timer_state.get("interval_ms")
        self._accumulated_ms = None if accumulated is None else int(accumulated)
        self._interval_ms = None if interval is None else int(interval)
        self._boundary_state = dict(timer_state)
        self._logical_accumulated_ms = self._accumulated_ms
        self._pending_deliveries = 0

    def _completed_deliveries(
        self,
        timer_state: dict,
        counter_deliveries: int,
        interval: int,
    ) -> tuple[int, int]:
        """Separate completed callbacks from a callback seen mid-transition.

        The callback increments ``timer_state+0x0c`` before it updates either
        floating accumulator.  A gameplay breakpoint can consequently sample
        one additional counter tick while the callback is still between its
        public and simulation stores.  When the pause gates are open, the
        simulation accumulator gives us a stable completion signal: carry that
        in-flight delivery to the next boundary instead of assigning its
        effect to the earlier producer phase.

        If the accumulator fields are unavailable, or a pause transition makes
        the completion count ambiguous, retain the original counter-delta
        behavior.  Those cases remain covered by the ordinary callback-phase
        observations and are not rewritten from a torn compound snapshot.
        """

        completed, pending = infer_completed_timer_deliveries(
            self._boundary_state,
            timer_state,
            counter_deliveries,
            self._pending_deliveries,
            interval,
            self._interval_ms,
        )
        self._pending_deliveries = pending
        return completed, pending

    def observe(
        self,
        timer_state: dict | None,
        frame: int,
        controller,
        *,
        phase: str = "frame_entry",
    ) -> int:
        if not isinstance(timer_state, dict):
            return 0
        accumulated = timer_state.get("accumulated_ms")
        interval = timer_state.get("interval_ms")
        if accumulated is None or interval is None:
            return 0
        accumulated = int(accumulated)
        interval = int(interval)
        if self._accumulated_ms is None or self._interval_ms is None:
            self._accumulated_ms = accumulated
            self._interval_ms = interval
            self._boundary_state = dict(timer_state)
            self._logical_accumulated_ms = accumulated
            self._pending_deliveries = 0
            return 0
        try:
            deliveries = infer_timer_delivery_count(
                self._accumulated_ms,
                accumulated,
                self._interval_ms,
            )
        except ValueError:
            # If the service changes its interval between boundaries, retry
            # with the newly observed interval.  A non-divisible delta still
            # means this recording cannot express causal deliveries safely.
            deliveries = infer_timer_delivery_count(
                self._accumulated_ms,
                accumulated,
                interval,
            )
        completed, pending = self._completed_deliveries(
            timer_state,
            deliveries,
            interval,
        )
        logical_before = self._logical_accumulated_ms
        if logical_before is None:
            logical_before = self._accumulated_ms
        logical_after = (
            logical_before + completed * self._interval_ms if logical_before is not None else accumulated
        )
        boundary = {
            "timer_boundary_before": {
                "interval_ms": self._interval_ms,
                "accumulated_ms": logical_before,
            },
            "timer_boundary_after": {
                "interval_ms": self._interval_ms,
                "accumulated_ms": logical_after,
            },
            "timer_boundary_sampled_accumulated_ms": accumulated,
            "timer_boundary_pending_delivery_count": pending,
            "timer_boundary_delivery_count": completed,
        }
        for ordinal in range(completed):
            controller.event(
                {
                    **boundary,
                    "type": "timer_callback_delivery",
                    "frame": frame,
                    "callback_ordinal": ordinal,
                    "interval_ms": self._interval_ms,
                    "callback_arg0": self._interval_ms,
                    "callback_arg1": 0,
                    "delivery_source": "timer_state_boundary_delta",
                    "timer_boundary_phase": phase,
                }
            )
        if not completed:
            controller.event(
                {
                    **boundary,
                    "type": "timer_boundary_sample",
                    "frame": frame,
                    "delivery_source": "timer_state_boundary_delta",
                    "timer_boundary_phase": phase,
                }
            )
        self._accumulated_ms = accumulated
        self._interval_ms = interval
        self._logical_accumulated_ms = logical_after
        self._boundary_state = dict(timer_state)
        return completed


_recording_timer_sampler = _RecordingTimerBoundarySampler()


def _recording_timing_producer_sample(phase: str) -> dict:
    """Capture the producer operands that are not callback-owned state."""

    return {
        "type": "timing_producer_sample",
        "frame": _recording_controller.current_frame_index,
        "timer_boundary_phase": phase,
        "previous_time_raw": mem.u32(RECORDING_TIMING_PREVIOUS_TIME),
        "simulation_time_raw": mem.u32(TIMER_SIMULATION_TIME),
        "simulation_accumulator_raw": int.from_bytes(mem.bytes(TIMER_SIMULATION_ACCUMULATOR, 8), "little"),
        "timing_delta_q11_raw": mem.u32(RECORDING_TIMING_DELTA),
        "timing_ring_index_raw": mem.u32(RECORDING_TIMING_RING_INDEX),
        "timing_ring_raw": [mem.u32(0x0056868C + index * 4) for index in range(3)],
        "simulation_pause_gate_a": bool(mem.u32(TIMER_PAUSE_GATE_A)),
        "simulation_pause_gate_b": bool(mem.u32(TIMER_PAUSE_GATE_B)),
    }


class TonyRecordingTimerClockReadBreakpoint(TonyBreakpoint):
    """Sample deliveries that occur before retail reads its simulation clock."""

    ADDRESS = 0x0049F1A0

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(self.ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.controller.active_frame is None:
            return
        timer_state = _recording_timer_boundary_state()
        _recording_timer_sampler.observe(
            timer_state,
            self.controller.active_frame,
            self.controller,
            phase="clock_read",
        )


class TonyRecordingTimerUpdateBreakpoint(TonyBreakpoint):
    """Sample deliveries before retail computes the frame timing delta."""

    ADDRESS = 0x0046A0F0

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(self.ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        global _recording_timer_initial_state, _recording_timer_recording_id
        if self.controller.state.value == "Idle":
            return
        if self.controller.recording_id != _recording_timer_recording_id:
            _recording_timer_recording_id = self.controller.recording_id
            _recording_timer_initial_state = None
            _recording_timer_sampler.reset(None)
        timer_state = _recording_timer_boundary_state()
        if not _recording_timer_sampler.initialized:
            _recording_timer_sampler.reset(timer_state)
            _recording_timer_initial_state = _recording_timer_state()
            return
        _recording_timer_sampler.observe(
            timer_state,
            self.controller.current_frame_index,
            self.controller,
            phase="timer_update",
        )


class TonyRecordingTimerProducerReadBreakpoint(TonyBreakpoint):
    """Sample deliveries after the timing producer latches its old clock."""

    # FUN_00468b30 loads DAT_00568604 before reading DAT_0056e320.  A timer
    # delivery between those two reads contributes to the current delta while
    # leaving the producer's previous-time operand unchanged.
    ADDRESS = 0x00468B40

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(self.ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.controller.state.value == "Idle":
            return
        _recording_timer_sampler.observe(
            _recording_timer_boundary_state(),
            self.controller.current_frame_index,
            self.controller,
            phase="timing_producer",
        )
        self.controller.event(_recording_timing_producer_sample("timing_producer"))


class TonyRecordingTimerProducerDeltaReadBreakpoint(TonyBreakpoint):
    """Sample deliveries before the producer reads its timing delta clock."""

    ADDRESS = 0x00468B6B

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(self.ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.controller.state.value == "Idle":
            return
        _recording_timer_sampler.observe(
            _recording_timer_boundary_state(),
            self.controller.current_frame_index,
            self.controller,
            phase="timing_delta",
        )
        self.controller.event(_recording_timing_producer_sample("timing_delta"))


class TonyRecordingTimerProducerOutputBreakpoint(TonyBreakpoint):
    """Sample deliveries before the producer reads its timing-delta output."""

    ADDRESS = 0x00468B7C

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(self.ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.controller.state.value == "Idle":
            return
        _recording_timer_sampler.observe(
            _recording_timer_boundary_state(),
            self.controller.current_frame_index,
            self.controller,
            phase="timing_delta_output",
        )
        self.controller.event(_recording_timing_producer_sample("timing_delta_output"))


class TonyRecordingTimerProducerPreviousStoreBreakpoint(TonyBreakpoint):
    """Sample deliveries before the producer latches previous-time."""

    ADDRESS = RECORDING_TIMING_PREVIOUS_STORE

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(self.ADDRESS, internal=True)

    def on_hit(self, _ctx: Context) -> None:
        if self.controller.state.value == "Idle":
            return
        _recording_timer_sampler.observe(
            _recording_timer_boundary_state(),
            self.controller.current_frame_index,
            self.controller,
            phase="timing_previous_store",
        )
        self.controller.event(_recording_timing_producer_sample("timing_previous_store"))


def _recording_signed8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def _recording_input(controller: RecordingController) -> tuple[dict, bool]:
    keyboard = mem.bytes(GLOBALS["KeyboardState"], 0x100)
    hotkey = controller.hotkey_scan_code
    raw_axes = (
        list(mem.bytes(RECORDING_RAW_AXIS_ADDRESS, 4))
        if mem.readable(RECORDING_RAW_AXIS_ADDRESS, 4)
        else None
    )
    normalized_axes = (
        {
            "horizontal": _recording_signed8(mem.u8(RECORDING_NORMALIZED_AXIS_ADDRESS)),
            "vertical": _recording_signed8(mem.u8(RECORDING_NORMALIZED_AXIS_ADDRESS + 1)),
        }
        if mem.readable(RECORDING_NORMALIZED_AXIS_ADDRESS, 2)
        else None
    )
    return (
        {
            "action_mask": mem.u16(GLOBALS["ActionMask"]),
            "action_mask_raw_u32": mem.u32(GLOBALS["ActionMask"]),
            "raw_controller_sample": raw_axes,
            "raw_axes": {
                "device_bytes": (
                    [{"raw": value, "signed": _recording_signed8(value)} for value in raw_axes]
                    if raw_axes is not None
                    else None
                ),
                "normalized": normalized_axes,
            },
            "normalized_axes": normalized_axes,
            "keyboard_state": keyboard.hex(),
            "held_scan_codes": [
                code for code, value in enumerate(keyboard) if value & 0x80 and code != hotkey
            ],
            "control_hotkey_scan_code": hotkey,
        },
        bool(keyboard[hotkey] & 0x80),
    )


def _recording_player_snapshot(player: int) -> dict:
    return canonical_player_snapshot(player, mem)


def _recording_metadata(player: int) -> dict:
    level = mem.u32(GLOBALS["CurrentLevel"])
    return {
        "binary_sha256": THPS2_BUILD_SHA256,
        "retail_executable_sha256": THPS2_BUILD_SHA256,
        "instrumentation_version": "gdb-recording-v6",
        "level": {
            "index": level,
            "name": next((name for name, index in THPS2_LEVELS.items() if index == level), None),
        },
        "player_identity": {
            "slot": 0,
            "object_address": f"0x{player:08x}",
        },
    }


def _recording_timer_state() -> dict | None:
    """Capture the callback-owned timer state once per recording.

    This is initialization state, not a replayed clock value.  Subsequent
    timer values are reconstructed from the recorded delivery events.
    """

    if not mem.readable(TIMER_STATE_ADDRESS, 0x14):
        return None
    record = {
        name: mem.u32(TIMER_STATE_ADDRESS + offset)
        for name, offset in (
            ("timer_handle", 0x00),
            ("interval_ms", 0x04),
            ("opaque_08", 0x08),
            ("accumulated_ms", 0x0C),
            ("opaque_10", 0x10),
        )
    }
    for name, address in (
        ("public_accumulator", TIMER_PUBLIC_ACCUMULATOR),
        ("simulation_accumulator", TIMER_SIMULATION_ACCUMULATOR),
    ):
        if not mem.readable(address, 8):
            return None
        raw = int.from_bytes(mem.bytes(address, 8), "little")
        record[name] = {
            "raw": raw,
            "value": struct.unpack("<d", raw.to_bytes(8, "little"))[0],
        }
    record.update(
        {
            "public_tick": mem.u32(TIMER_PUBLIC_TICK),
            "simulation_time": mem.u32(TIMER_SIMULATION_TIME),
            "simulation_pause_gate_a": bool(mem.u32(TIMER_PAUSE_GATE_A)),
            "simulation_pause_gate_b": bool(mem.u32(TIMER_PAUSE_GATE_B)),
        }
    )
    return record


def _recording_timer_boundary_state() -> dict | None:
    """Read the atomic callback counter used by boundary delivery sampling."""

    if not mem.readable(TIMER_STATE_ADDRESS, 0x10):
        return None
    record = {
        "interval_ms": mem.u32(TIMER_STATE_ADDRESS + 0x04),
        "accumulated_ms": mem.u32(TIMER_STATE_ADDRESS + 0x0C),
    }
    for name, address in (
        ("public_accumulator", TIMER_PUBLIC_ACCUMULATOR),
        ("simulation_accumulator", TIMER_SIMULATION_ACCUMULATOR),
    ):
        if mem.readable(address, 8):
            raw = int.from_bytes(mem.bytes(address, 8), "little")
            record[name] = {
                "raw": raw,
                "value": struct.unpack("<d", raw.to_bytes(8, "little"))[0],
            }
    for name, address in (
        ("public_tick", TIMER_PUBLIC_TICK),
        ("simulation_time", TIMER_SIMULATION_TIME),
        ("simulation_pause_gate_a", TIMER_PAUSE_GATE_A),
        ("simulation_pause_gate_b", TIMER_PAUSE_GATE_B),
    ):
        if mem.readable(address, 4):
            record[name] = mem.u32(address)
    return record


def _recording_timer_capture_initial(controller: RecordingController) -> None:
    """Latch the timer baseline when a host start command is first observed."""

    global _recording_timer_initial_state, _recording_timer_recording_id
    if controller.state.value != "StartPending" or controller.active_frame is not None:
        return
    _recording_timer_recording_id = controller.recording_id
    _recording_timer_initial_state = _recording_timer_state()
    _recording_timer_sampler.reset(_recording_timer_boundary_state())


class _RecordingEventSink:
    """Adapter used by existing probes to append into the active frame."""

    def __init__(self, controller: RecordingController):
        self.controller = controller

    def event(self, record: dict) -> None:
        self.controller.event(record)


_RECORDING_FORENSIC_FAMILIES = (
    "collision",
    "service",
    "recovery",
    "rng",
    "animation",
    "correction",
    "state",
    "position",
    "timing",
)
_RECORDING_FORENSIC_ALIASES = {
    "shared-service": "service",
    "state-transition": "state",
}


def _recording_frame_provider() -> int | None:
    if _recording_controller is None:
        return None
    return _recording_controller.active_frame


def _recording_append_forensic(*breakpoints) -> int:
    for breakpoint in breakpoints:
        runtime_breakpoints.append(breakpoint)
        _recording_forensic_breakpoints.append(breakpoint)
    return len(breakpoints)


class _RecordingCorrectionWatchArm(TonyBreakpoint):
    """Arm exact transient-correction watches once the player exists."""

    PHYSICS_FRAME_ENTRY = 0x0049E680
    CORRECTION_OFFSETS = (0x58, 0x5C, 0x60)

    def __init__(self, writer):
        self.writer = writer
        self.armed = False
        super().__init__(self.PHYSICS_FRAME_ENTRY, internal=True)

    def on_hit(self, ctx: Context) -> None:
        if self.armed:
            return
        player = ctx.this_ptr()
        if not ctx.memory.valid(player):
            return
        if watchpoints.available() < len(self.CORRECTION_OFFSETS):
            write(
                "could not arm recording correction watches: "
                f"need {len(self.CORRECTION_OFFSETS)} hardware slots"
            )
            self.enabled = False
            return
        for offset in self.CORRECTION_OFFSETS:
            watchpoints.arm(
                player + offset,
                size=4,
                label=f"recording.correction+0x{offset:02x}",
                limit=1000,
                writer=self.writer,
            )
        self.armed = True
        self.enabled = False
        write(f"armed recording correction watches at 0x{player:08x}+0x58/0x5c/0x60")


def _recording_arm_forensic_family(family: str) -> int:
    """Arm a declaratively registered diagnostic family."""

    if family in _recording_forensic_families:
        return 0
    if _recording_event_sink is None:
        raise gdb.GdbError("recording instrumentation is not initialized")
    if family not in PROBE_FAMILIES:
        raise gdb.GdbError(
            f"unknown forensic family {family!r}; choose from "
            + ", ".join((*_RECORDING_FORENSIC_FAMILIES, "all", "clear"))
        )
    breakpoints = build_probe_family(
        family,
        writer=_recording_event_sink,
        frame_provider=_recording_frame_provider,
        controller=_recording_controller,
        watch_arm_factory=_RecordingCorrectionWatchArm,
    )
    count = _recording_append_forensic(*breakpoints)
    _recording_forensic_families.add(family)
    return count


def _recording_clear_forensic() -> int:
    for breakpoint in _recording_forensic_breakpoints:
        breakpoint.enabled = False
    count = len(_recording_forensic_breakpoints)
    _recording_forensic_breakpoints.clear()
    _recording_forensic_families.clear()
    return count


class TonyRecordingInputBreakpoint(TonyBreakpoint):
    """Observe post-poll input and detect the recorder hotkey edge."""

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(THPS2_ADDRESSES["gameplay_update"][0], internal=True)

    def on_hit(self, _ctx: Context) -> None:
        self.controller.poll_control()
        _recording_timer_capture_initial(self.controller)
        input_record, hotkey_down = _recording_input(self.controller)
        try:
            self.controller.on_input(input_record, hotkey_down=hotkey_down)
        except RecordingError as exc:
            self.controller.set_error(str(exc))


class TonyRecordingFrameReturnBreakpoint(TonyBreakpoint):
    """Finalize one physics frame at the return address captured at entry."""

    def __init__(
        self,
        controller: RecordingController,
        address: int,
        frame: int,
        player: int,
    ):
        self.controller = controller
        self.frame = frame
        self.player = player
        super().__init__(address, internal=True, temporary=True)

    def on_hit(self, _ctx: Context) -> None:
        self.enabled = False
        if self.controller.active_frame != self.frame:
            return
        # The callback can arrive after the exact simulation-clock load but
        # before the physics wrapper returns.  Those deliveries are too late
        # to affect this player's store, but they are visible in this frame's
        # after snapshot.  Keep the sample on the next frame record (the
        # current frame has already been opened) and label it as the causal
        # post-physics phase for replay.
        _recording_timer_sampler.observe(
            _recording_timer_boundary_state(),
            self.frame + 1,
            self.controller,
            phase="post_physics",
        )
        try:
            self.controller.end_frame(_recording_player_snapshot(self.player))
            if self.controller.exit_on_complete and self.controller.state.name == "IDLE":
                gdb.execute("quit")
        except (OSError, TypeError, ValueError, RecordingError) as exc:
            # Recording is an observer.  A malformed/unsupported probe value
            # must end the file cleanly and never leave the gameplay thread
            # stopped at an internal temporary breakpoint.
            self.controller.close_incomplete(f"frame-{self.frame}-finalization-failed: {exc}")


class TonyRecordingFrameEntryBreakpoint(TonyBreakpoint):
    """Begin capture at the canonical per-player physics-frame boundary."""

    def __init__(self, controller: RecordingController):
        self.controller = controller
        super().__init__(THPS2_ADDRESSES["physics_frame"][0], internal=True)

    def on_hit(self, ctx: Context) -> None:
        player = ctx.this_ptr()
        current = mem.u32(GLOBALS["Player"])
        if not mem.valid(player) or player != current:
            return
        input_record = self.controller.latest_input
        if input_record is None:
            input_record, _hotkey_down = _recording_input(self.controller)
        frame_index = self.controller.current_frame_index
        # The metadata is written only with the first header. Avoid rereading
        # the level/global identity on every subsequent frame.
        metadata = _recording_metadata(player) if frame_index == 0 else None
        timer_state = _recording_timer_boundary_state()
        if frame_index == 0:
            # The input boundary latches the canonical initial state before
            # the first recorded physics frame.  Keep a fallback for
            # recordings started while that boundary was not yet reached.
            initial_timer_state = _recording_timer_initial_state
            if initial_timer_state is None:
                initial_timer_state = _recording_timer_state()
                _recording_timer_sampler.reset(timer_state)
            if initial_timer_state is not None:
                metadata["initial_timer_state"] = initial_timer_state
        _recording_timer_sampler.observe(
            timer_state,
            frame_index,
            self.controller,
            phase="physics_entry",
        )
        try:
            frame = self.controller.begin_frame(
                _recording_player_snapshot(player),
                input_record=input_record,
                metadata=metadata,
            )
        except RecordingError as exc:
            raise gdb.GdbError(str(exc)) from exc
        if frame is None:
            return
        return_address = ctx.return_address()
        if return_address == 0:
            raise gdb.GdbError("could not install recording frame return breakpoint")
        runtime_breakpoints.append(
            TonyRecordingFrameReturnBreakpoint(
                self.controller,
                return_address,
                frame,
                player,
            )
        )


class TonyRecordingStart(gdb.Command):
    """tony-record-start [FILE] [--force] [--frames COUNT] [--quit] -- start capture."""

    def __init__(self):
        super().__init__("tony-record-start", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(
            arg,
            "tony-record-start [FILE] [--force] [--frames COUNT] [--quit]",
        ) if arg.strip() else []
        force = False
        exit_on_complete = False
        frame_limit = None
        positional = []
        index = 0
        while index < len(values):
            value = values[index]
            if value == "--force":
                force = True
            elif value == "--quit":
                exit_on_complete = True
            elif value == "--frames":
                index += 1
                if index >= len(values):
                    raise gdb.GdbError("--frames requires a positive count")
                frame_limit = integer(values[index])
            elif value.startswith("--frames="):
                frame_limit = integer(value.split("=", 1)[1])
            else:
                positional.append(value)
            index += 1
        values = positional
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-record-start [FILE] [--force] [--frames COUNT]")
        if frame_limit is not None and frame_limit <= 0:
            raise gdb.GdbError("--frames must be a positive count")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        _recording_controller.exit_on_complete = exit_on_complete
        global _recording_timer_initial_state, _recording_timer_recording_id
        _recording_timer_initial_state = None
        _recording_timer_recording_id = None
        _recording_timer_sampler.reset(None)
        try:
            recording_id = _recording_controller.request_start(
                values[0] if values else None,
                overwrite=force,
                frame_limit=frame_limit,
            )
        except RecordingError as exc:
            raise gdb.GdbError(str(exc)) from exc
        write(
            f"recording start pending: {recording_id}; "
            f"path {_recording_controller.path}; "
            f"frame limit {frame_limit if frame_limit is not None else 'none'}"
        )


class TonyRecordingStop(gdb.Command):
    """tony-record-stop -- close after the current complete physics frame."""

    def __init__(self):
        super().__init__("tony-record-stop", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        if arg.strip():
            raise gdb.GdbError("usage: tony-record-stop")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        if not _recording_controller.request_stop():
            write("no recording is active")
            return
        write("recording stop pending at the next physics-frame return")


class TonyRecordingToggle(gdb.Command):
    """tony-record-toggle [FILE] [--force] -- share the hotkey lifecycle."""

    def __init__(self):
        super().__init__("tony-record-toggle", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(arg, "tony-record-toggle [FILE] [--force]") if arg.strip() else []
        force = values[-1:] == ["--force"]
        if force:
            values.pop()
        if len(values) > 1:
            raise gdb.GdbError("usage: tony-record-toggle [FILE] [--force]")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        try:
            recording_id = _recording_controller.request_toggle(
                values[0] if values else None,
                overwrite=force,
            )
        except RecordingError as exc:
            raise gdb.GdbError(str(exc)) from exc
        write(f"recording state: {_recording_controller.state.value} ({recording_id})")


class TonyRecordingStatus(gdb.Command):
    """tony-record-status -- print the controller's current state."""

    def __init__(self):
        super().__init__("tony-record-status", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        del from_tty
        if arg.strip():
            raise gdb.GdbError("usage: tony-record-status")
        if _recording_controller is None:
            raise gdb.GdbError("recording controller is not initialized")
        write(json.dumps(_recording_controller.status(), sort_keys=True))


class TonyRecordingForensic(gdb.Command):
    """tony-record-forensic FAMILY... -- add selected diagnostic probes."""

    def __init__(self):
        super().__init__("tony-record-forensic", gdb.COMMAND_BREAKPOINTS)

    def invoke(self, arg, from_tty):
        del from_tty
        values = argv(
            arg,
            "tony-record-forensic FAMILY... | clear",
        )
        if values == ["clear"]:
            write(f"disabled {_recording_clear_forensic()} recording forensic breakpoints")
            return
        if "clear" in values:
            raise gdb.GdbError("clear cannot be combined with a forensic family")
        families = []
        for value in values:
            family = _RECORDING_FORENSIC_ALIASES.get(value.casefold(), value.casefold())
            if family == "all":
                families.extend(_RECORDING_FORENSIC_FAMILIES)
            else:
                families.append(family)
        armed = 0
        for family in dict.fromkeys(families):
            armed += _recording_arm_forensic_family(family)
        selected = ", ".join(dict.fromkeys(families))
        write(f"recording forensic families armed: {selected} ({armed} breakpoints)")


def install_recording_instrumentation() -> None:
    """Install the always-on canonical capture boundaries once per inferior."""

    global _recording_controller, _recording_event_sink
    if _recording_controller is not None:
        return
    session_dir = os.environ.get("TONY_SESSION_DIR")
    _recording_controller = RecordingController(session_dir=session_dir)
    _recording_event_sink = _RecordingEventSink(_recording_controller)
    runtime_breakpoints.extend(
        (
            TonyRecordingInputBreakpoint(_recording_controller),
            TonyRecordingFrameEntryBreakpoint(_recording_controller),
            TonyRecordingTimerUpdateBreakpoint(_recording_controller),
            TonyRecordingTimerClockReadBreakpoint(_recording_controller),
        )
    )
