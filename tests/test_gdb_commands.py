from __future__ import annotations

import ast
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMANDS = ROOT / "re" / "gdb" / "opentony" / "commands"


def _gdb_command_classes() -> set[str]:
    result = set()
    for path in COMMANDS.glob("*.py"):
        tree = ast.parse(path.read_text(encoding="utf-8"))
        for node in tree.body:
            if not isinstance(node, ast.ClassDef):
                continue
            if any(
                isinstance(base, ast.Attribute)
                and isinstance(base.value, ast.Name)
                and base.value.id == "gdb"
                and base.attr == "Command"
                for base in node.bases
            ):
                result.add(node.name)
    return result


def test_gdb_command_surface_is_split_into_responsibility_modules():
    assert not (ROOT / "re" / "gdb" / "opentony" / "commands.py").exists()
    assert {
        "common.py",
        "control.py",
        "diagnostics.py",
        "knowledge.py",
        "probes.py",
        "recording.py",
        "replay.py",
        "registry.py",
        "sessions.py",
    } <= {path.name for path in COMMANDS.glob("*.py")}
    assert max(path.stat().st_size for path in COMMANDS.glob("*.py")) < 70_000


def test_every_gdb_command_is_registered():
    registry = ast.parse((COMMANDS / "registry.py").read_text(encoding="utf-8"))
    command_classes = _gdb_command_classes()
    registrations = {
        call.func.id
        for call in ast.walk(registry)
        if isinstance(call, ast.Call) and isinstance(call.func, ast.Name) and call.func.id in command_classes
    }
    assert registrations == command_classes


def test_probe_families_are_centralized():
    tree = ast.parse((COMMANDS / "probes.py").read_text(encoding="utf-8"))
    assignment = next(
        node
        for node in tree.body
        if isinstance(node, ast.Assign)
        and any(isinstance(target, ast.Name) and target.id == "PROBE_FAMILIES" for target in node.targets)
    )
    assert isinstance(assignment.value, ast.Dict)
    assert {key.value for key in assignment.value.keys if isinstance(key, ast.Constant)} == {
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
