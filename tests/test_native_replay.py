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

    assert "version 1" in wire
    assert "init -1 0 0" in wire
    assert "frame 0 36864 -41 40" in wire
    assert wire.endswith("end\n")


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
