import hashlib
from pathlib import Path
from types import SimpleNamespace
from typing import ClassVar

import pytest

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


def test_split_module_preserves_coverage_and_bytes(tmp_path: Path, monkeypatch):
    configure(tmp_path, monkeypatch)
    split.split_init(SimpleNamespace(force=False, chunk_size=10))

    assert split.split_module(SimpleNamespace(start_va="0x401003", end_va="0x401007")) == 0
    manifest = split._load_manifest()
    text_modules = [module for module in manifest["modules"] if module["section"] == ".text"]
    assert [(module["start_va"], module["end_va"]) for module in text_modules] == [
        (0x401000, 0x401003),
        (0x401003, 0x401007),
        (0x401007, 0x40100A),
    ]
    assert b"".join(split._original_path(module).read_bytes() for module in text_modules) == b"0123456789"
    assert split._validate_coverage(manifest) == []


def test_compare_reports_first_mismatch(tmp_path: Path, monkeypatch, capsys):
    configure(tmp_path, monkeypatch)
    split.split_init(SimpleNamespace(force=False, chunk_size=10))
    module = split._load_manifest()["modules"][0]
    built = split._built_path(module)
    built.parent.mkdir(parents=True, exist_ok=True)
    built.write_bytes(b"012X456789")
    monkeypatch.setattr(split, "_disassembly", lambda *_args: "00401003: nop")

    assert split.split_compare(SimpleNamespace(module=module["id"])) == 1
    output = capsys.readouterr().out
    assert "matching prefix: 3 bytes" in output
    assert "first mismatch VA: 0x00401003" in output
    assert "expected disassembly" in output


def test_generate_symbols_inc(tmp_path: Path, monkeypatch):
    monkeypatch.setattr(split, "ROOT", tmp_path)

    def load_symbols(path):
        if path.endswith("functions.yml"):
            return {"functions": [{"name": "Math_Vector3Add", "address": 0x4CA9F0}]}
        if path.endswith("globals.yml"):
            return {"globals": [{"name": "Current Player", "address": 0x56A858}]}
        return {}

    monkeypatch.setattr(split, "load_yaml", load_symbols)
    assert split.split_symbols(SimpleNamespace()) == 0
    output = (tmp_path / "match/generated/symbols.inc").read_text()
    assert "Math_Vector3Add equ 0x004ca9f0" in output
    assert "Current_Player" in output


def test_compose_coverage_has_exact_complete_ownership():
    section = {"start_va": 0x401000, "raw_size": 8}
    data = b"\x90\x90AB\x00\x00CD"
    claims = [{"start_va": 0x401002, "end_va": 0x401004, "kind": "function", "name": "helper"}]

    intervals = split._compose_coverage(section, data, claims)

    assert sum(interval["size"] for interval in intervals) == len(data)
    assert [interval["kind"] for interval in intervals] == ["padding", "function", "padding", "unknown"]
    assert intervals[1]["name"] == "helper"


def test_propose_modules_does_not_mutate_manifest(tmp_path: Path, monkeypatch):
    configure(tmp_path, monkeypatch)
    split.split_init(SimpleNamespace(force=False, chunk_size=10))
    monkeypatch.setattr(split, "COVERAGE", tmp_path / "match/generated/coverage.yml")
    monkeypatch.setattr(split, "PROPOSALS", tmp_path / "match/generated/module-proposals.yml")
    split.save_yaml(
        split.COVERAGE,
        {
            "version": 1,
            "intervals": [
                {"start_va": 0x401002, "end_va": 0x401006, "size": 4, "kind": "function", "name": "helper"}
            ],
        },
    )
    before = split.MANIFEST.read_bytes()

    assert split.split_propose_modules(SimpleNamespace()) == 0
    assert split.MANIFEST.read_bytes() == before
    proposals = split.load_yaml(split.PROPOSALS)
    assert proposals["proposal_count"] == 1
    assert proposals["proposals"][0]["command"] == "tony split module 0x00401002 0x00401006"
    assert proposals["proposals"][0]["status"] == "review"

    coverage = split.load_yaml(split.COVERAGE)
    coverage["intervals"][0]["instruction_boundary_safe"] = True
    split.save_yaml(split.COVERAGE, coverage)
    args = SimpleNamespace(safe_only=True, address_range="0x401000:0x402000")
    assert split.split_propose_modules(args) == 0
    filtered = split.load_yaml(split.PROPOSALS)
    assert filtered["proposal_count"] == 1
    assert filtered["proposals"][0]["status"] == "safe"


def test_parse_address_range_rejects_backwards_range():
    with pytest.raises(SystemExit, match="greater than"):
        split._parse_address_range("0x402000:0x401000")


def test_accept_proposal_dry_run_and_apply(tmp_path: Path, monkeypatch):
    configure(tmp_path, monkeypatch)
    split.split_init(SimpleNamespace(force=False, chunk_size=10))
    monkeypatch.setattr(split, "PROPOSALS", tmp_path / "match/generated/module-proposals.yml")
    manifest = split._load_manifest()
    split.save_yaml(
        split.PROPOSALS,
        {
            "source_sha256": manifest["source_sha256"],
            "proposals": [
                {
                    "name": "helper",
                    "start_va": 0x401003,
                    "end_va": 0x401007,
                    "status": "safe",
                }
            ],
        },
    )
    before = split.MANIFEST.read_bytes()

    assert split.split_accept_proposal(SimpleNamespace(selector="helper", dry_run=True)) == 0
    assert split.MANIFEST.read_bytes() == before
    assert split.split_accept_proposal(SimpleNamespace(selector="helper", dry_run=False)) == 0
    text_modules = [module for module in split._load_manifest()["modules"] if module["section"] == ".text"]
    assert len(text_modules) == 3


def test_accept_proposals_rolls_back_failed_batch(tmp_path: Path, monkeypatch):
    configure(tmp_path, monkeypatch)
    split.split_init(SimpleNamespace(force=False, chunk_size=10))
    monkeypatch.setattr(split, "PROPOSALS", tmp_path / "match/generated/module-proposals.yml")
    manifest = split._load_manifest()
    proposals = [
        {"name": "first", "start_va": 0x401001, "end_va": 0x401003, "status": "safe"},
        {"name": "second", "start_va": 0x401005, "end_va": 0x401007, "status": "safe"},
    ]
    split.save_yaml(split.PROPOSALS, {"source_sha256": manifest["source_sha256"], "proposals": proposals})
    before_manifest = split.MANIFEST.read_bytes()
    before_sources = {path: path.read_bytes() for path in (tmp_path / "match/modules").rglob("*.asm")}
    original_split_module = split.split_module
    calls = 0

    def fail_second(args):
        nonlocal calls
        calls += 1
        if calls == 2:
            raise RuntimeError("injected failure")
        return original_split_module(args)

    monkeypatch.setattr(split, "split_module", fail_second)
    with pytest.raises(RuntimeError, match="injected"):
        split._accept_proposals(proposals, dry_run=False)

    assert split.MANIFEST.read_bytes() == before_manifest
    assert {path: path.read_bytes() for path in (tmp_path / "match/modules").rglob("*.asm")} == before_sources
