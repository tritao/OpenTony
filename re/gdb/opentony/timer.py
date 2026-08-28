"""Deterministic replay service for the retail multimedia timer callback.

The callback delivery is an external input.  Its integer clocks and floating
accumulators are derived state, so replay advances this state from the
recorded delivery count instead of restoring the recorded clock outputs.
"""

from __future__ import annotations

import math
import struct
from dataclasses import dataclass

TIMER_STATE_ADDRESS = 0x006A05A0
TIMER_PUBLIC_ACCUMULATOR = 0x006A0590
TIMER_SIMULATION_ACCUMULATOR = 0x006A0598
TIMER_PUBLIC_TICK = 0x0056E31C
TIMER_SIMULATION_TIME = 0x0056E320
TIMER_PAUSE_GATE_A = 0x00561C04
TIMER_PAUSE_GATE_B = 0x0056A8E0


def infer_timer_delivery_count(
    previous_accumulated_ms: int,
    current_accumulated_ms: int,
    interval_ms: int,
) -> int:
    """Infer completed callback deliveries from the timer state counter.

    ``SimulationTimeTimerCallback`` updates the 32-bit accumulated-millisecond
    field once per delivery.  Sampling that single word at two all-stop
    gameplay boundaries therefore gives an exact delivery count, including
    across the natural 32-bit wrap, provided the callback interval is stable.
    """

    interval = int(interval_ms) & 0xFFFFFFFF
    if interval <= 0:
        raise ValueError(f"timer interval must be positive, got {interval_ms!r}")
    delta = (int(current_accumulated_ms) - int(previous_accumulated_ms)) & 0xFFFFFFFF
    if delta % interval:
        raise ValueError(
            "timer accumulated-millisecond delta is not divisible by interval: "
            f"{delta} % {interval}"
        )
    return delta // interval


def _integer(value, default: int = 0) -> int:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        try:
            return int(value, 0)
        except ValueError:
            return default
    return default


def _raw_double(value) -> tuple[int, float]:
    if isinstance(value, dict):
        raw = value.get("raw")
        if isinstance(raw, int):
            raw &= 0xFFFFFFFFFFFFFFFF
            return raw, struct.unpack("<d", raw.to_bytes(8, "little"))[0]
        value = value.get("value", 0.0)
    try:
        floating = float(value)
    except (TypeError, ValueError):
        floating = 0.0
    raw = int.from_bytes(struct.pack("<d", floating), "little")
    return raw, floating


def _raw_from_double(value: float) -> int:
    """Model FUN_005004f4's truncate-toward-zero low-word result."""

    if not math.isfinite(value):
        return 0
    return math.trunc(value) & 0xFFFFFFFF


