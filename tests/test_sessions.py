import json
from pathlib import Path

import pytest

from tony import sessions
from tony.audio import AudioCleanup


def _use_temp_registry(monkeypatch, tmp_path: Path):
    root = tmp_path / "sessions"
    prefixes = tmp_path / "prefixes"
    monkeypatch.setattr(sessions, "_sessions_root", lambda: root)
    monkeypatch.setattr(sessions, "_prefix_root", lambda: prefixes)
    monkeypatch.setattr(sessions, "_port_range", lambda: range(46000, 46010))
    return root, prefixes


def test_create_sessions_get_distinct_ports_and_prefixes(monkeypatch, tmp_path: Path):
    root, prefixes = _use_temp_registry(monkeypatch, tmp_path)

    first = sessions.create_session("warehouse", None, isolated=True)
    second = sessions.create_session("menu", None, isolated=True)

    assert first.port != second.port
    assert first.prefix == prefixes / "warehouse"
    assert second.prefix == prefixes / "menu"
    assert json.loads((root / "warehouse/session.json").read_text())['port'] == first.port
    assert json.loads((root / "menu/session.json").read_text())['port'] == second.port


def test_create_session_rejects_active_port_and_duplicate_name(monkeypatch, tmp_path: Path):
    _use_temp_registry(monkeypatch, tmp_path)
    first = sessions.create_session("warehouse", 46000, isolated=True)

    with pytest.raises(SystemExit, match="already in use"):
        sessions.create_session("menu", 46000, isolated=True)
    with pytest.raises(SystemExit, match="already exists"):
        sessions.create_session("warehouse", None, isolated=True)
    assert first.session_id == "warehouse"


def test_stop_and_clean_session(monkeypatch, tmp_path: Path):
    _use_temp_registry(monkeypatch, tmp_path)
    session = sessions.create_session("warehouse", None, isolated=True)

    sessions.stop_session("warehouse")
    assert sessions.load_session("warehouse").data["status"] == "stopped"
    sessions.clean_session("warehouse")

    assert not session.prefix.exists()
    with pytest.raises(SystemExit, match="not found"):
        sessions.load_session("warehouse")


def test_clean_rejects_active_session(monkeypatch, tmp_path: Path):
    _use_temp_registry(monkeypatch, tmp_path)
    sessions.create_session("warehouse", None, isolated=True)

    with pytest.raises(SystemExit, match="still active"):
        sessions.clean_session("warehouse")


def test_dead_active_session_is_stale_and_cleanable(monkeypatch, tmp_path: Path, capsys):
    _use_temp_registry(monkeypatch, tmp_path)
    session = sessions.create_session("warehouse", None, isolated=True)
    session.update(status="running", proxy_pid=99991, gdb_pid=99992)
    monkeypatch.setattr(sessions, "_pid_alive", lambda pid: False)

    assert session.active is False
    sessions.sessions_list(None)
    assert "stale" in capsys.readouterr().out
    sessions.clean_session("warehouse")
    assert not session.path.exists()


def test_clean_preserves_session_when_audio_cleanup_fails(monkeypatch, tmp_path: Path):
    _use_temp_registry(monkeypatch, tmp_path)
    session = sessions.create_session("warehouse", None, isolated=True)
    session.update(
        status="stopped",
        audio_muted=True,
        audio_pactl="/usr/bin/pactl",
        audio_module_id="271",
        audio_sink="opentony_debug_warehouse",
    )
    monkeypatch.setattr(
        sessions,
        "cleanup_muted_audio",
        lambda _data: AudioCleanup(False, "pactl-unavailable"),
    )

    with pytest.raises(SystemExit, match="could not clean debug session audio"):
        sessions.clean_session("warehouse")

    assert session.path.exists()
    assert sessions.load_session("warehouse").data["audio_cleanup_error"] == "pactl-unavailable"
