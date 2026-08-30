import json
from pathlib import Path
from types import SimpleNamespace

import pytest

from tony import audio, debug, display


def _winedbg_result(output, returncode=0):
    return SimpleNamespace(returncode=returncode, stdout=output)


def test_find_game_pid(monkeypatch):
    output = """ pid      threads  executable (all id:s are in hex)
 00000040 10       'services.exe'
 00000198 4        'THawk2.nocd.exe'
"""
    monkeypatch.setattr(debug.subprocess, "run", lambda *args, **kwargs: _winedbg_result(output))

    assert debug._find_game_pid({}) == 0x198


def test_find_game_pid_requires_one_game(monkeypatch):
    monkeypatch.setattr(debug.subprocess, "run", lambda *args, **kwargs: _winedbg_result(""))
    with pytest.raises(SystemExit, match="no running THawk2.nocd.exe"):
        debug._find_game_pid({})

    output = """ 00000198 1        'THawk2.nocd.exe'
 00000210 1        'THawk2.nocd.exe'
"""
    monkeypatch.setattr(debug.subprocess, "run", lambda *args, **kwargs: _winedbg_result(output))
    with pytest.raises(SystemExit, match="multiple THawk2.nocd.exe"):
        debug._find_game_pid({})


def test_muted_audio_uses_temporary_pulse_sink(monkeypatch):
    calls = []
    monkeypatch.setattr(audio.shutil, "which", lambda name: "/usr/bin/pactl" if name == "pactl" else None)

    def run(command, **kwargs):
        calls.append((command, kwargs))
        return _winedbg_result("271\n")

    monkeypatch.setattr(audio.subprocess, "run", run)
    env = {"PULSE_SERVER": "unix:/tmp/pulse"}

    start = audio.start_muted_audio(env, "warehouse-1")

    assert start.route is not None
    assert start.route.pactl == "/usr/bin/pactl"
    assert start.route.module_id == "271"
    assert start.route.sink_name == "opentony_debug_warehouse_1"
    assert env["PULSE_SINK"] == "opentony_debug_warehouse_1"
    assert calls[0][0] == [
        "/usr/bin/pactl",
        "load-module",
        "module-null-sink",
        "sink_name=opentony_debug_warehouse_1",
        "sink_properties=device.description=opentony_debug_warehouse_1",
    ]



def test_cleanup_muted_audio_verifies_module_identity(monkeypatch):
    calls = []
    monkeypatch.setattr(audio.shutil, "which", lambda name: "/usr/bin/pactl")

    def run(command, **kwargs):
        calls.append((command, kwargs))
        if command[1:4] == ["list", "short", "modules"]:
            return _winedbg_result("271\tmodule-null-sink\tsink_name=opentony_debug_warehouse_1\n")
        return _winedbg_result("")

    monkeypatch.setattr(audio.subprocess, "run", run)
    result = audio.cleanup_muted_audio(
        {
            "audio_pactl": "/usr/bin/pactl",
            "audio_module_id": "271",
            "audio_sink": "opentony_debug_warehouse_1",
            "audio_pulse_server": "unix:/tmp/pulse",
        }
    )

    assert result.ok is True
    assert result.status == "removed"
    assert calls[1][0] == ["/usr/bin/pactl", "unload-module", "271"]


def test_cleanup_muted_audio_rejects_reused_module_id(monkeypatch):
    calls = []
    monkeypatch.setattr(audio.shutil, "which", lambda name: "/usr/bin/pactl")

    def run(command, **kwargs):
        calls.append(command)
        return _winedbg_result("271\tmodule-null-sink\tsink_name=some_other_session\n")

    monkeypatch.setattr(audio.subprocess, "run", run)
    result = audio.cleanup_muted_audio(
        {
            "audio_pactl": "/usr/bin/pactl",
            "audio_module_id": "271",
            "audio_sink": "opentony_debug_warehouse_1",
        }
    )

    assert result.ok is False
    assert result.status == "audio-module-identity-mismatch"
    assert calls == [["/usr/bin/pactl", "list", "short", "modules"]]


def test_move_process_audio_to_sink_targets_only_matching_process(monkeypatch):
    calls = []
    monkeypatch.setattr(audio.shutil, "which", lambda name: "/usr/bin/pactl")

    def run(command, **kwargs):
        calls.append(command)
        if command[1:] == ["list", "sink-inputs"]:
            return _winedbg_result(
                """Sink Input #41
\tSink: 1
\tProperties:
\t\tapplication.process.id = \"1234\"
Sink Input #42
\tSink: 2
\tProperties:
\t\tapplication.process.id = \"5678\"
"""
            )
        return _winedbg_result("")

    monkeypatch.setattr(audio.subprocess, "run", run)
    route = audio.MutedAudio("/usr/bin/pactl", "271", "opentony_debug_attached", None)

    moved, error = audio.move_process_audio_to_sink(route, 1234)

    assert error is None
    assert moved == (("41", "1"),)
    assert calls == [
        ["/usr/bin/pactl", "list", "sink-inputs"],
        ["/usr/bin/pactl", "move-sink-input", "41", "opentony_debug_attached"],
    ]


