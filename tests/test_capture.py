from __future__ import annotations

import json
import struct
from pathlib import Path

import pytest
import yaml

from tony.capture import (
    ACTION_STRUCT,
    CONFIG_PREFIX_STRUCT,
    FRAME_STRUCT,
    HEADER_STRUCT,
    INITIAL_STATE_STRUCT,
    OTCAP_MAGIC,
    OTCAP_MAX_ACTION_INTERVALS,
    OTCAP_PLAYER_BLOB_SIZE,
    CaptureDecodeError,
    _capture_desktop_spec,
    _headless_capture_command,
    compare_recordings,
    convert_capture,
    decode_capture,
)
from tony.cli import build_parser

BUILD_SHA = bytes.fromhex("f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c")


def _capture(
    path: Path,
    *,
    frame_count: int = 1,
    timer_samples: dict[int, list[tuple[int, ...]]] | None = None,
) -> None:
    config_offset = HEADER_STRUCT.size
    config_size = CONFIG_PREFIX_STRUCT.size + ACTION_STRUCT.size * OTCAP_MAX_ACTION_INTERVALS
    initial_offset = config_offset + config_size
    data_offset = (initial_offset + INITIAL_STATE_STRUCT.size + 4095) & ~4095
    frame_limit = max(1, frame_count)
    header = HEADER_STRUCT.pack(
        OTCAP_MAGIC,
        1,
        HEADER_STRUCT.size,
        config_offset,
        config_size,
        initial_offset,
        INITIAL_STATE_STRUCT.size,
        data_offset,
        64 * 1024 * 1024,
        data_offset + frame_count * FRAME_STRUCT.size,
        frame_count,
        frame_limit,
        3,
        0,
        12,
        0x00400000,
        OTCAP_PLAYER_BLOB_SIZE,
        42,
        BUILD_SHA,
    )
    config = CONFIG_PREFIX_STRUCT.pack(1, config_size, frame_limit, 1, 12, 0, BUILD_SHA)
    config += ACTION_STRUCT.pack(0x40, 20, 8) + b"\0" * (ACTION_STRUCT.size * (OTCAP_MAX_ACTION_INTERVALS - 1))
    initial = INITIAL_STATE_STRUCT.pack(INITIAL_STATE_STRUCT.size, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    frames = []
    for index in range(frame_count):
        blob = bytearray(OTCAP_PLAYER_BLOB_SIZE)
        struct.pack_into("<I", blob, 0x08, index)
        timing = (index, 0, 0, 0, 0, 0)
        frame_timers = (timer_samples or {}).get(index, [])
        timers = [0] * 10 * 8
        for timer_index, timer in enumerate(frame_timers):
            timers[timer_index * 10 : timer_index * 10 + 10] = timer
        events = []
        for _ in range(16):
            events.extend((0, 0, 0, 0, b"\0" * 64))
        frames.append(
            FRAME_STRUCT.pack(
                index,
                0x40 if 20 <= index < 28 else 0,
                0,
                0x12340000,
                len(frame_timers),
                0,
                len(blob),
                len(blob),
                0,
                bytes(blob),
                bytes(blob),
                *timing,
                *timing,
                *timers,
                *events,
            )
        )
    path.write_bytes(header + config + initial + b"\0" * (data_offset - initial_offset - len(initial)) + b"".join(frames))


def test_capture_layout_decodes_player_and_action_mask(tmp_path):
    source = tmp_path / "capture.otcap"
    _capture(source, frame_count=21)

    decoded = decode_capture(source)

    assert decoded["build_sha256"] == BUILD_SHA.hex()
    assert decoded["actions"] == [(0x40, 20, 8)]
    assert decoded["frames"][20]["input"] == {"action_mask": 0x40}
    assert decoded["frames"][0]["before"]["position"]["raw"] == [0, 0, 0]


def test_capture_decoder_infers_timer_delivery_from_boundary_counter(tmp_path):
    source = tmp_path / "capture.otcap"
    timer_samples = {
        0: [
            (1, 0, 16, 0, 0, 0, 0, 0, 0, 0),
            (4, 0, 16, 16, 1, 0, 0, 0, 0, 0x3feeb851eb851eb8),
        ]
    }
    _capture(source, timer_samples=timer_samples)

    decoded = decode_capture(source)

    deliveries = [
        event for event in decoded["frames"][0]["events"]
        if event["type"] == "timer_callback_delivery"
    ]
    assert len(deliveries) == 1
    assert deliveries[0]["timer_boundary_delivery_count"] == 1
    assert deliveries[0]["timer_boundary_sampled_accumulated_ms"] == 16


def test_capture_decoder_rejects_unknown_status(tmp_path):
    source = tmp_path / "capture.otcap"
    _capture(source)
    raw = bytearray(source.read_bytes())
    struct.pack_into("<I", raw, 8 + 11 * 4, 4)  # CaptureHeader.status
    source.write_bytes(raw)

    with pytest.raises(CaptureDecodeError, match="capture failed"):
        decode_capture(source)


def test_capture_decoder_rejects_unknown_build_identity(tmp_path):
    source = tmp_path / "capture.otcap"
    _capture(source)
    raw = bytearray(source.read_bytes())
    raw[HEADER_STRUCT.size - 32] ^= 0x01
    source.write_bytes(raw)

    with pytest.raises(CaptureDecodeError, match="build identities differ|unsupported capture build"):
        decode_capture(source)


def test_capture_decoder_rejects_complete_but_short_bounded_run(tmp_path):
    source = tmp_path / "capture.otcap"
    _capture(source, frame_count=1)
    raw = bytearray(source.read_bytes())
    struct.pack_into("<I", raw, 8 + 10 * 4, 2)  # frame_limit
    struct.pack_into("<I", raw, HEADER_STRUCT.size + 2 * 4, 2)
    source.write_bytes(raw)

    with pytest.raises(CaptureDecodeError, match="stopped at 1 frames; expected 2"):
        decode_capture(source)


def test_capture_conversion_keeps_otrec_contract(tmp_path):
    source = tmp_path / "capture.otcap"
    output = tmp_path / "recording.otrec"
    _capture(source)

    summary = convert_capture(source, output)

    assert summary["frames"] == 1
    records = [json.loads(line) for line in output.read_text().splitlines()]
    assert records[0]["capture_schema_version"] == 2
    assert records[1]["type"] == "initial_state"
    assert records[-1] == {
        "complete": True,
        "format": "opentony-retail-recording-v1",
        "frames": 1,
        "recording_id": "inproc-recording",
        "type": "end",
    }


def test_recording_comparator_reports_first_frame_difference(tmp_path):
    left = tmp_path / "left.otrec"
    right = tmp_path / "right.otrec"
    left.write_text('{"type":"frame","frame":0,"input":{"action_mask":1}}\n')
    right.write_text('{"type":"frame","frame":0,"input":{"action_mask":2}}\n')

    result = compare_recordings(left, right)

    assert not result["equal"]
    assert result["difference"]["path"] == ["frames", 0, "input", "action_mask"]


def test_snapshot_comparator_ignores_recorder_specific_events(tmp_path):
    left = tmp_path / "gdb.otrec"
    right = tmp_path / "inproc.otrec"
    before = {"position": {"raw": [1, 2, 3]}, "timing": {"simulation_time": {"raw": 4}}}
    after = {"position": {"raw": [5, 6, 7]}, "timing": {"simulation_time": {"raw": 8}}}
    left.write_text(
        json.dumps({"type": "frame", "frame": 0, "input": {"action_mask": 64, "keyboard_state": "00"}, "before": before, "after": after, "events": [{"type": "timer_callback_delivery"}]})
        + "\n"
    )
    right.write_text(
        json.dumps({"type": "frame", "frame": 0, "input": {"action_mask": 64}, "before": before, "after": after, "events": []})
        + "\n"
    )

    result = compare_recordings(left, right, scope="snapshots")

    assert result == {"equal": True, "scope": "snapshots", "frames": 1, "difference": None}


def test_snapshot_comparator_checks_before_and_after_state(tmp_path):
    left = tmp_path / "gdb.otrec"
    right = tmp_path / "inproc.otrec"
    frame = {"type": "frame", "frame": 0, "input": {"action_mask": 0}, "before": {}, "after": {"physics_state": 1}}
    changed = {**frame, "after": {"physics_state": 2}}
    left.write_text(json.dumps(frame) + "\n")
    right.write_text(json.dumps(changed) + "\n")

    result = compare_recordings(left, right, scope="snapshots")

    assert not result["equal"]
    assert result["difference"]["path"] == ["frames", 0, "after", "physics_state"]


def test_capture_backend_parser_keeps_gdb_default_and_exposes_inproc():
    default = build_parser().parse_args(["scenario", "capture", "warehouse-idle"])
    inproc = build_parser().parse_args(["scenario", "capture", "warehouse-idle", "--backend", "inproc"])

    assert default.backend == "gdb"
    assert inproc.backend == "inproc"


def test_capture_qualification_parser_requires_two_recordings():
    args = build_parser().parse_args(
        ["capture", "qualify", "--gdb", "gdb.otrec", "--inproc", "inproc.otrec"]
    )

    assert args.gdb == "gdb.otrec"
    assert args.inproc == "inproc.otrec"


def test_capture_hook_manifest_matches_supported_pe():
    manifest = yaml.safe_load(Path("re/config/capture_hooks.yml").read_text())
    executable = Path("build/disc/files/SETUP/data/THawk2.exe")
    if not executable.is_file():
        pytest.skip("recorded executable is not hydrated")
    data = executable.read_bytes()

    for hook in manifest["hooks"].values():
        rva = int(hook["rva"])
        expected = bytes.fromhex(hook["expected"])
        assert data[rva : rva + len(expected)] == expected
        assert hook["overwrite"] >= len(expected)


def test_physics_detour_is_a_trampoline_and_not_a_gdb_stop():
    source = Path("src/capture/win32/hooks.cpp").read_text()

    assert "VirtualAlloc" in source
    assert "VirtualProtect" in source
    assert "FlushInstructionCache" in source
    assert "g_physics_trampoline" in source
    assert "ot_capture_physics_before" in source
    assert "ot_capture_physics_after" in source
    assert "pushad" in source
    assert "call dword ptr [g_physics_trampoline]" in source
    assert "target[0] = 0xe9" in source


def test_input_detour_reuses_post_poll_action_edge_boundary():
    source = Path("src/capture/win32/hooks.cpp").read_text()

    assert '"action_mask_injection"' in source
    assert "ot_capture_input_boundary" in source
    assert "ot_capture_input_hook" in source
    assert "g_input_trampoline" in source
    assert "OTCAP_ACTION_MASK_ADDRESS" in source
    assert "Preserve the untouched high word" in source


def test_timer_detour_records_boundary_samples_and_reuses_counter_inference():
    source = Path("src/capture/win32/hooks.cpp").read_text()

    assert '"timer_update"' in source
    assert '"clock_read"' in source
    assert "ot_capture_timer_boundary" in source
    assert "ot_capture_timer_hook" in source
    assert "ot_capture_clock_read_hook" in source
    assert "g_timer_trampoline" in source
    assert "g_clock_read_trampoline" in source
    assert "OTCAP_TIMER_STATE_ADDRESS" in source
    assert "OTCAP_TIMER_SIMULATION_ACCUMULATOR_ADDRESS" in source
    assert "g_frame_timer_samples" in source


def test_frontend_detours_reuse_verified_bootstrap_boundaries():
    source = Path("src/capture/win32/hooks.cpp").read_text()
    manifest = yaml.safe_load(Path("re/config/capture_hooks.yml").read_text())

    for name in (
        "frontend_play_result",
        "frontend_level_select",
        "launch_level",
        "skip_movie_primary",
        "skip_movie_secondary",
        "frontend_key_state",
        "frontend_summary",
    ):
        assert manifest["hooks"][name]["status"] == "verified"

    assert "ot_capture_frontend_play_boundary" in source
    assert "ot_capture_frontend_level_boundary" in source
    assert "ot_capture_launch_level_boundary" in source
    assert "ot_capture_frontend_key_boundary" in source
    assert "ot_capture_frontend_summary_boundary" in source
    assert "OTCAP_FRONTEND_SELECTION_CALL_RETURN" in source
    assert "OTCAP_FRONTEND_LEVEL_RESULT_ADDRESS" in source
    assert "g_frontend_summary_key_ticks" in source
    assert "g_frontend_summary_installed" in source
    assert "g_frontend_play_armed" in source
    assert "g_frontend_level_override_valid" in source
    assert "disarm_frontend_hook" in source


def test_capture_host_fails_closed_when_frontend_never_reaches_gameplay():
    source = Path("src/capture/win32/capture_host.cpp").read_text()

    assert "OTCAP_ERROR_TIMEOUT" in source
    assert "GetTickCount()" in source
    assert "OTCAP_STATUS_FAILED" in source


def test_capture_host_launches_suspended_then_resumes_after_injection():
    source = Path("src/capture/win32/capture_host.cpp").read_text()

    assert "CREATE_SUSPENDED" in source
    assert "CreateRemoteThread" in source
    assert "ResumeThread(process.hThread)" in source
    assert "executable_directory" in source


def test_inproc_capture_keeps_game_inside_managed_headless_display():
    source = Path("tony/capture.py").read_text()

    assert "HeadlessDisplay" in source
    assert "headless_wine_command(command)" in source
    assert "cwd=executable.parent" in source


def test_inproc_capture_starts_a_managed_wine_virtual_desktop():
    command = ["wine", "capture-host.exe", "--frames", "1"]
    wrapped = _headless_capture_command(command, Path("capture.otcap"), _capture_desktop_spec())

    assert wrapped[:2] == ["sh", "-c"]
    assert "wine explorer \"$desktop\"" in wrapped[2]
    assert "/desktop=OpenTony,1024x768" in wrapped
    assert "capture.otcap" in wrapped
    assert "--frames" in wrapped


def test_physics_capture_publishes_complete_records_before_count():
    source = Path("src/capture/win32/shared_buffer.cpp").read_text()

    record_write = source.index("memcpy(record->player_before")
    frame_publish = source.index("InterlockedExchange((LONG *)&header->frame_count")
    assert record_write < frame_publish
