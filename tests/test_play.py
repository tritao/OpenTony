from types import SimpleNamespace
from pathlib import Path

from tony import commands
from tony.cli import parse_args
from tony import wine


def test_play_mounts_before_running(monkeypatch):
    events = []
    args = SimpleNamespace(game_args=["-fullscreen"])

    monkeypatch.setattr(commands, "_recorded_exe", lambda: events.append("identity"))
    monkeypatch.setattr(commands, "wine_mount_disc", lambda value: events.append(("mount", value)))
    monkeypatch.setattr(commands, "run_game", lambda value: events.append(("run", value)) or 23)

    assert commands.play_game(args) == 23
    assert events == ["identity", ("mount", args), ("run", args)]


def test_play_forwards_option_arguments():
    args = parse_args(["play", "--fullscreen", "--level", "warehouse"])
    assert args.game_args == ["--fullscreen", "--level", "warehouse"]


def test_play_strips_argument_separator():
    args = parse_args(["play", "--", "--fullscreen"])
    assert args.game_args == ["--fullscreen"]


def test_run_uses_generated_nocd_executable(monkeypatch, tmp_path: Path):
    executable = tmp_path / "THawk2.nocd.exe"
    calls = []

    monkeypatch.setattr(wine, "nocd_executable", lambda: executable)
    monkeypatch.setattr(wine, "wine_env", lambda: {"WINEPREFIX": str(tmp_path / "prefix")})
    monkeypatch.setattr(
        wine.subprocess,
        "run",
        lambda command, **kwargs: calls.append((command, kwargs)) or SimpleNamespace(returncode=7),
    )

    result = wine.run_game(SimpleNamespace(game_args=["--fullscreen"]))

    assert result == 7
    assert calls[0][0] == ["wine", str(executable), "--fullscreen"]
    assert calls[0][1]["cwd"] == tmp_path
