from __future__ import annotations

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

    assert "version 2" in wire
    assert "init -1 0 0" in wire
    assert "frame 0 36864 -41 40 256 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0" in wire
    assert wire.endswith("end\n")


def test_native_wire_carries_outer_correction_seams() -> None:
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

    assert line.split()[-11:-3] == ["1", "4", "5", "6", "1", "7", "8", "9"]


def test_native_strict_wire_disables_derived_channels() -> None:
    recording = {
        "initial": _snapshot(0),
        "frames": [
            {
                "frame": 0,
                "input": {"action_mask": 0, "horizontal": 0, "vertical": 0, "scale": 256},
                "events": [
                    {
                        "type": "ollie_random_input",
                    },
                    {
                        "type": "velocity_damping_random_input",
                    },
                    {"type": "motion_correction_input", "correction_s32": [1, 2, 3]},
                    {"type": "response_correction_input", "operand_s32": [4, 5, 6]},
                ],
            }
        ],
    }

    tokens = _wire_input(
        recording["initial"], recording["frames"], mode="strict"
    ).splitlines()[2].split()

    assert tokens[6] == "0"  # random_available
    assert tokens[21] == "0"  # damping_available
    assert tokens[27] == "0"  # motion_available
    assert tokens[31] == "0"  # response_available


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

    assert line.split()[-3:] == ["1", "1234", "1"]


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
