from pathlib import Path
from types import SimpleNamespace

from tony import commands, common, wine
from tony.cli import parse_args


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
    assert calls[0][0] == ["wine", "explorer", "/desktop=OpenTony,640x480", str(executable), "--fullscreen"]
    assert calls[0][1]["cwd"] == tmp_path


def test_run_headless_wraps_the_configured_display(monkeypatch, tmp_path: Path):
    executable = tmp_path / "THawk2.nocd.exe"
    calls = []

    class FakeProcess:
        def wait(self):
            return 0

    class FakeDisplay:
        def __init__(self, cfg, env):
            calls.append(("display", cfg, env))

        def popen(self, command, **kwargs):
            calls.append(("popen", command, kwargs))
            return FakeProcess()

        def stop_recording(self):
            calls.append("stop-recording")

        def close(self):
            calls.append("close")

    monkeypatch.setattr(wine, "nocd_executable", lambda: executable)
    monkeypatch.setattr(wine, "wine_env", lambda: {"WINEPREFIX": str(tmp_path / "prefix")})
    monkeypatch.setattr(wine, "headless_wine_env", lambda: {"WINEPREFIX": str(tmp_path / "headless-prefix")})
    monkeypatch.setattr(wine, "headless_wine_command", lambda command: ["headless-wrapper", *command])
    monkeypatch.setattr(wine, "HeadlessDisplay", FakeDisplay)
    monkeypatch.setattr(wine, "configure_visual_capture", lambda display, args: calls.append("capture"))

    result = wine.run_game(SimpleNamespace(game_args=["--fullscreen"], headless=True))

    assert result == 0
    assert calls[1][0] == "popen"
    assert calls[1][1] == [
        "headless-wrapper",
        "wine",
        "explorer",
        "/desktop=OpenTony,640x480",
        str(executable),
        "--fullscreen",
    ]
    assert calls[-2:] == ["stop-recording", "close"]


def test_headless_wine_command_initializes_empty_prefixes():
    command = common.headless_wine_command(["winedbg", "--gdb"])

    assert "wineboot -i" in command[2]
    assert "wineboot -u" not in command[2]
