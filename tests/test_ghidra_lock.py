from tony import ghidra_lock


def test_ghidra_project_lock_uses_private_worktree_lock(tmp_path, monkeypatch):
    calls = []
    monkeypatch.setattr(ghidra_lock, "flock", lambda descriptor, operation: calls.append((descriptor, operation)))

    with ghidra_lock.ghidra_project_lock(tmp_path):
        assert (tmp_path / ".tools/locks/ghidra-project.lock").is_file()

    assert len(calls) == 1
    assert calls[0][1] == ghidra_lock.LOCK_EX | ghidra_lock.LOCK_NB


def test_ghidra_project_lock_waits_after_contention(tmp_path, monkeypatch, capsys):
    operations = []

    def lock(_descriptor, operation):
        operations.append(operation)
        if len(operations) == 1:
            raise BlockingIOError

    monkeypatch.setattr(ghidra_lock, "flock", lock)

    with ghidra_lock.ghidra_project_lock(tmp_path):
        pass

    assert operations == [ghidra_lock.LOCK_EX | ghidra_lock.LOCK_NB, ghidra_lock.LOCK_EX]
    assert "Waiting for another Ghidra command" in capsys.readouterr().err
