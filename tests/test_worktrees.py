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
    monkeypatch.setattr(worktrees, "_readiness", lambda: [("input", True), ("project", False)])

    assert worktrees.worktree_verify(SimpleNamespace()) == 1
    output = capsys.readouterr().out
    assert "MISSING project" in output
    assert "do not download or rebuild" in output


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
