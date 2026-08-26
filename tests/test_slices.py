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