@dataclass(slots=True)
class RetailTimerState:
    timer_handle: int = 0
    interval_ms: int = 0
    opaque_08: int = 0
    accumulated_ms: int = 0
    opaque_10: int = 0
    public_accumulator: float = 0.0
    simulation_accumulator: float = 0.0
    simulation_pause_gate_a: bool = False
    simulation_pause_gate_b: bool = False
    public_tick: int = 0
    simulation_time: int = 0

    @classmethod
    def from_record(cls, record: dict | None) -> RetailTimerState | None:
        if not isinstance(record, dict):
            return None
        public = record.get("public_accumulator", record.get("public_accumulator_after", 0.0))
        simulation = record.get(
            "simulation_accumulator",
            record.get("simulation_accumulator_after", 0.0),
        )
        _public_raw, public_value = _raw_double(public)
        _simulation_raw, simulation_value = _raw_double(simulation)
        return cls(
            timer_handle=_integer(record.get("timer_handle")),
            interval_ms=_integer(record.get("interval_ms")) & 0xFFFFFFFF,
            opaque_08=_integer(record.get("opaque_08")) & 0xFFFFFFFF,
            accumulated_ms=_integer(record.get("accumulated_ms")) & 0xFFFFFFFF,
            opaque_10=_integer(record.get("opaque_10")) & 0xFFFFFFFF,
            public_accumulator=public_value,
            simulation_accumulator=simulation_value,
            simulation_pause_gate_a=bool(record.get("simulation_pause_gate_a", False)),
            simulation_pause_gate_b=bool(record.get("simulation_pause_gate_b", False)),
            public_tick=_integer(record.get("public_tick", record.get("public_tick_raw")))
            & 0xFFFFFFFF,
            simulation_time=_integer(
                record.get("simulation_time", record.get("simulation_time_raw"))
            )
            & 0xFFFFFFFF,
        )

    def copy(self) -> RetailTimerState:
        return RetailTimerState(
            self.timer_handle,
            self.interval_ms,
            self.opaque_08,
            self.accumulated_ms,
            self.opaque_10,
            self.public_accumulator,
            self.simulation_accumulator,
            self.simulation_pause_gate_a,
            self.simulation_pause_gate_b,
            self.public_tick,
            self.simulation_time,
        )

    def advance(self, *, interval_ms: int | None = None) -> dict:
        """Apply one callback delivery and return the derived observation."""

        before = self.accumulated_ms
        interval = self.interval_ms if interval_ms is None else interval_ms
        if interval_ms is not None and interval != self.interval_ms:
            raise ValueError(
                "timer callback interval differs from captured timer state: "
                f"{interval} != {self.interval_ms}"
            )
        self.accumulated_ms = (self.accumulated_ms + interval) & 0xFFFFFFFF
        # This follows the long-double intermediate used by the native model;
        # the final assignment rounds to the inferior's PE32 double.
        delta = float(interval) * 0.001 * 60.0
        self.public_accumulator = float(self.public_accumulator + delta)
        advanced = not self.simulation_pause_gate_a and not self.simulation_pause_gate_b
        if advanced:
            self.simulation_accumulator = float(self.simulation_accumulator + delta)
        self.public_tick = _raw_from_double(self.public_accumulator)
        self.simulation_time = _raw_from_double(self.simulation_accumulator)
        return {
            "accumulated_ms_before": before,
            "accumulated_ms_after": self.accumulated_ms,
            "interval_delta": delta,
            "public_accumulator": self.public_accumulator,
            "simulation_accumulator": self.simulation_accumulator,
            "simulation_advanced": advanced,
            "public_tick": self.public_tick,
            "simulation_time": self.simulation_time,
        }


def advance_timer(state: RetailTimerState, event: dict | None = None) -> dict:
    """Python mirror of ``camera::advance_timer`` for the GDB replay path."""

    interval = None
    if isinstance(event, dict) and event.get("interval_ms") is not None:
        interval = _integer(event["interval_ms"])
    return state.advance(interval_ms=interval)


