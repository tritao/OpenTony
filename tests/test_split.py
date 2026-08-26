import hashlib
from pathlib import Path
from types import SimpleNamespace
from typing import ClassVar

from tony import split


class FakeSection:
    def __init__(self, name: bytes, rva: int, raw_offset: int, raw_size: int, virtual_size: int):
        self.Name = name
        self.VirtualAddress = rva
        self.PointerToRawData = raw_offset
        self.SizeOfRawData = raw_size
        self.Misc_VirtualSize = virtual_size


class FakePE:
    OPTIONAL_HEADER = SimpleNamespace(ImageBase=0x400000)
    sections: ClassVar = [
        FakeSection(b".text\0\0\0", 0x1000, 0x200, 10, 10),
        FakeSection(b".data\0\0\0", 0x2000, 0x400, 4, 12),
    ]


def configure(tmp_path: Path, monkeypatch) -> tuple[Path, bytes]:
    source = tmp_path / "THawk2.exe"
    source_bytes = bytearray(0x500)
    source_bytes[0x200:0x20A] = b"0123456789"
    source_bytes[0x400:0x404] = b"data"
    source.write_bytes(source_bytes)
    config = {
        "executables": {
            "thps2_pc": {
                "path": str(source),
                "sha256": hashlib.sha256(source_bytes).hexdigest(),
            }
        }
    }
    monkeypatch.setattr(split, "ROOT", tmp_path)
    monkeypatch.setattr(split, "MANIFEST", tmp_path / "match/manifest.yml")
    original_load_yaml = split.load_yaml

    def load_yaml(path):
        if path == "re/config/binaries.yml":
            return config
        return original_load_yaml(path)

    monkeypatch.setattr(split, "load_yaml", load_yaml)
    monkeypatch.setattr(split.pefile, "PE", lambda *_args, **_kwargs: FakePE())
    return source, bytes(source_bytes)


def test_raw_split_round_trip_is_byte_identical(tmp_path: Path, monkeypatch):
    source, source_bytes = configure(tmp_path, monkeypatch)

    assert split.split_init(SimpleNamespace(force=False, chunk_size=6)) == 0
    manifest = split._load_manifest()
    assert [module["size"] for module in manifest["modules"]] == [6, 4, 4]
    assert manifest["sections"][1]["zero_fill_size"] == 8

    for module in manifest["modules"]:
        original = split._original_path(module)
        built = split._built_path(module)
        built.parent.mkdir(parents=True, exist_ok=True)
        built.write_bytes(original.read_bytes())

    output = tmp_path / "match/generated/THawk2.rebuilt.exe"
    assert split.split_rebuild(SimpleNamespace(no_build=True, output=str(output))) == 0
    assert output.read_bytes() == source_bytes
    assert output.read_bytes() == source.read_bytes()
    assert split.split_verify(SimpleNamespace()) == 0


def test_coverage_verifier_reports_gap():
    manifest = {
        "sections": [{"name": ".text", "start_va": 0x401000, "file_offset": 0x200, "raw_size": 8}],
        "modules": [
            {
                "id": "text_00401000",
                "section": ".text",
                "file_offset": 0x200,
                "size": 4,
                "start_va": 0x401000,
                "end_va": 0x401004,
            },
            {
                "id": "text_00401006",
                "section": ".text",
                "file_offset": 0x206,
                "size": 2,
                "start_va": 0x401006,
                "end_va": 0x401008,
            },
        ],
    }

    assert any("gap" in error for error in split._validate_coverage(manifest))
