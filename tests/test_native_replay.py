from __future__ import annotations

import pytest

from tony.native_replay import _parse_native_output, _wire_input


def _snapshot(value: int) -> dict:
    return {
        "physics_state": value,
        "position": {"raw": [value, 0, 0]},
        "position_history": {"raw": [value, 0, 0]},
        "response_velocity": {"raw": [value, 0, 0]},
        "correction": {"raw": [value, 0, 0]},
        "air_motion": {"raw": [value, 0, 0]},
        "turn": {"accumulator_raw": value},
        "orientation": {
            "row_0": {"raw": [value, 0, 0]},
            "row_1": {"raw": [0, value, 0]},
            "row_2": {"raw": [0, 0, value]},
        },
    }


def test_native_wire_preserves_initial_state_and_direct_input() -> None:
    recording = {
        "initial": _snapshot(-1),
        "frames": [
            {
                "frame": 0,
                "input": {
                    "action_mask": 0x9000,
                    "normalized_axes": {"horizontal": -41, "vertical": 40},
                },
            }
        ],
    }
    wire = _wire_input(recording["initial"], recording["frames"])

    assert "version 9" in wire
    assert "init -1 0 0" in wire
    assert "frame 0 36864 -41 40 256 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" in wire
    assert wire.endswith("end\n")


def test_native_wire_does_not_restore_outer_correction_seams() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [
            {
                "frame": 0,
                "input": {"action_mask": 0},
                "events": [
                    {
                        "type": "motion_correction_input",
                        "correction_s32": [4, 5, 6],
                    },
                    {
                        "type": "response_correction_input",
                        "operand_s32": [7, 8, 9],
                    },
                ],
            }
        ],
    }

    line = _wire_input(recording["initial"], recording["frames"]).splitlines()[2]

    assert line.split()[28:36] == ["0", "0", "0", "0", "0", "0", "0", "0"]


def test_native_wire_disables_derived_channels() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [
            {
                "frame": 0,
                "input": {"action_mask": 0, "horizontal": 0, "vertical": 0, "scale": 256},
                "events": [
                    {
                        "type": "ollie_random_input",
                        "purpose": "impulse.first",
                        "raw_roll": 20,
                    },
                    {
                        "type": "velocity_damping_random_input",
                        "purpose": "decay_threshold",
                        "raw_roll": 30,
                    },
                    {"type": "motion_correction_input", "correction_s32": [1, 2, 3]},
                    {"type": "response_correction_input", "operand_s32": [4, 5, 6]},
                ],
            }
        ],
    }

    tokens = _wire_input(recording["initial"], recording["frames"]).splitlines()[2].split()

    assert tokens[6] == "1"  # causal random_available
    assert tokens[22] == "0"  # no complete current-format component set
    assert tokens[28] == "0"  # motion_available
    assert tokens[32] == "0"  # response_available


def test_native_wire_carries_current_velocity_damping_components() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [
            {
                "frame": 0,
                "input": {"action_mask": 0},
                "events": [
                    {
                        "type": "shared_random_call",
                        "caller": "0x0049d4a1",
                        "return_value_s32": 60,
                    },
                    {
                        "type": "shared_random_call",
                        "caller": "0x0049d5a0",
                        "return_value_s32": 60,
                    },
                    {
                        "type": "velocity_damping_component_input",
                        "purpose": "decay_x",
                        "raw_value": 4,
                    },
                    {
                        "type": "velocity_damping_component_input",
                        "purpose": "decay_y",
                        "raw_value": 5896,
                    },
                    {
                        "type": "velocity_damping_component_input",
                        "purpose": "decay_z",
                        "raw_value": 4573,
                    },
                ],
            }
        ],
    }

    tokens = _wire_input(recording["initial"], recording["frames"]).splitlines()[2].split()

    assert tokens[21:28] == ["1", "1", "60", "4", "5896", "4573", "60"]


def test_native_wire_maps_inproc_shared_random_state_two_call() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [{
            "frame": 0,
            "input": {"action_mask": 0},
            "events": [{
                "type": "shared_random_call",
                "caller": "0x0049e742",
                "return_value_s32": 50,
            }],
        }],
    }

    tokens = _wire_input(recording["initial"], recording["frames"]).splitlines()[2].split()

    assert tokens[45:47] == ["1", "50"]


def test_native_wire_accepts_identical_forensic_damping_observations() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [{
            "frame": 0,
            "input": {"action_mask": 0},
            "events": [
                {
                    "type": "velocity_damping_random_input",
                    "purpose": "rescale_threshold",
                    "raw_roll": 60,
                },
                {
                    "type": "shared_random_call",
                    "caller": "0x0049d4a1",
                    "return_value_s32": 60,
                },
            ],
        }],
    }

    line = _wire_input(recording["initial"], recording["frames"]).splitlines()[2]

    assert line.split()[23] == "60"


def test_native_wire_rejects_conflicting_forensic_damping_observations() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [{
            "frame": 0,
            "input": {"action_mask": 0},
            "events": [
                {
                    "type": "velocity_damping_random_input",
                    "purpose": "rescale_threshold",
                    "raw_roll": 60,
                },
                {
                    "type": "shared_random_call",
                    "caller": "0x0049d4a1",
                    "return_value_s32": 61,
                },
            ],
        }],
    }

    with pytest.raises(ValueError, match="conflicting velocity damping"):
        _wire_input(recording["initial"], recording["frames"])


def test_native_wire_carries_air_action_control_channel() -> None:
    before = _snapshot(0)
    before["raw_physics_words"] = [0] * 11 + [1234]
    before["physics"] = {"air_control_enabled": True}
    recording = {
        "initial": _snapshot(0),
        "frames": [{
            "frame": 0,
            "input": {"action_mask": 0},
            "before": before,
            "events": [],
        }],
    }

    line = _wire_input(recording["initial"], recording["frames"]).splitlines()[2]

    assert line.split()[36:39] == ["1", "1234", "1"]


def test_native_output_parser_reads_signed_snapshot_fields() -> None:
    values = [1] * 28
    output = "native-replay-v1\nframe 0 " + " ".join(map(str, values)) + "\n"

    frames = _parse_native_output(output)

    assert frames == [
        (
            0,
            {
                "position": [1, 1, 1],
                "previous_position": [1, 1, 1],
                "collision_response": [1, 1, 1],
                "motion_correction": [1, 1, 1],
                "air_motion": [1, 1, 1],
                "physics_state": 1,
                "ground_update_state": 1,
                "ground_physics_mode": 1,
                "turn_accumulator": 1,
                "orientation": [1] * 9,
            },
        )
    ]
