"""Scenario manifests and orchestration helpers.

Scenario files are deliberately small intent descriptions.  Recordings and
replay traces are generated under ``build/`` and are never part of the
manifest itself.
"""

from __future__ import annotations

import argparse
import json
import shlex
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import yaml

from .common import ROOT, resolve
from .levels import parse_level
from .recording import load_recording, validate_recording

SCENARIO_ROOT = ROOT / "re/scenarios"
SCENARIO_BUILD_ROOT = ROOT / "build/scenarios"

ACTION_MASKS = {
    "jump": 0x0010,
    "grind": 0x0080,
    "grab": 0x0020,
    "kick": 0x0040,
    "spinleft": 0x0004,
    "nollie": 0x0001,
    "spinright": 0x0008,
    "switch": 0x0002,
    "left": 0x8000,
    "right": 0x2000,
    "up": 0x1000,
    "down": 0x4000,
}
FORENSIC_FAMILIES = {
    "all",
    "core",
    "collision",
    "service",
    "recovery",
    "rng",
    "animation",
    "correction",
    "state",
    "position",
    "timing",
}
EXPECTED_RESULTS = {"match", "diverge"}


class ScenarioError(ValueError):
    """Raised when a scenario manifest is missing or invalid."""


def scenario_files(root: Path = SCENARIO_ROOT) -> list[Path]:
    """Return scenario manifests in stable filename order."""

    return sorted((*root.glob("*.yml"), *root.glob("*.yaml")))


def _scenario_path(name_or_path: str | Path, root: Path = SCENARIO_ROOT) -> Path:
    value = Path(name_or_path)
    if value.is_absolute():
        return resolve(value)
    if value.suffix in {".yml", ".yaml"}:
        return root / value
    if value.parent != Path("."):
        return resolve(value)
    for suffix in (".yml", ".yaml"):
        candidate = root / f"{value}{suffix}"
        if candidate.is_file():
            return candidate
    return root / f"{value}.yml"


