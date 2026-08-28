from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "re/gdb"))

from opentony.timer import (
    TIMER_PAUSE_GATE_A,
    TIMER_PAUSE_GATE_B,
    TIMER_PUBLIC_ACCUMULATOR,
    TIMER_PUBLIC_TICK,
    TIMER_SIMULATION_ACCUMULATOR,
    TIMER_SIMULATION_TIME,
    TIMER_STATE_ADDRESS,
    TimerReplayService,
    infer_timer_delivery_count,
)


class FakeMemory:
    def __init__(self):
        self.values = {
            TIMER_PAUSE_GATE_A: 0,
            TIMER_PAUSE_GATE_B: 0,
        }
        self.bytes_values = {}

    def readable(self, address, size=1):
        if size == 8:
            return address in self.bytes_values
        return address in self.values

    def u8(self, address):
        return self.values[address] & 0xFF

    def u32(self, address):
        return self.values[address]

    def write_u32(self, address, value):
        self.values[address] = value & 0xFFFFFFFF

    def write(self, address, data):
        self.bytes_values[address] = bytes(data)

    def double(self, address):
        return struct.unpack("<d", self.bytes_values[address])[0]


def _initial_timer_state():
    return {
        "timer_handle": 1,
        "interval_ms": 16,
        "opaque_08": 16,
        "accumulated_ms": 0,
        "opaque_10": 0,
        "public_accumulator": {"raw": 0, "value": 0.0},
        "simulation_accumulator": {"raw": 0, "value": 0.0},
        "public_tick": 0,
        "simulation_time": 0,
    }


def _delivery():
    return {
        "type": "timer_callback_delivery",
        "interval_ms": 16,
        "callback_arg0": 16,
        "callback_arg1": 0,
    }


def test_timer_replay_advances_only_recorded_deliveries():
    memory = FakeMemory()
    service = TimerReplayService(_initial_timer_state())

    results = service.apply_frame(
        {"events": [_delivery(), _delivery()]},
        memory,
    )

    assert len(results) == 2
    assert service.deliveries == 2
    assert service.state.accumulated_ms == 32
    assert service.state.public_accumulator == 1.92
    assert service.state.simulation_accumulator == 1.92
    assert service.state.public_tick == 1
    assert service.state.simulation_time == 1
    assert memory.u32(TIMER_STATE_ADDRESS + 0x0C) == 32
    assert memory.u32(TIMER_PUBLIC_TICK) == 1
    assert memory.u32(TIMER_SIMULATION_TIME) == 1
    assert memory.double(TIMER_PUBLIC_ACCUMULATOR) == 1.92
    assert memory.double(TIMER_SIMULATION_ACCUMULATOR) == 1.92


def test_timer_replay_suppresses_live_callback_after_boundary_model():
    memory = FakeMemory()
    service = TimerReplayService(_initial_timer_state())
    service.apply_frame({"events": [_delivery()]}, memory)

    # Simulate writes made by an uncontrolled callback before its final store.
    memory.write_u32(TIMER_STATE_ADDRESS + 0x0C, 0xDEADBEEF)
    memory.write_u32(TIMER_PUBLIC_TICK, 0xDEADBEEF)
    memory.write_u32(TIMER_SIMULATION_TIME, 0xDEADBEEF)
    register = []

    assert service.suppress_live_callback(
        memory, result_register_setter=register.append
    )
    assert register == [0]
    assert memory.u32(TIMER_STATE_ADDRESS + 0x0C) == 16
    assert memory.u32(TIMER_PUBLIC_TICK) == 0
    assert memory.u32(TIMER_SIMULATION_TIME) == 0


def test_timer_replay_uses_callback_pause_gate_observation():
    memory = FakeMemory()
    service = TimerReplayService(_initial_timer_state())
    event = _delivery()
    event["simulation_pause_gate_a"] = True
    event["simulation_pause_gate_b"] = False

    service.apply_frame({"events": [event]}, memory)

    assert service.state.public_accumulator == 0.96
    assert service.state.simulation_accumulator == 0.0
    assert service.state.public_tick == 0
    assert service.state.simulation_time == 0


def test_timer_replay_applies_only_requested_frame_phase():
    memory = FakeMemory()
    service = TimerReplayService(_initial_timer_state())
    entry = _delivery()
    clock_read = _delivery()
    clock_read["timer_boundary_phase"] = "clock_read"

    assert len(service.apply_frame({"events": [entry, clock_read]}, memory)) == 1
    assert service.state.accumulated_ms == 16
    assert len(
        service.apply_frame(
            {"events": [entry, clock_read]}, memory, phase="clock_read"
        )
    ) == 1
    assert service.state.accumulated_ms == 32


def test_timer_initial_state_is_read_from_recording_header():
    initial = _initial_timer_state()
    assert TimerReplayService.initial_from_recording(
        {"initial_timer_state": initial}, []
    ) == initial


def test_timer_delivery_count_uses_atomic_millisecond_counter():
    assert infer_timer_delivery_count(432, 480, 16) == 3
    assert infer_timer_delivery_count(0xFFFFFFF0, 0x10, 16) == 2


def test_timer_delivery_count_rejects_partial_interval():
    with pytest.raises(ValueError, match="not divisible"):
        infer_timer_delivery_count(0, 15, 16)