def test_cleanup_muted_audio_restores_owned_stream_before_unloading_sink(monkeypatch):
    calls = []
    monkeypatch.setattr(audio.shutil, "which", lambda name: "/usr/bin/pactl")

    def run(command, **kwargs):
        calls.append(command)
        if command[1:] == ["list", "sink-inputs"]:
            return _winedbg_result(
                """Sink Input #41
\tSink: opentony_debug_attached
\tProperties:
\t\tapplication.process.id = \"1234\"
"""
            )
        if command[1:4] == ["list", "short", "modules"]:
            return _winedbg_result(
                "271\tmodule-null-sink\tsink_name=opentony_debug_attached\n"
            )
        return _winedbg_result("")

    monkeypatch.setattr(audio.subprocess, "run", run)
    result = audio.cleanup_muted_audio(
        {
            "audio_pactl": "/usr/bin/pactl",
            "audio_module_id": "271",
            "audio_sink": "opentony_debug_attached",
            "audio_moved_inputs": [{"input_id": "41", "original_sink": "1"}],
        }
    )

    assert result.ok is True
    assert calls == [
        ["/usr/bin/pactl", "list", "sink-inputs"],
        ["/usr/bin/pactl", "move-sink-input", "41", "1"],
        ["/usr/bin/pactl", "list", "short", "modules"],
        ["/usr/bin/pactl", "unload-module", "271"],
    ]


def test_find_game_linux_pid_matches_prefix_and_executable(monkeypatch, tmp_path: Path):
    prefix = tmp_path / "prefix"
    prefix.mkdir()
    proc = tmp_path / "proc"
    game = proc / "1234"
    game.mkdir(parents=True)
    (game / "environ").write_bytes(f"WINEPREFIX={prefix}\0".encode())
    (game / "cmdline").write_bytes(b"wine-preloader\0Z:\\game\\THawk2.nocd.exe\0")
    (game / "stat").write_text("1234 (wine-preloader) R 1 1 1", encoding="ascii")
    assert debug._find_game_linux_pid({"WINEPREFIX": str(prefix)}, proc) == 1234


def test_xvfb_command_uses_16_bit_software_profile(monkeypatch):
    monkeypatch.setattr(display.shutil, "which", lambda name: "/usr/bin/xvfb-run" if name == "xvfb-run" else None)
    env = {"WINEPREFIX": "/tmp/prefix"}
    cfg = {
        "virtual_desktop": {"width": 1024, "height": 768},
        "xvfb": {
            "depth": 16,
            "server_args": ["+extension", "GLX"],
            "environment": {
                "LIBGL_ALWAYS_SOFTWARE": "1",
                "MESA_LOADER_DRIVER_OVERRIDE": "llvmpipe",
            },
        },
    }

    command = debug._xvfb_command(cfg, env)

    assert command == [
        "/usr/bin/xvfb-run",
        "-a",
        "-s",
        "-screen 0 1024x768x16 +extension GLX",
    ]
    assert env["LIBGL_ALWAYS_SOFTWARE"] == "1"
    assert env["MESA_LOADER_DRIVER_OVERRIDE"] == "llvmpipe"


def test_xvfb_command_prefers_headless_display_profile(monkeypatch):
    monkeypatch.setattr(
        display.shutil,
        "which",
        lambda name: "/usr/bin/xvfb-run" if name == "xvfb-run" else None,
    )
    cfg = {
        "virtual_desktop": {"width": 1024, "height": 768},
        "headless_display": {"width": 640, "height": 480},
        "xvfb": {
            "depth": 16,
            "server_args": ["+extension", "GLX"],
            "environment": {"MESA_LOADER_DRIVER_OVERRIDE": "llvmpipe"},
        },
    }
    environment = {}

    assert display.xvfb_command(cfg, environment) == [
        "/usr/bin/xvfb-run",
        "-a",
        "-s",
        "-screen 0 640x480x16 +extension GLX",
    ]