class TimerReplayService:
    """Own the deterministic timer state used by strict retail replay."""

    EVENT_TYPE = "timer_callback_delivery"
    BOUNDARY_EVENT_TYPE = "timer_boundary_sample"

    def __init__(self, initial_record: dict | None = None):
        self.state = RetailTimerState.from_record(initial_record)
        self.initial_record = initial_record
        self.initialized = self.state is not None
        self.deliveries = 0

    @property
    def available(self) -> bool:
        return self.state is not None

    @staticmethod
    def initial_from_recording(header: dict, frames: list[dict]) -> dict | None:
        initial = header.get("initial_timer_state") if isinstance(header, dict) else None
        if isinstance(initial, dict):
            return initial
        for frame in frames:
            for event in frame.get("events", ()):
                if event.get("type") != TimerReplayService.EVENT_TYPE:
                    continue
                before = event.get("timer_state_before")
                if not isinstance(before, dict):
                    before = event.get("timer_state_at_store")
                if not isinstance(before, dict):
                    continue
                candidate = dict(before)
                interval = _integer(event.get("interval_ms", candidate.get("interval_ms")))
                accumulated = _integer(candidate.get("accumulated_ms"))
                candidate["interval_ms"] = interval
                candidate["accumulated_ms"] = (accumulated - interval) & 0xFFFFFFFF
                after_public = event.get("public_accumulator_after")
                after_simulation = event.get("simulation_accumulator_after")
                delta = float(interval) * 0.001 * 60.0
                _raw, public_value = _raw_double(after_public)
                _raw, simulation_value = _raw_double(after_simulation)
                candidate["public_accumulator"] = public_value - delta
                candidate["simulation_accumulator"] = simulation_value - delta
                candidate["public_tick"] = _raw_from_double(
                    candidate["public_accumulator"]
                )
                candidate["simulation_time"] = _raw_from_double(
                    candidate["simulation_accumulator"]
                )
                return candidate
        return None

    @staticmethod
    def _read_gate(memory, address: int) -> bool | None:
        if memory.readable(address, 1):
            return bool(memory.u8(address))
        if memory.readable(address, 4):
            return bool(memory.u32(address))
        return None

    def _read_gates(self, memory) -> None:
        gate_a = self._read_gate(memory, TIMER_PAUSE_GATE_A)
        gate_b = self._read_gate(memory, TIMER_PAUSE_GATE_B)
        if gate_a is not None:
            self.state.simulation_pause_gate_a = gate_a
        if gate_b is not None:
            self.state.simulation_pause_gate_b = gate_b

    def _write_double(self, memory, address: int, value: float) -> None:
        memory.write(address, struct.pack("<d", value))

    @staticmethod
    def _matches_raw_double(expected, actual: float) -> bool:
        if expected is None:
            return True
        expected_raw, _expected_value = _raw_double(expected)
        actual_raw = int.from_bytes(struct.pack("<d", actual), "little")
        return expected_raw == actual_raw

    @classmethod
    def validate_event(cls, event: dict, result: dict) -> None:
        """Check model output against captured observations, if present."""

        state_after = event.get("timer_state_after", event.get("timer_state_at_store"))
        if isinstance(state_after, dict):
            observed_accumulated = state_after.get("accumulated_ms")
            if observed_accumulated is not None and _integer(observed_accumulated) != result["accumulated_ms_after"]:
                raise ValueError(
                    "captured timer accumulated_ms does not match model: "
                    f"{observed_accumulated} != {result['accumulated_ms_after']}"
                )
        observed_public = event.get("public_accumulator_after")
        if not cls._matches_raw_double(observed_public, result["public_accumulator"]):
            raise ValueError("captured public timer accumulator does not match model")
        observed_simulation = event.get("simulation_accumulator_after")
        if not cls._matches_raw_double(
            observed_simulation, result["simulation_accumulator"]
        ):
            raise ValueError("captured simulation timer accumulator does not match model")
        for key, result_key in (
            ("public_tick_after_raw", "public_tick"),
            ("simulation_time_result_raw", "simulation_time"),
        ):
            observed = event.get(key)
            if observed is not None and _integer(observed) & 0xFFFFFFFF != result[result_key]:
                raise ValueError(
                    f"captured {key} does not match model: "
                    f"{observed} != {result[result_key]}"
                )

    def publish(self, memory) -> None:
        if self.state is None:
            return
        state = self.state
        for offset, value in (
            (0x00, state.timer_handle),
            (0x04, state.interval_ms),
            (0x08, state.opaque_08),
            (0x0C, state.accumulated_ms),
            (0x10, state.opaque_10),
        ):
            memory.write_u32(TIMER_STATE_ADDRESS + offset, value)
        self._write_double(memory, TIMER_PUBLIC_ACCUMULATOR, state.public_accumulator)
        self._write_double(memory, TIMER_SIMULATION_ACCUMULATOR, state.simulation_accumulator)
        memory.write_u32(TIMER_PUBLIC_TICK, state.public_tick)
        memory.write_u32(TIMER_SIMULATION_TIME, state.simulation_time)

    def apply_frame(
        self,
        frame: dict,
        memory,
        *,
        phase: str = "physics_entry",
    ) -> list[dict]:
        """Advance deliveries recorded for one deterministic frame phase."""

        if self.state is None:
            return []
        results = []
        self._read_gates(memory)
        phase_events = []
        for event in frame.get("events", ()):
            if event.get("type") not in (self.EVENT_TYPE, self.BOUNDARY_EVENT_TYPE):
                continue
            # Captures made before the phase-aware timer service used the
            # generic frame_entry label.  Those deliveries occurred between
            # the outer timing update and Skater_PhysicsFrame, so retain that
            # fixture compatibility as the physics_entry phase.
            event_phase = event.get("timer_boundary_phase", "physics_entry")
            if event_phase == "frame_entry":
                event_phase = "physics_entry"
            if event_phase != phase:
                continue
            phase_events.append(event)

        boundary_keys = (
            "timer_boundary_before",
            "timer_boundary_after",
            "timer_boundary_delivery_count",
        )
        segments = []
        for event in phase_events:
            metadata = (
                tuple(event.get(key) for key in boundary_keys)
                if any(key in event for key in boundary_keys)
                else None
            )
            if metadata is not None:
                boundary_before, boundary_after, delivery_count = metadata
                if not isinstance(boundary_before, dict) or not isinstance(
                    boundary_after, dict
                ) or not isinstance(delivery_count, int):
                    raise TypeError("timer boundary metadata is incomplete")
            if segments and segments[-1][0] == metadata:
                segments[-1][1].append(event)
            else:
                segments.append([metadata, [event]])

        for metadata, segment_events in segments:
            if metadata is not None:
                boundary_before, boundary_after, delivery_count = metadata
                expected_accumulated = _integer(boundary_before.get("accumulated_ms"))
                if self.state.accumulated_ms != (expected_accumulated & 0xFFFFFFFF):
                    raise ValueError(
                        "modeled timer state does not reach recorded boundary start: "
                        f"{self.state.accumulated_ms} != {expected_accumulated}"
                    )
                observed_count = sum(
                    event.get("type") == self.EVENT_TYPE
                    for event in segment_events
                )
                if observed_count != delivery_count:
                    raise ValueError(
                        "recorded timer boundary delivery count is inconsistent: "
                        f"{delivery_count} != {observed_count}"
                    )

            for event in segment_events:
                if event.get("type") != self.EVENT_TYPE:
                    continue
                if event.get("simulation_pause_gate_a") is not None:
                    self.state.simulation_pause_gate_a = bool(
                        event["simulation_pause_gate_a"]
                    )
                if event.get("simulation_pause_gate_b") is not None:
                    self.state.simulation_pause_gate_b = bool(
                        event["simulation_pause_gate_b"]
                    )
                if event.get("simulation_pause_gate_a") is None:
                    gate_a = self._read_gate(memory, TIMER_PAUSE_GATE_A)
                    if gate_a is not None:
                        self.state.simulation_pause_gate_a = gate_a
                if event.get("simulation_pause_gate_b") is None:
                    gate_b = self._read_gate(memory, TIMER_PAUSE_GATE_B)
                    if gate_b is not None:
                        self.state.simulation_pause_gate_b = gate_b
                interval = event.get("interval_ms")
                result = advance_timer(self.state, {"interval_ms": interval})
                self.validate_event(event, result)
                results.append(result)
                self.deliveries += 1

            if metadata is not None:
                expected_accumulated = _integer(boundary_after.get("accumulated_ms"))
                if self.state.accumulated_ms != (expected_accumulated & 0xFFFFFFFF):
                    raise ValueError(
                        "modeled timer state does not reach recorded boundary end: "
                        f"{self.state.accumulated_ms} != {expected_accumulated}"
                    )
        self.publish(memory)
        return results
