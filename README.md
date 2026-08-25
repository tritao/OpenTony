# OpenTony

OpenTony is a Linux-first reverse-engineering workspace for the **PC version of Tony Hawk's Pro Skater 2**.

The repository deliberately separates original game material from reverse-engineering knowledge:

- `game/` — user-supplied disc image / installed game. Never committed.
- `re/` — canonical reverse-engineering knowledge. Commit this.
- `tony/` — the Python `tony` workflow CLI. Commit this.
- `build/` — disposable generated analysis, traces, exports, and Ghidra projects. Never commit.
- `.tools/` — provisioned developer tools and the Wine prefix. Never commit.
- `src/` — eventual native reconstruction. Commit this when it exists.

## What this starter borrows

From **OpenAladdin**:

1. A single discoverable workflow frontend (`tony`).
2. Pinned, SHA-verified Ghidra provisioning.
3. PyGhidra as a native CPython integration.
4. Generated Ghidra projects instead of treating a local `.gpr` as source of truth.
5. Tracked experiment definitions and a versioned trace format.
6. A future original-vs-reimplementation differential-test workflow.

From **Quicky**:

1. Exact binary provenance and hashes before assigning addresses meaning.
2. Explicit separation between observation, inference, and confirmation.
3. Controlled experiments that modify copies, never canonical inputs.
4. Reproducible command lines and small validation tools.
5. A `CODEX_HANDOFF.md` restart guide so analysis survives context/machine changes.

Game-specific code and tools are **not** copied from either project.

## First setup

Place your image here:

```text
game/THPS2.img
```

On supported Linux distributions (Ubuntu/Linux Mint and Arch-family in this starter):

```bash
./scripts/bootstrap-linux.sh
source .tools/venv/bin/activate
```

Then:

```bash
tony doctor
tony media identify --record
tony verify
```

Do not convert or overwrite the only copy of the image. All extraction/conversion work belongs in `build/`.

Once the game is installed under `game/installed/`, identify the actual executable rather than assuming its filename:

```bash
tony exe identify game/installed/<GAME.EXE> --record
tony verify
```

Then initialize Wine and build the Ghidra project:

```bash
tony wine init
tony run
tony ghidra rebuild
tony ghidra export-functions
```

Generated Ghidra output is under `build/ghidra/`.

## Useful commands

```bash
tony doctor
tony setup ghidra

tony media identify [path] [--record]
tony media list [path]
tony media extract [path] [--output build/disc]

tony exe identify <path> [--record]
tony verify

tony wine init
tony run
tony debug

tony ghidra rebuild
tony ghidra export-functions

tony experiments list
tony compare <trace-a.jsonl> <trace-b.jsonl>
```

## Toolchain choices

- Static analysis: **Ghidra 12.1.3**
- Ghidra automation: **PyGhidra / CPython 3**, not Jython
- Java: **JDK 21**
- Runtime: **Wine stable 11+**, normal 64-bit prefix / new WoW64 model
- Dynamic debugger: **WineDbg GDB proxy + GDB Python API**
- Orchestration: **Python**

We intentionally do **not** use Git submodules initially. Ghidra and Wine are provisioned tools, not source dependencies. External THPS/Neversoft projects belong in `references/` as links until we have a concrete reason to pin one as a source dependency.

See `docs/WORKFLOW.md`, `docs/TOOLING.md`, and `CODEX_HANDOFF.md` before doing substantial RE work.