def test_configure_virtual_desktop_sets_default_and_size(monkeypatch):
    calls = []

    def run(command, **kwargs):
        calls.append((command, kwargs))
        return _winedbg_result("The operation completed successfully")

    monkeypatch.setattr(debug.subprocess, "run", run)
    debug._configure_virtual_desktop(
        {"WINEPREFIX": "/tmp/prefix"},
        {"virtual_desktop": {"name": "OpenTony", "width": 1024, "height": 768}},
    )

    assert [call[0] for call in calls] == [
        [
            "wine",
            "reg",
            "add",
            r"HKCU\Software\Wine\Explorer",
            "/v",
            "Desktop",
            "/d",
            "OpenTony",
            "/f",
        ],
        [
            "wine",
            "reg",
            "add",
            r"HKCU\Software\Wine\Explorer\Desktops",
            "/v",
            "OpenTony",
            "/d",
            "1024x768",
            "/f",
        ],
    ]


def test_xvfb_command_rejects_invalid_depth():
    with pytest.raises(SystemExit, match="invalid Xvfb screen depth"):
        debug._xvfb_command({"xvfb": {"depth": 15}}, {})


def test_headless_display_publishes_connection_details(monkeypatch, tmp_path: Path):
    cfg = {
        "virtual_desktop": {"width": 1024, "height": 768},
        "xvfb": {"depth": 16, "server_args": ["+extension", "GLX"], "environment": {"RENDER": "software"}},
    }
    calls = []

    class FakeProcess:
        returncode = None

        def __init__(self, command, **kwargs):
            calls.append((command, kwargs))
            metadata = Path(kwargs["env"]["TONY_DISPLAY_INFO"])
            metadata.write_text(":77\n/tmp/xauthority\n", encoding="utf-8")

        def poll(self):
            return None

    monkeypatch.setattr(display.shutil, "which", lambda name: "/usr/bin/xvfb-run" if name == "xvfb-run" else None)
    monkeypatch.setattr(display.subprocess, "Popen", FakeProcess)

    session = display.HeadlessDisplay(cfg, {"WINEPREFIX": str(tmp_path / "prefix")})
    process = session.popen(["wine", "game.exe"], cwd=tmp_path)

    assert process.returncode is None
    assert session.info is not None
    assert session.environment["DISPLAY"] == ":77"
    assert session.environment["XAUTHORITY"] == "/tmp/xauthority"
    assert calls[0][0][:4] == ["/usr/bin/xvfb-run", "-a", "-s", "-screen 0 1024x768x16 +extension GLX"]
    assert calls[0][1]["env"]["RENDER"] == "software"
    session.close()


def test_recover_incomplete_trace_appends_explicit_footer(tmp_path):
    session_dir = tmp_path / "session"
    session_dir.mkdir()
    trace_path = tmp_path / "trace.jsonl"
    trace_path.write_text(
        "\n".join(
            json.dumps(record)
            for record in (
                {"type": "header"},
                {"type": "watchpoint", "frame": 17},
            )
        )
        + "\n",
        encoding="utf-8",
    )
    (session_dir / "trace.active").write_text(
        json.dumps({"path": str(trace_path), "experiment": "test"}) + "\n",
        encoding="utf-8",
    )
    session = SimpleNamespace(path=session_dir)

    assert debug._recover_incomplete_trace(session, "gdb-proxy-disconnected:1") is True
    records = [json.loads(line) for line in trace_path.read_text(encoding="utf-8").splitlines()]
    assert records[-1] == {
        "complete": False,
        "frames": 17,
        "reason": "gdb-proxy-disconnected:1",
        "type": "end",
    }
    assert not (session_dir / "trace.active").exists()


def test_recover_incomplete_recording_counts_completed_frames(tmp_path):
    session_dir = tmp_path / "session"
    session_dir.mkdir()
    recording_path = tmp_path / "recording.otrec"
    recording_path.write_text(
        "\n".join(
            json.dumps(record)
            for record in (
                {
                    "type": "header",
                    "format": "opentony-retail-recording-v1",
                    "recording_id": "recording-test",
                },
                {"type": "initial_state", "frame": 0},
                {"type": "frame", "frame": 0},
            )
        )
        + "\n",
        encoding="utf-8",
    )
    (session_dir / "recording.active").write_text(
        json.dumps(
            {
                "path": str(recording_path),
                "recording_id": "recording-test",
                "format": "opentony-retail-recording-v1",
            }
        )
        + "\n",
        encoding="utf-8",
    )
    session = SimpleNamespace(path=session_dir)

    assert debug._recover_incomplete_recording(session, "gdb-exited:1") is True
    records = [json.loads(line) for line in recording_path.read_text().splitlines()]
    assert records[-1] == {
        "complete": False,
        "format": "opentony-retail-recording-v1",
        "frames": 1,
        "reason": "gdb-exited:1",
        "recording_id": "recording-test",
        "type": "end",
    }
    assert not (session_dir / "recording.active").exists()


def test_parent_death_signal_is_best_effort():
    # The helper is used as a subprocess pre-exec hook; it must not make
    # debugger startup fail on platforms without Linux prctl.
    assert debug._set_parent_death_signal() is None
