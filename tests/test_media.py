from pathlib import Path

from tony.commands import _convert_raw_cd, _detect_media_format


def _raw_mode2_sector(payload: bytes = b"") -> bytes:
    sector = bytearray(2352)
    sector[:12] = b"\x00" + b"\xff" * 10 + b"\x00"
    sector[15] = 2
    sector[24:24 + len(payload)] = payload
    return bytes(sector)


def test_detect_and_convert_raw_mode2_form1(tmp_path: Path):
    source = tmp_path / "disc.img"
    pvd = bytearray(2048)
    pvd[:7] = b"\x01CD001\x01"
    pvd[80:84] = (17).to_bytes(4, "little")
    pvd[84:88] = (17).to_bytes(4, "big")
    source.write_bytes(
        b"".join(_raw_mode2_sector(pvd if index == 16 else b"") for index in range(17))
        + (b"\x00" * (2 * 2352))
    )

    media_format = _detect_media_format(source)
    assert media_format["format"] == "raw-cd-mode2-form1"
    assert media_format["user_data_offset"] == 24
    assert media_format["user_data_size"] == 2048

    destination = tmp_path / "disc.iso"
    _convert_raw_cd(source, destination, media_format)

    assert destination.stat().st_size == 17 * 2048
    with destination.open("rb") as stream:
        stream.seek(16 * 2048)
        assert stream.read(6) == b"\x01CD001"
