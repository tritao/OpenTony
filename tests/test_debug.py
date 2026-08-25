from types import SimpleNamespace

import pytest

from tony import debug, display


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
