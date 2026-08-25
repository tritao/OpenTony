from pathlib import Path

import pytest

from tony import identity
from tony.common import sha256


def test_recorded_executable_requires_matching_hash(tmp_path: Path, monkeypatch):
    executable = tmp_path / "THPS2.exe"
    executable.write_bytes(b"retail executable")
    config = {
        "executables": {
            "thps2_pc": {
                "path": str(executable),
                "sha256": sha256(executable),
            }
        }
    }
    monkeypatch.setattr(identity, "load_yaml", lambda _path: config)

    assert identity.recorded_executable() == executable

    executable.write_bytes(b"replacement executable")
    with pytest.raises(SystemExit, match="SHA-256 mismatch"):
        identity.recorded_executable()
