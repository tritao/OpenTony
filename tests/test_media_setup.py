import hashlib
from pathlib import Path

import pytest

from tony import media_setup
from tony.common import sha256


class FakeResponse:
    def __init__(self, data: bytes):
        self.data = data
        self.offset = 0
        self.headers = {"Content-Length": str(len(data))}

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return None

    def read(self, size: int) -> bytes:
        chunk = self.data[self.offset : self.offset + size]
        self.offset += len(chunk)
        return chunk


def configure(monkeypatch, target: Path, data: bytes) -> None:
    monkeypatch.setattr(
        media_setup,
        "load_yaml",
        lambda _path: {
            "media": {
                "thps2_pc_disc": {
                    "path": str(target),
                    "source_url": "https://example.test/THPS2.img",
                    "size": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            }
        },
    )


def test_install_media_downloads_verified_file(tmp_path: Path, monkeypatch, capsys):
    target = tmp_path / "game" / "THPS2.img"
    data = b"recorded disc image"
    configure(monkeypatch, target, data)
    monkeypatch.setattr(media_setup.urllib.request, "urlopen", lambda _url, **_kwargs: FakeResponse(data))

    assert media_setup.install_media() == target
    assert target.read_bytes() == data
    assert not list(target.parent.glob(".THPS2.img.*.part"))
    assert "Downloaded 0.0 MiB / 0.0 MiB (100%)" in capsys.readouterr().out


def test_install_media_reuses_matching_file(tmp_path: Path, monkeypatch):
    target = tmp_path / "THPS2.img"
    data = b"recorded disc image"
    target.write_bytes(data)
    configure(monkeypatch, target, data)
    monkeypatch.setattr(media_setup.urllib.request, "urlopen", lambda _url, **_kwargs: pytest.fail("unexpected download"))

    assert media_setup.install_media() == target
    assert sha256(target) == hashlib.sha256(data).hexdigest()


def test_install_media_refuses_to_replace_different_file(tmp_path: Path, monkeypatch):
    target = tmp_path / "THPS2.img"
    configure(monkeypatch, target, b"recorded disc image")
    target.write_bytes(b"different image")

    with pytest.raises(SystemExit, match="refusing to replace"):
        media_setup.install_media()