def _context(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def _event_context(index: int, event: Any) -> str:
    frame = event.get("frame") if isinstance(event, dict) else None
    return f"input[{index}]" + (f" (frame {frame})" if frame is not None else "")


def validate_scenario(document: Any, *, path: str | Path = "scenario") -> list[str]:
    """Return schema errors for one intent manifest.

    ``version`` is optional for the initial manifests so the compact form in
    the workflow proposal remains valid.  When present it is pinned to one.
    """

    context = str(path)
    errors: list[str] = []
    if not isinstance(document, dict):
        return [f"{context}: manifest must be a mapping"]
    if "version" in document and document.get("version") != 1:
        errors.append(f"{context}: version must be 1")

    level = document.get("level")
    if not isinstance(level, str) or not level.strip():
        errors.append(f"{context}: level must be a nonempty string")
    else:
        try:
            parse_level(level)
        except (argparse.ArgumentTypeError, SystemExit) as exc:
            errors.append(f"{context}: invalid level {level!r}: {exc}")

    frames = document.get("frames")
    if isinstance(frames, bool) or not isinstance(frames, int) or frames <= 0:
        errors.append(f"{context}: frames must be a positive integer")

    events = document.get("input", [])
    if not isinstance(events, list):
        errors.append(f"{context}: input must be a list")
        events = []
    active: dict[str, int] = {}
    previous_frame = -1
    for index, event in enumerate(events):
        event_context = _event_context(index, event)
        if not isinstance(event, dict):
            errors.append(f"{context}: {event_context} must be a mapping")
            continue
        frame = event.get("frame")
        if isinstance(frame, bool) or not isinstance(frame, int):
            errors.append(f"{context}: {event_context}.frame must be an integer")
        elif isinstance(frames, int) and not isinstance(frames, bool) and not 0 <= frame < frames:
            errors.append(f"{context}: {event_context}.frame must be between 0 and {frames - 1}")
        if isinstance(frame, int) and not isinstance(frame, bool) and frame < previous_frame:
            errors.append(f"{context}: input events must be ordered by frame")
        if isinstance(frame, int) and not isinstance(frame, bool):
            previous_frame = max(previous_frame, frame)

        action = event.get("action")
        normalized_action = action.casefold() if isinstance(action, str) else None
        if normalized_action not in ACTION_MASKS:
            errors.append(
                f"{context}: {event_context}.action must be one of " + ", ".join(sorted(ACTION_MASKS))
            )
        state = event.get("state")
        if state not in {"press", "release"}:
            errors.append(f"{context}: {event_context}.state must be 'press' or 'release'")
        if normalized_action in ACTION_MASKS and state in {"press", "release"}:
            if state == "press":
                if normalized_action in active:
                    errors.append(
                        f"{context}: {event_context} presses already-held action {normalized_action!r}"
                    )
                elif isinstance(frame, int) and not isinstance(frame, bool):
                    active[normalized_action] = frame
            elif normalized_action not in active:
                errors.append(
                    f"{context}: {event_context} releases action {normalized_action!r} before press"
                )
            else:
                del active[normalized_action]

    forensics = document.get("forensics", ["core"])
    if not isinstance(forensics, list):
        errors.append(f"{context}: forensics must be a list")
        forensics = []
    for index, family in enumerate(forensics):
        if not isinstance(family, str) or family.casefold() not in FORENSIC_FAMILIES:
            errors.append(
                f"{context}: forensics[{index}] must be one of " + ", ".join(sorted(FORENSIC_FAMILIES))
            )

    expect = document.get("expect")
    if not isinstance(expect, dict):
        errors.append(f"{context}: expect must be a mapping")
    else:
        for target, expected in expect.items():
            if target not in {"retail", "native"}:
                errors.append(f"{context}: expect has unknown target {target!r}")
                continue
            status = expected.get("status") if isinstance(expected, dict) else expected
            if status not in EXPECTED_RESULTS:
                errors.append(f"{context}: expect.{target} must be 'match' or 'diverge'")
            if isinstance(expected, dict) and "frame" in expected:
                divergence_frame = expected["frame"]
                if (
                    isinstance(divergence_frame, bool)
                    or not isinstance(divergence_frame, int)
                    or divergence_frame < 0
                    or (
                        isinstance(frames, int)
                        and not isinstance(frames, bool)
                        and divergence_frame >= frames
                    )
                ):
                    errors.append(f"{context}: expect.{target}.frame must be a frame in the scenario")
    return errors


def load_scenario(name_or_path: str | Path) -> dict[str, Any]:
    """Load and validate a named scenario manifest."""

    path = _scenario_path(name_or_path)
    if not path.is_file():
        raise ScenarioError(f"scenario manifest not found: {_context(path)}")
    try:
        document = yaml.safe_load(path.read_text(encoding="utf-8"))
    except (OSError, yaml.YAMLError) as exc:
        raise ScenarioError(f"could not read scenario manifest {_context(path)}: {exc}") from exc
    errors = validate_scenario(document, path=_context(path))
    if errors:
        raise ScenarioError("\n".join(errors))
    result = dict(document)
    result["_id"] = path.stem
    result["_path"] = path
    result["level"] = result["level"].strip().casefold()
    result["forensics"] = [family.casefold() for family in result.get("forensics", ["core"])]
    result["input"] = [{**event, "action": event["action"].casefold()} for event in result.get("input", [])]
    return result


def scenario_recording_path(scenario: str | dict[str, Any]) -> Path:
    """Return the generated retail recording path for a scenario."""

    scenario_id = scenario if isinstance(scenario, str) else scenario["_id"]
    return SCENARIO_BUILD_ROOT / str(scenario_id) / "retail.otrec"


def scenario_native_trace_path(scenario: str | dict[str, Any]) -> Path:
    scenario_id = scenario if isinstance(scenario, str) else scenario["_id"]
    return SCENARIO_BUILD_ROOT / str(scenario_id) / "native.jsonl"


def _input_intervals(events: list[dict[str, Any]], frame_count: int) -> list[tuple[str, int, int]]:
    """Convert press/release intent into existing bounded action-edge probes."""

    release_frames: dict[str, int] = {}
    for event in events:
        if event["state"] == "release":
            release_frames[event["action"]] = event["frame"]
    intervals = []
    for event in events:
        if event["state"] != "press":
            continue
        start = event["frame"]
        end = release_frames.get(event["action"], frame_count)
        hold = max(1, end - start)
        intervals.append((event["action"], start, hold))
    return intervals


def scenario_capture_commands(scenario: dict[str, Any], output: Path, *, force: bool) -> list[str]:
    """Build the deterministic GDB command sequence used by capture."""

    commands = ["tony-frame-clock frame_tick"]
    if scenario["forensics"]:
        commands.append(
            "tony-record-forensic "
            + " ".join(shlex.quote(family) for family in scenario["forensics"])
        )
    commands.append(
        "tony-record-start "
        + shlex.quote(str(output))
        + (" --force" if force else "")
        + f" --frames {scenario['frames']} --quit"
    )
    commands.extend(
        f"tony-action-edge {shlex.quote(action)} {start} {hold}"
        for action, start, hold in _input_intervals(scenario["input"], scenario["frames"])
    )
    return commands


def expected_result(scenario: dict[str, Any], target: str) -> tuple[str | None, int | None]:
    value = scenario.get("expect", {}).get(target)
    if isinstance(value, dict):
        return value.get("status"), value.get("frame")
    return value, None


def validate_scenario_recording(scenario: dict[str, Any], path: Path) -> tuple[dict, list[dict]]:
    """Validate both the recording format and its manifest identity/length."""

    summary, errors = validate_recording(path)
    errors = list(errors)
    if not errors:
        try:
            header = load_recording(path).header
        except (OSError, IndexError, ValueError) as exc:
            errors.append({"line": 1, "error": f"could not read recording header: {exc}"})
        else:
            level = header.get("level")
            level_name = level.get("name") if isinstance(level, dict) else None
            if level_name != scenario["level"]:
                errors.append(
                    {
                        "line": 1,
                        "error": f"recording level is {level_name!r}; expected {scenario['level']!r}",
                    }
                )
            if summary.get("frames") != scenario["frames"]:
                errors.append(
                    {
                        "line": 0,
                        "error": f"recording has {summary.get('frames')} frames; expected {scenario['frames']}",
                    }
                )
    summary["scenario"] = scenario["_id"]
    summary["valid"] = not errors
    return summary, errors


def scenario_list(_args=None) -> int:
    manifests = scenario_files()
    if not manifests:
        print("No scenario manifests.")
        return 0
    print(f"{'Scenario':<28} {'Level':<12} {'Frames':<7} Expectations")
    for path in manifests:
        try:
            scenario = load_scenario(path)
        except ScenarioError as exc:
            print(f"{path.stem:<28} INVALID      -       {exc}")
            continue
        expectations = scenario.get("expect", {})
        display = (
            ", ".join(
                f"{target}={value.get('status') if isinstance(value, dict) else value}"
                for target, value in expectations.items()
            )
            or "-"
        )
        print(f"{scenario['_id']:<28} {scenario['level']:<12} {scenario['frames']:<7} {display}")
    return 0


def scenario_capture(args) -> int:
    scenario = _load_cli_scenario(args.name)
    output = resolve(args.output) if getattr(args, "output", None) else scenario_recording_path(scenario)
    output.parent.mkdir(parents=True, exist_ok=True)
    backend = getattr(args, "backend", "gdb")
    if backend == "inproc":
        from .capture import run_inproc_capture

        print(f"capturing scenario {scenario['_id']} with in-process recorder -> {output}")
        code = run_inproc_capture(
            scenario,
            output,
            force=bool(getattr(args, "force", False)),
            host=getattr(args, "capture_host", None),
            dll=getattr(args, "capture_dll", None),
            wine_prefix=getattr(args, "headless_prefix", None),
        )
        if code or not _validated_scenario_recording(scenario, output):
            return code or 1
        return 0
    if backend == "hybrid":
        raise SystemExit("hybrid capture is reserved for same-run shadow qualification (M3)")
    from .commands import _level_debug_args
    from .debug import debug_game as launch_debug

    debug_args = SimpleNamespace(
        level=parse_level(scenario["level"]),
        headless=True,
        headless_launch=True,
        unmute=bool(getattr(args, "unmute", False)),
        screenshot=None,
        record=None,
        session=getattr(args, "session", None),
        port=getattr(args, "port", None),
        pid=None,
        game_args=[],
        gdb_commands=scenario_capture_commands(
            scenario,
            output,
            force=bool(getattr(args, "force", False)),
        ),
    )
    print(f"capturing scenario {scenario['_id']} -> {output}")
    code = launch_debug(_level_debug_args(debug_args, batch=True, headless_launch=True))
    if code:
        return code
    if not _validated_scenario_recording(scenario, output):
        return 1
    return 0


def _scenario_recording(args, scenario: dict[str, Any]) -> Path:
    return resolve(args.recording) if getattr(args, "recording", None) else scenario_recording_path(scenario)


def _print_recording_errors(summary: dict, errors: list[dict]) -> None:
    print(json.dumps({"summary": summary, "errors": errors}, indent=2, sort_keys=True))


def _validated_scenario_recording(scenario: dict[str, Any], path: Path) -> bool:
    summary, errors = validate_scenario_recording(scenario, path)
    if errors:
        _print_recording_errors(summary, errors)
        return False
    print(f"recording: {path} ({summary['frames']} frames, valid)")
    return True


def _load_cli_scenario(name: str) -> dict[str, Any]:
    try:
        return load_scenario(name)
    except ScenarioError as exc:
        raise SystemExit(str(exc)) from exc


def _retail_args(args, scenario: dict[str, Any], path: Path) -> SimpleNamespace:
    return SimpleNamespace(
        path=str(path),
        level=parse_level(scenario["level"]),
        session=getattr(args, "session", None),
        port=getattr(args, "port", None),
        unmute=bool(getattr(args, "unmute", False)),
    )


def _native_args(args, scenario: dict[str, Any], path: Path) -> SimpleNamespace:
    return SimpleNamespace(
        path=str(path),
        trg=getattr(args, "trg", None),
        psx=getattr(args, "psx", None),
        asset_root=getattr(args, "asset_root", None),
        native_binary=getattr(args, "native_binary", None),
        output=getattr(args, "output", None) or str(scenario_native_trace_path(scenario)),
    )


def scenario_retail(args) -> int:
    """Run a scenario recording through strict retail replay."""

    scenario = _load_cli_scenario(args.name)
    path = _scenario_recording(args, scenario)
    if not _validated_scenario_recording(scenario, path):
        return 1
    from .commands import replay_retail

    return replay_retail(_retail_args(args, scenario, path))


def scenario_native(args) -> int:
    """Run a scenario recording through strict native replay."""

    scenario = _load_cli_scenario(args.name)
    path = _scenario_recording(args, scenario)
    if not _validated_scenario_recording(scenario, path):
        return 1
    from .commands import replay_native

    return replay_native(_native_args(args, scenario, path))


def _run_scenario_target(target, replay_args) -> tuple[str, int, bool]:
    """Return ``(display result, code, infrastructure_error)``."""

    try:
        code = int(target(replay_args) or 0)
    except SystemExit as exc:
        print(f"{replay_args.path}: {exc}")
        return "ERROR", 1, True
    result = "MATCH" if code == 0 else "DIVERGE"
    return result, code, False


def scenario_verify(args) -> int:
    """Validate and run both strict replay targets for one scenario."""

    scenario = _load_cli_scenario(args.name)
    path = _scenario_recording(args, scenario)
    if not _validated_scenario_recording(scenario, path):
        return 1

    from .commands import replay_native, replay_retail

    retail_result, _retail_code, retail_error = _run_scenario_target(
        replay_retail,
        _retail_args(args, scenario, path),
    )
    native_result, _native_code, native_error = _run_scenario_target(
        replay_native,
        _native_args(args, scenario, path),
    )
    print()
    print(f"{'Scenario':<28} {'Retail':<10} {'Native':<10}")
    print("-" * 51)
    print(f"{scenario['_id']:<28} {retail_result:<10} {native_result:<10}")

    failed = retail_error or native_error
    for target, result in (("retail", retail_result), ("native", native_result)):
        expected, expected_frame = expected_result(scenario, target)
        if expected is None:
            continue
        if result.casefold() != expected.casefold():
            failed = True
            suffix = f" at frame {expected_frame}" if expected_frame is not None else ""
            print(f"FAIL {target}: expected {expected.upper()}{suffix}, got {result}")
    return 1 if failed else 0
