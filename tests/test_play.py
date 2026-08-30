from pathlib import Path
from types import SimpleNamespace

import pytest

from tony import audio, commands, common, wine
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
    args = parse_args(["play", "--fullscreen", "--quality", "high"])
    assert args.game_args == ["--fullscreen", "--quality", "high"]


def test_play_level_uses_frontend_debugger_path(monkeypatch):
    events = []
    args = SimpleNamespace(level=12, game_args=["-fullscreen"], headless=False)

    monkeypatch.setattr(commands, "_recorded_exe", lambda: events.append("identity"))
    monkeypatch.setattr(commands, "wine_mount_disc", lambda value: events.append(("mount", value)))
    monkeypatch.setattr(
        commands,
        "_debug_game",
        lambda value: events.append(("debug", value)) or 17,
    )

    assert commands.play_game(args) == 17
    debug_args = events[-1][1]
    assert debug_args.level is None
    assert debug_args.pid is None
    assert debug_args.game_args == ["-fullscreen"]
    assert debug_args.headless_launch is False
    assert debug_args.virtual_desktop is True
    assert debug_args.gdb_commands == [
        "tony-skip-movies",
        "tony-frontend-play 1 12",
        "tony-frontend-confirm",
        "continue",
    ]
    assert debug_args.gdb_batch is True


def test_debug_level_uses_frontend_path_without_batching(monkeypatch):
    events = []
    args = SimpleNamespace(level=12, pid=None, game_args=[], gdb_commands=[])

    monkeypatch.setattr(
        commands,
        "_debug_game",
        lambda value: events.append(value) or 0,
    )

    assert commands.debug_game(args) == 0
    debug_args = events[0]
    assert debug_args.gdb_commands == [
        "tony-skip-movies",
        "tony-frontend-play 1 12",
        "tony-frontend-confirm",
    ]
    assert debug_args.gdb_batch is False


def test_play_level_honors_headless(monkeypatch):
    args = SimpleNamespace(level=12, game_args=[], headless=True)
    captured = []

    monkeypatch.setattr(commands, "_recorded_exe", lambda: None)
    monkeypatch.setattr(commands, "wine_mount_disc", lambda value: 0)
    monkeypatch.setattr(commands, "_debug_game", lambda value: captured.append(value) or 0)

    assert commands.play_game(args) == 0
    assert captured[0].headless_launch is True
    assert captured[0].virtual_desktop is False


def test_visible_level_play_requires_headless_for_capture():
    args = SimpleNamespace(level=12, game_args=[], headless=False, screenshot="frame.png")

    with pytest.raises(SystemExit, match="visual capture requires --headless"):
        commands._level_debug_args(args, batch=True, headless_launch=False)


def test_debug_level_cannot_attach_to_existing_process():
    args = SimpleNamespace(level=12, pid="1234", game_args=[])

    with pytest.raises(SystemExit, match="requires a debugger-launched game"):
        commands.debug_game(args)


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
    assert calls[0][0] == ["wine", "explorer", "/desktop=OpenTony,1024x768", str(executable), "--fullscreen"]
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
    route = audio.MutedAudio("/usr/bin/pactl", "271", "opentony_debug_run", None)

    def start_muted(env, session_id):
        env["PULSE_SINK"] = route.sink_name
        return audio.AudioStart(route)

    monkeypatch.setattr(wine, "start_muted_audio", start_muted)
    monkeypatch.setattr(
        wine,
        "cleanup_muted_audio",
        lambda data: calls.append(("cleanup-audio", data)) or audio.AudioCleanup(True, "removed"),
    )

    result = wine.run_game(SimpleNamespace(game_args=["--fullscreen"], headless=True))

    assert result == 0
    assert calls[1][0] == "popen"
    assert calls[1][1] == [
        "headless-wrapper",
        "wine",
        "explorer",
        "/desktop=OpenTony,1024x768",
        str(executable),
        "--fullscreen",
    ]
    assert calls[-3:] == [
        "stop-recording",
        "close",
        (
            "cleanup-audio",
            {
                "audio_pactl": "/usr/bin/pactl",
                "audio_module_id": "271",
                "audio_sink": "opentony_debug_run",
                "audio_pulse_server": None,
            },
        ),
    ]
    assert calls[0][2]["PULSE_SINK"] == "opentony_debug_run"


def test_headless_wine_command_initializes_empty_prefixes():
    command = common.headless_wine_command(["winedbg", "--gdb"])

    assert 'if [ ! -f "$WINEPREFIX/system.reg" ]; then' in command[2]
    assert "wineboot -i" in command[2]
    assert "wineboot -u" not in command[2]
