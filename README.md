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

For a lightweight per-command setup, use the repository wrapper instead:

```bash
./tony.sh doctor
```

It creates `.tools/venv` when needed, installs OpenTony in editable mode, and forwards all arguments to `tony`.

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
tony wine mount-disc
tony run
tony ghidra rebuild
tony ghidra export-functions
tony ghidra decompile 0x0041c2d0 --output build/ghidra/decomp/game-loop.c
```

`tony media extract` keeps the original image untouched, converts raw Mode 2/Form 1 CD sectors when needed, writes a normalized ISO and extraction manifest under `build/disc/`, and restores files under `build/disc/files/`. The normalized ISO contains only the declared ISO-9660 volume; use `tony media tracks` to inspect any raw tail beyond it. Generated Ghidra output is under `build/ghidra/`.

Asset extraction is currently two-stage for the PC installer: extract `ALL.PKR` first, then inspect or extract individual `.PRE` resource containers from the generated tree. For example:

```bash
tony assets extract-pkr build/disc/files/SETUP/data/ALL.PKR --output build/assets/all-pkr
tony assets extract-pre build/assets/all-pkr/files/data/LEVEL.PRE --output build/assets/level-pre
```

Both commands validate the observed container structure and write manifests under `build/`; the original archives remain untouched.

The extracted PC data also includes a `CD.HED` hash table, a matching `CD.HET` filename table, a separate `CD.HEP` filename table, and a `CD.WAD` payload file. Inspect the metadata with `tony assets inspect-hed`; `tony assets extract-hed` only writes payloads when the WAD contains data and the table describes non-overlapping direct ranges.

`tony wine mount-disc` attaches the generated ISO read-only, maps it as Wine `D:` with CD-ROM semantics, adds the raw-device `D::` mapping, and verifies that Wine can read the volume. A Linux loop device is enough for filesystem access but does not implement optical CD-ROM TOC ioctls; `tony run` and `tony play` use the generated no-CD executable to bypass that retail check. If the command warns that the raw loop device is not readable, grant the current user read-only access using the exact command it prints. Keep it mounted while running the game so disc-hosted assets remain available; clean up afterward with `tony wine unmount-disc`.

`tony exe patch-nocd` creates `THawk2.nocd.exe` beside the recorded executable and leaves the canonical retail executable unchanged. The generated copy bypasses the retail CD audio-TOC check; `tony run` and `tony play` create and use it automatically. They also launch inside a `1024x768` Wine virtual desktop, so the game cannot change the host monitor resolution. The filesystem disc mount is still useful because the game loads data, music, and movies from the disc. Use `tony play` to mount and launch, or `tony run` when the disc is already mounted.

## Useful commands

```bash
tony doctor
tony setup ghidra

tony media identify [path] [--record]
tony media tracks [path]
tony media list [path]
tony media extract [path] [--output build/disc] [--force]

tony assets inspect-pkr <path>
tony assets extract-pkr <path> [--output build/assets/pkr] [--force]
tony assets inspect-pre <path>
tony assets extract-pre <path> [--output build/assets/pre] [--force]
tony assets inventory <path>
tony assets inspect-trg <path> [--nodes]
tony assets inspect-psx <path> [--models] [--textures] [--tags]
tony assets extract-psx <path> [--output build/assets/psx] [--force]
tony assets explore <generated-asset-directory> [--open]
tony assets inspect-hed <path> [--entries]
tony assets extract-hed <path> [--output build/assets/cd-wad] [--force]

tony exe identify <path> [--record]
tony exe patch-nocd
tony verify

tony wine init
tony wine mount-disc
tony wine unmount-disc
tony run
tony play
tony run --headless --screenshot build/debug/frame.png
tony debug --record build/debug/session.mp4

tony ghidra rebuild
tony ghidra export-functions
tony ghidra decompile 0x0041c2d0 --output build/ghidra/decomp/game-loop.c

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
