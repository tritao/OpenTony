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
