from __future__ import annotations

import sys
from contextlib import contextmanager
from fcntl import LOCK_EX, LOCK_NB, flock
from pathlib import Path

from .common import ROOT


@contextmanager
def ghidra_project_lock(root: Path = ROOT):
    """Serialize writable Ghidra access within one worktree.

    The lock lives outside ``build/ghidra`` because a deterministic rebuild
    replaces that directory. Separate worktrees retain separate locks and may
    continue analyzing in parallel.
    """

    path = root / ".tools/locks/ghidra-project.lock"
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+", encoding="utf-8") as stream:
        try:
            flock(stream.fileno(), LOCK_EX | LOCK_NB)
        except BlockingIOError:
            print(f"Waiting for another Ghidra command in {root} ...", file=sys.stderr, flush=True)
            flock(stream.fileno(), LOCK_EX)
        yield
