from __future__ import annotations

from pathlib import Path
from types import SimpleNamespace

from tony.cli import build_parser
from tony.scenarios import (
    ScenarioError,
    load_scenario,
    scenario_capture,
    scenario_capture_commands,
    scenario_files,
    validate_scenario,
)


def test_initial_scenario_manifests_are_valid_and_intent_only():
    manifests = scenario_files()
    assert [path.stem for path in manifests] == [
        "warehouse-idle",
        "warehouse-ollie-land",
        "warehouse-straight",
        "warehouse-turn-ollie",
        "warehouse-turn",
    ]
    for path in manifests:
        scenario = load_scenario(path)
        assert scenario["frames"] == 256
        assert not any(str(value).endswith(".otrec") for value in scenario["input"])
        assert scenario["expect"]["retail"] == "match"


def test_idle_manifest_declares_only_qualified_causal_forensics():
    scenario = load_scenario("warehouse-idle")

    assert scenario["forensics"] == ["service", "rng"]
    assert "all" not in scenario["forensics"]


def test_scenario_parser_exposes_all_phase_one_commands():
    for command in ("list", "capture", "retail", "native", "verify"):
        operands = [] if command == "list" else ["warehouse-idle"]
        args = build_parser().parse_args(["scenario", command, *operands])
        assert args.scenario_command == command
        assert callable(args.func)


def test_scenario_capture_commands_preserve_frame_intent():
    scenario = load_scenario("warehouse-ollie-land")
    commands = scenario_capture_commands(
        scenario,
        Path("build/scenarios/warehouse-ollie-land/retail.otrec"),
        force=True,
    )

    assert commands[:3] == [
        "tony-frame-clock frame_tick",
        "tony-record-forensic all",
        "tony-record-start build/scenarios/warehouse-ollie-land/retail.otrec --force --frames 256 --quit",
    ]
    assert commands[3:] == ["tony-action-edge kick 20 8"]


def test_scenario_capture_commands_can_omit_diagnostic_forensics():
    scenario = load_scenario("warehouse-ollie-land")
    commands = scenario_capture_commands(
        scenario,
        Path("build/scenarios/warehouse-ollie-land/retail.otrec"),
        force=False,
        include_forensics=False,
    )

    assert commands == [
        "tony-frame-clock frame_tick",
        "tony-record-start build/scenarios/warehouse-ollie-land/retail.otrec --frames 256 --quit",
        "tony-action-edge kick 20 8",
    ]


def test_scenario_capture_parser_accepts_benchmark_frame_override():
    args = build_parser().parse_args(
        ["scenario", "capture", "warehouse-idle", "--frames", "1024"]
    )

    assert args.frames == 1024


def test_scenario_validation_rejects_invalid_edges_and_unknown_forensics():
    errors = validate_scenario(
        {
            "level": "warehouse",
            "frames": 4,
            "input": [{"frame": 3, "action": "jump", "state": "release"}],
            "forensics": ["made-up"],
            "expect": {"retail": "match"},
        }
    )
    assert any("releases action" in error for error in errors)
    assert any("forensics" in error for error in errors)


def test_scenario_capture_uses_batch_level_debugger(monkeypatch, tmp_path):
    captured = []

    def fake_debug(args):
        captured.append(args)
        return 0

    def fake_level_args(args, *, batch, headless_launch):
        assert batch is True
        assert headless_launch is True
        args.gdb_batch = batch
        return args

    monkeypatch.setattr("tony.debug.debug_game", fake_debug)
    monkeypatch.setattr("tony.commands._level_debug_args", fake_level_args)
    monkeypatch.setattr(
        "tony.scenarios.validate_scenario_recording",
        lambda _scenario, _path: ({"frames": 256}, []),
    )
    args = SimpleNamespace(
        name="warehouse-idle",
        output=str(tmp_path / "idle.otrec"),
        force=False,
        backend="gdb",
        session="scenario-test",
        port=31340,
        unmute=True,
    )

    assert scenario_capture(args) == 0
    assert captured[0].level == 12
    assert captured[0].gdb_batch is True
    assert captured[0].gdb_commands[-1].endswith("--frames 256 --quit")


def test_load_scenario_reports_missing_manifest():
    try:
        load_scenario("does-not-exist")
    except ScenarioError as exc:
        assert "scenario manifest not found" in str(exc)
    else:
        raise AssertionError("missing scenario did not fail")
