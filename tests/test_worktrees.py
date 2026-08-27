from types import SimpleNamespace

from tony import worktrees


def test_primary_worktree_prefers_main(monkeypatch):
    output = """worktree /repo/topic
branch refs/heads/topic

worktree /repo/main
branch refs/heads/main
"""
    monkeypatch.setattr(worktrees, "capture", lambda _command: (0, output))

    assert worktrees._primary_worktree() == worktrees.Path("/repo/main")


def test_worktree_verify_is_read_only(monkeypatch, capsys):
    monkeypatch.setattr(
        worktrees,
        "_capabilities",
        lambda: [
            ("static-evidence", "READY", "checked in"),
            ("live-pyghidra", "MISSING", "not provisioned"),
            ("binary-matching", "DEFERRED", "original modules"),
        ],
    )

    assert worktrees.worktree_verify(SimpleNamespace()) == 0
    output = capsys.readouterr().out
    assert "MISSING  live-pyghidra" in output
    assert "DEFERRED binary-matching" in output
    assert "worktree: PARTIAL" in output


def test_link_shared_creates_symlink(tmp_path, monkeypatch):
    source_root = tmp_path / "main"
    target_root = tmp_path / "topic"
    (source_root / "game").mkdir(parents=True)
    (source_root / "game/THPS2.img").write_bytes(b"disc")
    monkeypatch.setattr(worktrees, "ROOT", target_root)
    monkeypatch.setattr(worktrees, "_path_ready", lambda root, _relative: root == source_root)

    worktrees._link_shared(source_root, worktrees.Path("game/THPS2.img"))

    target = target_root / "game/THPS2.img"
    assert target.is_symlink()
    assert target.read_bytes() == b"disc"


def test_seed_ghidra_project_defers_when_canonical_project_is_missing(tmp_path, monkeypatch, capsys):
    source_root = tmp_path / "main"
    target_root = tmp_path / "topic"
    monkeypatch.setattr(worktrees, "ROOT", target_root)
    monkeypatch.setattr(
        worktrees,
        "load_yaml",
        lambda _path: {"ghidra": {"project_dir": "build/ghidra", "project_name": "OpenTony"}},
    )

    worktrees._seed_ghidra_project(source_root)

    assert not (target_root / "build/ghidra").exists()
    assert "DEFER build/ghidra" in capsys.readouterr().out
