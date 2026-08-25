from pathlib import Path

from tony.common import ROOT, load_yaml, sha256


def test_configs_load():
    assert load_yaml("re/config/ghidra.yml")["ghidra"]["version"]
    assert load_yaml("re/config/binaries.yml")["version"] == 1


def test_sha256(tmp_path: Path):
    path = tmp_path / "x.bin"
    path.write_bytes(b"abc")
    assert sha256(path) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"


def test_root_contains_project():
    assert (ROOT / "pyproject.toml").is_file()
