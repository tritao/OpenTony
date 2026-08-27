from pathlib import Path

from tony import wine


def test_existing_loop_prefers_already_mounted_device(monkeypatch, tmp_path: Path):
    image = tmp_path / "disc.iso"

    def fake_capture(command, **_kwargs):
        if command[0] == "losetup":
            return 0, f"/dev/loop8 {image}\n/dev/loop9 {image}"
        if command[-2:] == ["-o", "TARGET"]:
            return (0, "/media/disc") if command[3] == "/dev/loop9" else (1, "")
        raise AssertionError(command)

    monkeypatch.setattr(wine, "capture", fake_capture)

    assert wine._existing_loop_device(image) == "/dev/loop9"


def test_existing_loop_matches_canonical_worktree_image(monkeypatch, tmp_path: Path):
    shared = tmp_path / "shared.iso"
    linked = tmp_path / "worktree.iso"
    shared.touch()
    linked.symlink_to(shared)

    def fake_capture(command, **_kwargs):
        if command[0] == "losetup":
            return 0, f"/dev/loop7 {shared}"
        if command[0] == "findmnt":
            return 0, "/media/disc"
        raise AssertionError(command)

    monkeypatch.setattr(wine, "capture", fake_capture)

    assert wine._existing_loop_device(linked) == "/dev/loop7"


def test_existing_loop_prefers_current_wine_raw_mapping(monkeypatch, tmp_path: Path):
    image = tmp_path / "disc.iso"
    prefix = tmp_path / "prefix"
    dosdevices = prefix / "dosdevices"
    dosdevices.mkdir(parents=True)
    (dosdevices / "d::").symlink_to("/dev/loop9")

    def fake_capture(command, **_kwargs):
        if command[0] == "losetup":
            return 0, f"/dev/loop8 {image}\n/dev/loop9 {image}"
        if command[0] == "findmnt":
            return 0, "/media/disc"
        raise AssertionError(command)

    monkeypatch.setattr(wine, "capture", fake_capture)
    monkeypatch.setattr(wine, "wine_env", lambda: {"WINEPREFIX": str(prefix)})

    assert wine._existing_loop_device(image) == "/dev/loop9"


def test_set_disc_drive_is_noop_for_matching_links(monkeypatch, tmp_path: Path):
    prefix = tmp_path / "prefix"
    mount = tmp_path / "mounted"
    raw = tmp_path / "loop9"
    mount.mkdir()
    raw.touch()
    dosdevices = prefix / "dosdevices"
    dosdevices.mkdir(parents=True)
    (dosdevices / "d:").symlink_to(mount)
    (dosdevices / "d::").symlink_to(raw)
    monkeypatch.setattr(
        wine,
        "_wine_capture",
        lambda *_args, **_kwargs: (_ for _ in ()).throw(AssertionError("Wine should not run")),
    )

    assert wine._set_disc_drive(prefix, mount, str(raw)) is False


def test_wine_capture_reports_timeout(monkeypatch):
    def timeout(*_args, **_kwargs):
        raise wine.subprocess.TimeoutExpired(["wine"], 3)

    monkeypatch.setattr(wine.subprocess, "run", timeout)

    assert wine._wine_capture(["wine", "cmd"], timeout=3) == (124, "timed out after 3s")
