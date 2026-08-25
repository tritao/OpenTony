from pathlib import Path

import pytest

from tony import nocd


def test_nocd_patch_is_adjacent_and_idempotent(tmp_path: Path, monkeypatch):
    source = tmp_path / "THawk2.exe"
    payload = bytearray(b"\x00" * (nocd._PATCH_FILE_OFFSET + len(nocd._EXPECTED_BYTES)))
    payload[nocd._PATCH_FILE_OFFSET:] = nocd._EXPECTED_BYTES
    source.write_bytes(payload)
    monkeypatch.setattr(nocd, "recorded_executable", lambda: source)

    output = nocd.patch_nocd_executable()

    assert output == tmp_path / "THawk2.nocd.exe"
    assert output.read_bytes()[nocd._PATCH_FILE_OFFSET:] == nocd._PATCH_BYTES
    assert source.read_bytes()[nocd._PATCH_FILE_OFFSET:] == nocd._EXPECTED_BYTES
    assert nocd.patch_nocd_executable() == output


def test_nocd_patch_rejects_unknown_build(tmp_path: Path, monkeypatch):
    source = tmp_path / "THawk2.exe"
    source.write_bytes(b"\x00" * (nocd._PATCH_FILE_OFFSET + len(nocd._EXPECTED_BYTES)))
    monkeypatch.setattr(nocd, "recorded_executable", lambda: source)

    with pytest.raises(SystemExit, match="does not match the supported THPS2 build"):
        nocd.patch_nocd_executable()
