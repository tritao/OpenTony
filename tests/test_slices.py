import json
from types import SimpleNamespace

import pytest

from tony import slices


def test_slice_claim_refuses_another_owner_and_release_checks_owner(tmp_path, monkeypatch):
    monkeypatch.setattr(slices, "LEASE_ROOT", tmp_path / "leases")
    monkeypatch.setattr(
        slices,
        "_require_slice",
        lambda _slice_id: {"artifacts": {"evidence": [], "native": [], "tests": []}},
    )
    monkeypatch.setattr(slices, "capture", lambda command: (0, "abc123\n" if "rev-parse" in command else ""))

    first = SimpleNamespace(slice_id="collision-query", owner="worker-a", force=False)
    slices.slice_claim(first)
    lease = json.loads((slices.LEASE_ROOT / "collision-query.json").read_text())
    assert lease["owner"] == "worker-a"
    assert lease["base_commit"] == "abc123"

    with pytest.raises(SystemExit, match="claimed by worker-a"):
        slices.slice_claim(SimpleNamespace(slice_id="collision-query", owner="worker-b", force=False))
    with pytest.raises(SystemExit, match="claimed by worker-a"):
        slices.slice_release(SimpleNamespace(slice_id="collision-query", owner="worker-b", force=False))

    assert slices.slice_release(SimpleNamespace(slice_id="collision-query", owner="worker-a", force=False)) == 0
    assert not (slices.LEASE_ROOT / "collision-query.json").exists()


def test_slice_verify_reports_repository_summary(monkeypatch, capsys):
    monkeypatch.setattr(slices, "validate_slices", lambda: ([], {"slices": 2, "active": 1}))

    assert slices.slice_verify(SimpleNamespace()) == 0
    assert "2 total, 1 active" in capsys.readouterr().out


def test_lease_root_uses_git_common_directory(monkeypatch):
    monkeypatch.setattr(slices, "LEASE_ROOT", None)
    monkeypatch.setattr(slices, "capture", lambda _command: (0, "/repo/.git\n"))

    assert slices._lease_root() == slices.Path("/repo/.git/opentony/slice-leases")


def test_read_lease_migrates_legacy_claim(tmp_path, monkeypatch):
    shared = tmp_path / "shared"
    legacy = tmp_path / "legacy"
    legacy.mkdir()
    (legacy / "collision-query.json").write_text('{"owner": "worker-a"}\n')
    monkeypatch.setattr(slices, "LEASE_ROOT", shared)
    monkeypatch.setattr(slices, "LEGACY_LEASE_ROOT", legacy)

    assert slices._read_lease("collision-query")["owner"] == "worker-a"
    assert (shared / "collision-query.json").is_file()
    assert not (legacy / "collision-query.json").exists()


def test_slice_prompt_includes_context_and_priorities(monkeypatch, capsys):
    monkeypatch.setattr(
        slices,
        "_require_slice",
        lambda _slice_id: {"open_questions": ["Confirm the first ABI.", "Recover the object field."]},
    )
    monkeypatch.setattr(slices, "capture", lambda _command: (0, "re/collision-query\n"))

    assert slices.slice_prompt(SimpleNamespace(slice_id="collision-query")) == 0
    output = capsys.readouterr().out
    assert "Branch: re/collision-query" in output
    assert "tony slice claim collision-query" in output
    assert "Confirm the first ABI." in output
