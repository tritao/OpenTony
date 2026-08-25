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

The raw tail is recorded but intentionally unclassified; the normalized ISO contains only the declared ISO-9660 volume.

The extracted retail executable is now recorded:

- path: `build/disc/files/SETUP/data/THawk2.exe`
- size: `1450035` bytes
- SHA-256: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
- format: PE32/i386
- image base: `0x00400000`
- entry point: `0x00502f74`

The generated `build/patched/THawk2.nocd.exe` bypasses the observed CD audio-TOC gate without changing the recorded retail executable. A runtime smoke run reached DirectDraw and loaded the game paths from Wine `D:`; the generated runtime log is under `build/runtime/`. Isolated `tony debug` launches now use a `1024x768x16` Xvfb screen with Mesa llvmpipe, which reaches the executable entry point and the CD-check helper headlessly.

No main-loop, player-state, or gameplay semantics have yet been confirmed.

## Baseline checks

```bash
source .tools/venv/bin/activate
tony doctor
tony verify
pytest -q
```

## Intended first milestones

1. Extract/install the PC game without modifying the canonical image. **Complete.**
2. Identify `Setup.exe` and the installed game layout. **Complete.**
3. Record the exact retail `THawk2.exe` identity and PE32/i386 metadata. **Complete.**
4. Confirm the executable runs under the canonical Wine prefix. **Smoke run complete; repeatable runtime trace pending.**
5. Produce a deterministic Ghidra import from the recorded retail executable. **Complete.** Fresh rebuild currently exports `4739` functions.
6. Locate the startup path and main loop / frame boundary, then capture the first runtime trace. **Startup anchor observed; main loop pending.**
7. Recover enough player state to define `warehouse-idle`, `warehouse-run`, and `warehouse-ollie` experiments.

Update this file when a milestone materially changes the project's starting assumptions.
