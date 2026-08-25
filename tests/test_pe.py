from pathlib import Path
from types import SimpleNamespace

import pytest

import tony.pe as pe_module


def test_exe_identify_rejects_non_pe32_i386(tmp_path: Path, monkeypatch):
    executable = tmp_path / "wrong.exe"
    executable.write_bytes(b"not relevant to the fake parser")

    fake_pe = SimpleNamespace(
        FILE_HEADER=SimpleNamespace(Machine=0x8664),
        OPTIONAL_HEADER=SimpleNamespace(Magic=0x20B),
    )
    monkeypatch.setattr(pe_module.pefile, "PE", lambda _path: fake_pe)

    with pytest.raises(SystemExit, match="expected PE32/i386"):
        pe_module.exe_identify(SimpleNamespace(path=str(executable), record=False))
