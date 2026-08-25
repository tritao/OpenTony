import json
import struct
from pathlib import Path

import pytest

from tony.assets import (
    HedArchive,
    HedFormatError,
    PkrArchive,
    PkrFormatError,
    PreArchive,
    PsxArchive,
    TrgArchive,
    _filename_crc32,
    extract_hed,
    extract_pkr,
    extract_pre,
    extract_psx,
    inspect_hed,
    inspect_pkr,
    inspect_pre,
    inspect_psx,
    inspect_trg,
    inventory_assets,
)


def _write_hed_family(directory: Path, *, wad: bytes = b"abc") -> Path:
    name = b"hello.bin"
    het = bytearray(name + b"\0")
    het.extend(b"\0" * ((-len(het)) & 3))
    het.extend(struct.pack("<II", 0, len(wad)))
    het.extend(b"\xff\xff\xff\xff")
    (directory / "CD.HET").write_bytes(het)

    hed = struct.pack("<III", _filename_crc32(name), 0, len(wad)) + b"\0\0\0\0"
    (directory / "CD.HED").write_bytes(hed)
    (directory / "CD.WAD").write_bytes(wad)
    return directory / "CD.HED"


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


def _write_psx(path: Path) -> None:
    data = bytearray(b"\0" * 12)
    data.extend(b"\0" * 36)
    data.extend(struct.pack("<I", 1))
    data.extend(struct.pack("<I", 0))
    model_offset = len(data)
    data.extend(struct.pack("<HHHH", 0, 1, 1, 1))
    data.extend(struct.pack("<I", 1))
    data.extend(struct.pack("<hhhhhh", 0, 0, 0, 0, 0, 0))
    data.extend(struct.pack("<I", 0))
    data.extend(struct.pack("<hhhh", 1, 2, 3, 0))
    data.extend(struct.pack("<hhhh", 0, 1, 0, 0))
    data.extend(struct.pack("<HH", 0x10, 16))
    data.extend(struct.pack("<BBBB", 0, 0, 0, 0))
    data.extend(struct.pack("<BBBB", 0x20, 0, 0, 0))
    data.extend(struct.pack("<HH", 0, 0))
    tag_offset = len(data)
    data.extend(struct.pack("<II", 0x0000000A, 4))
    data.extend(b"TAG!")
    data.extend(struct.pack("<I", 0xFFFFFFFF))
    data.extend(struct.pack("<I", 0x1111))
    data.extend(struct.pack("<I", 1))
    data.extend(struct.pack("<I", 0x2222))
    data.extend(struct.pack("<I", 0))
    data.extend(struct.pack("<I", 1))
    data.extend(struct.pack("<I", 0x3333))
    data.extend(b"\0" * (256 * 2))
    data.extend(struct.pack("<I", 1))
    texture_offset_table = len(data)
    data.extend(struct.pack("<I", 0))
    texture_offset = len(data)
    data.extend(struct.pack("<IIIIHH", 0, 256, 0x3333, 0, 4, 2))
    data.extend(b"\0" * 8)
    struct.pack_into("<HHII", data, 0, 4, 2, tag_offset, 1)
    struct.pack_into("<I", data, 12 + 36 + 4, model_offset)
    struct.pack_into("<I", data, texture_offset_table, texture_offset)
    path.write_bytes(data)


def test_read_and_inspect_psx_metadata(tmp_path: Path):
    source = tmp_path / "MODEL.PSX"
    _write_psx(source)

    archive = PsxArchive.read(source)
    assert archive.version == 4
    assert archive.models[0].face_count == 1
    assert archive.tags[0].type_name == "blockmap"
    assert archive.textures[0].width == 4

    result = inspect_psx(source, include_models=True, include_textures=True, include_tags=True)
    assert result["model_count"] == 1
    assert result["texture_count"] == 1
    assert result["tags"][0]["name"] == "blockmap"

    output = tmp_path / "psx-output"
    manifest = extract_psx(source, output)
    assert (output / "textures/texture_00002222_0000.ppm").is_file()
    assert (output / "models/model_0000.obj").is_file()
    assert manifest["textures"][0]["width"] == 4
    assert json.loads((output / "manifest.json").read_text()) == manifest


def test_read_inspect_and_extract_hed_family(tmp_path: Path):
    source = _write_hed_family(tmp_path)

    archive = HedArchive.read(source)
    assert archive.entries[0].name == "hello.bin"
    assert archive.name_matches == 1
    assert archive.wad_status == "data"
    assert archive.out_of_bounds_count == 0

    result = inspect_hed(source, include_entries=True)
    assert result["format"] == "HED/WAD"
    assert result["file_count"] == 1
    assert result["name_matches"] == 1
    assert result["entries"][0]["output_name"] == "hello.bin"

    output = tmp_path / "hed-output"
    manifest = extract_hed(source, output)
    assert (output / "files/hello.bin").read_bytes() == b"abc"
    assert json.loads((output / "manifest.json").read_text()) == manifest


def test_extract_hed_rejects_zero_wad(tmp_path: Path):
    source = _write_hed_family(tmp_path, wad=b"\0\0\0")

    with pytest.raises(SystemExit, match="all-zero WAD"):
        extract_hed(source, tmp_path / "hed-output")


def test_extract_hed_rejects_overlapping_ranges(tmp_path: Path):
    name_a = b"a.bin"
    name_b = b"b.bin"
    het = bytearray()
    for name, size in ((name_a, 3), (name_b, 3)):
        het.extend(name + b"\0")
        het.extend(b"\0" * ((-len(het)) & 3))
        het.extend(struct.pack("<II", 0, size))
    het.extend(b"\xff\xff\xff\xff")
    (tmp_path / "CD.HET").write_bytes(het)
    (tmp_path / "CD.HED").write_bytes(
        struct.pack("<III", _filename_crc32(name_a), 0, 3)
        + struct.pack("<III", _filename_crc32(name_b), 1, 3)
        + b"\0\0\0\0"
    )
    (tmp_path / "CD.WAD").write_bytes(b"abcd")

    with pytest.raises(SystemExit, match="ranges overlap"):
        extract_hed(tmp_path / "CD.HED", tmp_path / "hed-output")


def test_invalid_hed_table_is_rejected(tmp_path: Path):
    source = tmp_path / "CD.HED"
    source.write_bytes(b"\0" * 5)

    with pytest.raises(HedFormatError):
        HedArchive.read(source)


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
