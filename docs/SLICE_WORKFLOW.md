# Reconstruction slice quick start

A slice is the smallest independently reviewable reconstruction outcome: its
retail evidence, exact matching scope, semantic model, native behavior where
applicable, tests, and unresolved questions. A slice may be one function, a
call chain, a data format, or a vertical feature; it is not necessarily an
entire subsystem.

Detailed policy lives in `docs/RECONSTRUCTION_WORKFLOW.md` and
`match/POLICY.md`.

## Start or continue

```bash
tony verify
tony slice list
tony slice prompt ID
tony slice claim ID
tony slice show ID
tony ghidra gaps --slice ID
tony ghidra inspect ADDRESS
```

Follow the checklist printed by `tony slice show ID`. Work on one coherent
scoped target: confirm its boundary and ABI, record evidence and accessed
fields, synchronize Ghidra, add a semantic model and tests, attempt exact
matching where useful, then update native progress conservatively.

Generated decompilation and reports belong under `build/`. Do not guess unknown
semantics or classify naked assembly as semantic C++.

## Create a slice

If no suitable slice exists, create `re/slices/ID.yml` using an existing
manifest only as a structural example. Declare known functions, types, globals,
artifacts, open questions, and completion criteria. Do not invent scope merely
to make the manifest look complete.

```bash
tony slice verify
tony slice claim ID
tony slice show ID
```

## Finish

```bash
tony verify --all
pytest -q
cmake --build build/native       # when native code changed
ctest --test-dir build/native    # when native code changed
tony slice release ID
```

Commit coherent evidence, matching, and native changes. A local claim
coordinates parallel sessions; it is not evidence and is never committed.

## Parallel agents

Use one Git worktree and branch per agent. Multiple read-only sessions may use
one tree, but do not run concurrent agents that edit or commit in the same
working directory.

```bash
git worktree add ../opentony-camera -b re/camera-update
cd ../opentony-camera
tony slice prompt camera-update
```

Slice claims are stored in Git's shared administrative directory, so every
worktree sees the same owner. Each worktree should use its own editable Python
environment and writable Ghidra project; do not point several agents at one
writable project. Provision ignored inputs and generated analysis in each
worktree as needed.
