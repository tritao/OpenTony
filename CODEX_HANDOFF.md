# OpenTony handoff

This file is the restart guide for a fresh Codex/agent session or another developer machine.

## Read first

1. `README.md`
2. `docs/WORKFLOW.md`
3. `re/evidence/README.md`
4. `re/config/binaries.yml`
5. the subsystem note relevant to the current task under `re/notes/`

## Non-negotiable project rules

- `game/` is user-supplied proprietary input. Never commit or modify canonical copies.
- `build/` and `.tools/` are disposable/generated.
- `re/` is the source of truth for recovered knowledge.
- Record exact executable/media hashes before relying on addresses.
- An appealing interpretation is not a fact. Use the evidence levels in `re/evidence/README.md`.
- Prefer a controlled experiment over prolonged speculation.
- New tooling should normally be exposed through `tony`, not as an undocumented one-off command.
- Ghidra projects are rebuilt from the executable plus tracked knowledge; do not rely on local project state alone.
- Keep generated decompilation dumps out of Git unless a small excerpt is intentionally curated as evidence.

## Current stage

Initial project skeleton only. No THPS2 executable identity, addresses, data structures, or gameplay semantics have been confirmed yet.

## Baseline checks

```bash
source .tools/venv/bin/activate
tony doctor
tony verify
pytest -q
```

## Intended first milestones

1. Hash and identify `game/THPS2.img`.
2. Extract/install the PC game without modifying the canonical image.
3. Record the exact game executable identity.
4. Confirm the executable runs under the canonical Wine prefix.
5. Produce a deterministic Ghidra import.
6. Locate the main loop / frame boundary and start a first runtime trace.
7. Recover enough player state to define `warehouse-idle`, `warehouse-run`, and `warehouse-ollie` experiments.

Update this file when a milestone materially changes the project's starting assumptions.
