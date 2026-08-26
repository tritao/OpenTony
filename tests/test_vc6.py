import hashlib
from pathlib import Path

import pytest

from tony import vc6


class FakeResponse:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return None

    def read(self, size: int) -> bytes:
        chunk = self.data[self.offset : self.offset + size]
        self.offset += len(chunk)
        return chunk


def test_download_verified_downloads_matching_media(tmp_path: Path, monkeypatch):
    data = b"vc6 disc"
    target = tmp_path / "vc6.iso"
    spec = {
        "url": "https://example.test/vc6.iso",
        "path": str(target),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }
    monkeypatch.setattr(vc6.urllib.request, "urlopen", lambda _url, **_kwargs: FakeResponse(data))

    assert vc6._download_verified(spec) == target
    assert target.read_bytes() == data


def test_download_verified_reuses_matching_media(tmp_path: Path, monkeypatch):
    data = b"vc6 disc"
    target = tmp_path / "vc6.iso"
    target.write_bytes(data)
    spec = {
        "url": "https://example.test/vc6.iso",
        "path": str(target),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }
    monkeypatch.setattr(
        vc6.urllib.request, "urlopen", lambda _url, **_kwargs: pytest.fail("unexpected download")
    )

    assert vc6._download_verified(spec) == target


def test_download_verified_refuses_unrecorded_hash(tmp_path: Path):
    with pytest.raises(SystemExit, match="has not been recorded"):
        vc6._download_verified(
            {
                "url": "https://example.test/vc6.iso",
                "path": str(tmp_path / "vc6.iso"),
                "size": 1,
                "sha256": "pending",
            }
        )


def test_overlay_case_insensitive_preserves_base_tree_casing(tmp_path: Path):
    source = tmp_path / "patch"
    destination = tmp_path / "install"
    (source / "bin").mkdir(parents=True)
    (destination / "BIN").mkdir(parents=True)
    (destination / "BIN/LINK.EXE").write_bytes(b"base")
    (source / "bin/link.exe").write_bytes(b"sp3")

    vc6._overlay_case_insensitive(source, destination)

    assert (destination / "BIN/LINK.EXE").read_bytes() == b"sp3"
    assert not (destination / "bin").exists()
