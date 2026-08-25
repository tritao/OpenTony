import json
import struct
from pathlib import Path

import pytest

from tony.assets import (
    PkrArchive,
    PkrFormatError,
    PreArchive,
    TrgArchive,
    extract_pkr,
    extract_pre,
    inspect_pkr,
    inspect_pre,
    inspect_trg,
    inventory_assets,
)


def _write_pkr(path: Path, *, marker: int = 0xFFFFFFFE, size_again: int | None = None) -> None:
    directory_name = b"data/"
    entries = [(b"hello.txt", b"hello\n"), (b"nested.bin", b"\x00\x01\x02")]
    header_size = 16
    directory_table_size = 40
    entries_offset = header_size + directory_table_size
    file_table_size = 48 * len(entries)
    payload_offset = entries_offset + file_table_size

    result = bytearray()
    result += struct.pack("<4sIII", b"PKR2", 1, 1, len(entries))
    result += struct.pack("<32sII", directory_name, entries_offset, len(entries))
    for index, (name, data) in enumerate(entries):
        offset = payload_offset + sum(len(item[1]) for item in entries[:index])
        repeated_size = len(data) if size_again is None or index else size_again
        result += struct.pack("<32sIIII", name, marker, offset, len(data), repeated_size)
    for _name, data in entries:
        result += data
    path.write_bytes(result)


def test_read_pkr2_and_inspect(tmp_path: Path):
    source = tmp_path / "ALL.PKR"
    _write_pkr(source)

    archive = PkrArchive.read(source)
    assert archive.version == 1
    assert [entry.archive_path for entry in archive.entries] == ["data/hello.txt", "data/nested.bin"]
    assert archive.entries[0].data_offset == 152

    result = inspect_pkr(source, include_entries=True)
    assert result["format"] == "PKR2"
    assert result["file_count"] == 2
    assert result["entries"][1]["size"] == 3


def test_extract_pkr_writes_files_and_manifest(tmp_path: Path):
    source = tmp_path / "ALL.PKR"
    output = tmp_path / "output"
    _write_pkr(source)

    manifest = extract_pkr(source, output)

    assert (output / "files/data/hello.txt").read_bytes() == b"hello\n"
    assert (output / "files/data/nested.bin").read_bytes() == b"\x00\x01\x02"
    assert json.loads((output / "manifest.json").read_text()) == manifest


def test_read_and_extract_pre(tmp_path: Path):
    source = tmp_path / "LEVEL.PRE"
    source.write_bytes(
        struct.pack("<I", 2)
        + b"one.bin\0"
        + struct.pack("<I", 3)
        + b"one"
        + b"\0"
        + b"two.bin\0"
        + struct.pack("<I", 4)
        + b"two!"
    )
    output = tmp_path / "pre-output"

    archive = PreArchive.read(source)
    assert [(entry.name, entry.size) for entry in archive.entries] == [("one.bin", 3), ("two.bin", 4)]
    assert inspect_pre(source)["file_count"] == 2
    manifest = extract_pre(source, output)

    assert (output / "files/one.bin").read_bytes() == b"one"
    assert (output / "files/two.bin").read_bytes() == b"two!"
    assert json.loads((output / "manifest.json").read_text()) == manifest


def test_inventory_assets_groups_extensions(tmp_path: Path):
    (tmp_path / "data").mkdir()
    (tmp_path / "data/a.BMP").write_bytes(b"123")
    (tmp_path / "data/b.bmp").write_bytes(b"12")
    (tmp_path / "data/README").write_bytes(b"1")

    result = inventory_assets(tmp_path, examples=1)

    assert result["file_count"] == 3
    assert result["total_size"] == 6
    assert result["extensions"] == [
        {"extension": ".bmp", "file_count": 2, "total_size": 5, "examples": ["data/a.BMP"]},
        {"extension": "<none>", "file_count": 1, "total_size": 1, "examples": ["data/README"]},
    ]


def test_read_trg_header_and_node_table(tmp_path: Path):
    source = tmp_path / "SKATE_T.TRG"
    source.write_bytes(
        struct.pack("<4sII", b"_TRG", 2, 2)
        + struct.pack("<II", 20, 24)
        + struct.pack("<HH", 1, 0x1234)
        + struct.pack("<HH", 255, 0x5678)
    )

    archive = TrgArchive.read(source)
    assert [(node.offset, node.size, node.type_name) for node in archive.nodes] == [
        (20, 4, "baddy"),
        (24, 4, "terminator"),
    ]
    result = inspect_trg(source, include_nodes=True)
    assert result["node_count"] == 2
    assert result["unknown_node_types"] == []
    assert result["nodes"][1]["name"] == "terminator"


@pytest.mark.parametrize(
    "kwargs",
    [
        {"marker": 0},
        {"size_again": 99},
    ],
)
def test_invalid_pkr2_file_records_are_rejected(tmp_path: Path, kwargs):
    source = tmp_path / "bad.PKR"
    _write_pkr(source, **kwargs)

    with pytest.raises(PkrFormatError):
        PkrArchive.read(source)
