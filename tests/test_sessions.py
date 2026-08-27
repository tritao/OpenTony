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


def test_session_process_ids_find_reparented_marker_and_prefix(monkeypatch, tmp_path: Path):
    _use_temp_registry(monkeypatch, tmp_path)
    session = sessions.create_session("warehouse", None, isolated=True)
    proc = tmp_path / "proc"
    marker_process = proc / "101"
    prefix_process = proc / "102"
    unrelated_process = proc / "103"
    for process in (marker_process, prefix_process, unrelated_process):
        process.mkdir(parents=True)
    (marker_process / "environ").write_bytes(b"TONY_SESSION_ID=warehouse\0")
    (prefix_process / "environ").write_bytes(
        f"WINEPREFIX={session.prefix.resolve()}\0".encode()
    )
    (unrelated_process / "environ").write_bytes(
        b"TONY_SESSION_ID=another-session\0WINEPREFIX=/tmp/unrelated\0"
    )

    assert sessions._session_process_ids(session, proc) == {101, 102}


def test_terminate_session_runtime_stops_wrappers_wine_and_reparented_processes(
    monkeypatch,
    tmp_path: Path,
):
    _use_temp_registry(monkeypatch, tmp_path)
    session = sessions.create_session("warehouse", None, isolated=True)
    session.update(status="running", proxy_pid=201, gdb_pid=202)
    terminated = []
    commands = []

    monkeypatch.setattr(sessions, "_session_pid_alive", lambda pid, _session_id: pid in {201, 202})
    monkeypatch.setattr(sessions, "_terminate_pid", lambda pid, timeout=2.0: terminated.append(pid))
    monkeypatch.setattr(sessions, "_session_process_ids", lambda _session: {203, 204})
    monkeypatch.setattr(
        sessions.subprocess,
        "run",
        lambda command, **kwargs: commands.append((command, kwargs)) or None,
    )

    sessions.terminate_session_runtime(session)

    assert terminated == [202, 201, 203, 204]
    assert [command for command, _kwargs in commands] == [
        ["wineserver", "-k"],
        ["wineserver", "-w"],
    ]
    assert all(
        kwargs["env"]["WINEPREFIX"] == str(session.prefix)
        and kwargs["env"]["TONY_SESSION_ID"] == "warehouse"
        for _command, kwargs in commands
    )


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


def test_cleanup_prefix_retains_session_metadata(monkeypatch, tmp_path: Path):
    _use_temp_registry(monkeypatch, tmp_path)
    session = sessions.create_session("warehouse", None, isolated=True)
    (session.prefix / "drive_c").mkdir()
    session.update(status="stopped")
    monkeypatch.setattr(sessions, "_live_wine_prefixes", lambda: set())

    assert sessions.cleanup_session_prefix(session) is True
    assert not session.prefix.exists()
    assert session.path.exists()
    assert sessions.load_session("warehouse").data["prefix_cleaned_at"]


def test_prune_protects_live_prefixes(monkeypatch, tmp_path: Path):
    _root, prefixes = _use_temp_registry(monkeypatch, tmp_path)
    stale = prefixes / "stale"
    live = prefixes / "live"
    stale.mkdir(parents=True)
    live.mkdir()
    monkeypatch.setattr(sessions, "_worktree_prefix_roots", lambda: {prefixes})
    monkeypatch.setattr(sessions, "_live_wine_prefixes", lambda: {live.resolve()})

    removed, protected = sessions.prune_session_prefixes()

    assert removed == [stale]
    assert protected == [live]
    assert not stale.exists()
    assert live.exists()
