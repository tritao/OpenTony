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

Milestone 1 is complete. `game/THPS2.img` is recorded as a 2352-byte-sector raw CD image with Mode 2/Form 1 sectors:

- size: `830514720` bytes / `353110` raw sectors
- SHA-256: `d7cb5caaa9751b9afced2ebbca68b74fdc0f7a0df70fa4d550230cd7ac33a66e`
- ISO-9660 volume: `316808` user-data sectors
- raw tail beyond the filesystem volume: `36302` sectors / `85382304` bytes
- raw-tail SHA-256: `0452b0321938bf7da2a52d0540ab9f2030aaac6c230c939ff621e7075eff60bf`

The raw tail is recorded but intentionally unclassified; the normalized ISO contains only the declared ISO-9660 volume. No THPS2 executable identity, addresses, data structures, or gameplay semantics have been confirmed yet.

## Baseline checks

```bash
source .tools/venv/bin/activate
tony doctor
tony verify
pytest -q
```

## Intended first milestones

1. Extract/install the PC game without modifying the canonical image.
2. Identify `Setup.exe` and the installed game layout.
3. Record the exact retail `THPS2.exe` identity and PE32/i386 metadata.
4. Confirm the executable runs under the canonical Wine prefix.
5. Produce a deterministic Ghidra import only after executable identity is recorded.
6. Locate the main loop / frame boundary and start a first runtime trace.
7. Recover enough player state to define `warehouse-idle`, `warehouse-run`, and `warehouse-ollie` experiments.

Update this file when a milestone materially changes the project's starting assumptions.
