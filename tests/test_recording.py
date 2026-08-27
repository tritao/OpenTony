from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "re/gdb"))

from opentony.recording import RecordingController, RecordingState, RecordingWriter

from tony import recording as recording_commands
from tony.recording import validate_recording


def _snapshot(value: int) -> dict:
    return {"physics": {"state_raw": value}, "position": {"raw": [value, 0, 0]}}


def test_host_start_queues_command_for_active_session(monkeypatch, tmp_path):
    session_dir = tmp_path / "session"
    session_dir.mkdir()
    session = SimpleNamespace(path=session_dir, session_id="warehouse", active=True)
    monkeypatch.setattr(recording_commands.sessions, "list_sessions", lambda: [session])
    args = SimpleNamespace(
        session=None,
        output=str(tmp_path / "warehouse.otrec"),
        force=True,
    )

    assert recording_commands.record_start(args) == 0
    commands = list((session_dir / "recording.commands").glob("*.json"))
    assert len(commands) == 1
    assert json.loads(commands[0].read_text()) == {
        "action": "start",
        "output": str(tmp_path / "warehouse.otrec"),
        "overwrite": True,
    }


def test_start_and_stop_are_committed_on_frame_boundaries(tmp_path):
    path = tmp_path / "warehouse.otrec"
    controller = RecordingController(
        writer_factory=RecordingWriter,
        clock=lambda: "2026-08-27T12:00:00.000+00:00",
    )

    recording_id = controller.request_start(path)
    assert recording_id.startswith("recording-")
    assert controller.state is RecordingState.START_PENDING
    assert not path.exists()

    controller.begin_frame(
        _snapshot(0),
        input_record={"action_mask": 0x8000, "normalized_axes": {"horizontal": -41}},
    )
    controller.event({"type": "physics_state_request", "state_requested": 1})
    controller.end_frame(_snapshot(1))
    assert controller.state is RecordingState.RECORDING
    assert controller.current_frame_index == 1

    assert controller.request_stop()
    assert controller.state is RecordingState.STOP_PENDING
    controller.begin_frame(_snapshot(1), input_record={"action_mask": 0})
    controller.end_frame(_snapshot(2))

    assert controller.state is RecordingState.IDLE
    assert controller.current_frame_index == 2
    records = [json.loads(line) for line in path.read_text().splitlines()]
    assert records[0]["type"] == "header"
    assert records[1]["type"] == "initial_state"
    assert [record["frame"] for record in records if record["type"] == "frame"] == [0, 1]
    assert records[2]["events"][0]["type"] == "physics_state_request"
    assert records[-1] == {
        "complete": True,
        "format": RecordingWriter.FORMAT,
        "frames": 2,
        "recording_id": recording_id,
        "type": "end",
    }


def test_hotkey_is_rising_edge_only_and_does_not_enter_input_record(tmp_path):
    controller = RecordingController()
    controller.DEFAULT_DIRECTORY = tmp_path

    assert controller.on_input({"action_mask": 0}, hotkey_down=True)
    assert controller.state is RecordingState.START_PENDING
    assert not controller.on_input({"action_mask": 0}, hotkey_down=True)
    controller.begin_frame(_snapshot(0), input_record=controller.latest_input)
    controller.end_frame(_snapshot(0))
    controller.on_input({"action_mask": 0}, hotkey_down=False)
    assert controller.on_input({"action_mask": 0}, hotkey_down=True)
    assert controller.state is RecordingState.STOP_PENDING
    controller.begin_frame(_snapshot(0), input_record=controller.latest_input)
    controller.end_frame(_snapshot(0))

    assert controller.current_frame_index == 2


def test_validator_requires_contiguous_complete_frames(tmp_path):
    path = tmp_path / "valid.otrec"
    controller = RecordingController()
    controller.request_start(
        path,
        metadata={
            "retail_executable_sha256": "test",
            "recording_timestamp": "2026-08-27T12:00:00.000+00:00",
            "level": {"index": 12, "name": "warehouse"},
            "player_identity": {"slot": 0},
            "instrumentation_version": "test",
        },
    )
    controller.begin_frame(_snapshot(0), input_record={"action_mask": 0})
    controller.end_frame(_snapshot(1))
    controller.request_stop()
    controller.begin_frame(_snapshot(1), input_record={"action_mask": 0})
    controller.end_frame(_snapshot(2))

    summary, errors = validate_recording(path)
    assert summary["valid"] is True
    assert summary["frames"] == 2
    assert errors == []

    records = [json.loads(line) for line in path.read_text().splitlines()]
    records[-2]["frame"] = 4
    path.write_text("\n".join(json.dumps(record) for record in records) + "\n")
    summary, errors = validate_recording(path)
    assert summary["valid"] is False
    assert any("contiguous" in error["error"] for error in errors)


def test_non_finite_probe_values_are_explicit_and_do_not_break_frame_close(tmp_path):
    path = tmp_path / "non-finite.otrec"
    controller = RecordingController(
        writer_factory=RecordingWriter,
        clock=lambda: "2026-08-27T12:00:00.000+00:00",
    )
    controller.request_start(path)
    controller.begin_frame(_snapshot(0), input_record={"action_mask": 0})
    controller.event(
        {
            "type": "position_commit",
            "word": {"raw": "0000c07f", "f32": math.nan},
        }
    )
    controller.end_frame(_snapshot(1))
    controller.request_stop()
    controller.begin_frame(_snapshot(1), input_record={"action_mask": 0})
    controller.end_frame(_snapshot(2))

    records = [json.loads(line) for line in path.read_text().splitlines()]
    event = records[2]["events"][0]
    assert event["word"] == {
        "f32": {"non_finite_float": "nan"},
        "raw": "0000c07f",
    }
    assert records[-1]["complete"] is True
    assert controller.state is RecordingState.IDLE
