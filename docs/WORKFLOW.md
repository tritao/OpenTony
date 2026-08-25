# Reverse-engineering workflow

OpenTony uses a loop of **identity -> hypothesis -> evidence -> experiment -> tracked knowledge -> reconstruction**.

## 1. Identity first

Every address and structure claim must belong to an exact executable build. Record hashes with:

```bash
tony media identify --record
tony exe identify game/installed/<actual-executable> --record
tony verify
```

Do not mix addresses from retail/demo/region/patch builds without explicitly recording the mapping.

## 2. Static analysis is reproducible

`tony ghidra rebuild` creates a fresh project under `build/ghidra/`, imports the recorded executable, runs analysis, then reapplies tracked names from `re/symbols/`.

Use the deterministic project to decompile one verified function at a time:

```bash
tony ghidra decompile 0x0041c2d0 --output build/ghidra/decomp/game-loop.c
```

Without `--output`, the decompiler text is printed to stdout. Keep large generated decompilations under `build/`; promote only concise, reviewed interpretations into `re/evidence/`.

The local Ghidra project is a cache. The real knowledge lives in Git.

The retail executable is also kept canonical. `tony exe patch-nocd` verifies its recorded hash and creates an adjacent `THawk2.nocd.exe` with the known CD audio-TOC gate bypassed. `tony run` and `tony play` use that generated copy automatically; they do not alter the executable used for identity or Ghidra rebuilds.

## 3. Name conservatively

Prefer:

```text
Skater_UpdateVerticalMotion
```

over:

```text
Skater_ApplyGravity
```

until the narrower meaning is proven. See `re/evidence/README.md`.

## 4. Dynamic analysis

The planned baseline is:

```text
THPS2.exe -> Wine -> WineDbg GDB proxy -> GDB -> Python callbacks
```

Start it with:

```bash
tony debug
```

For a running game, use the safe virtual-desktop launcher first and attach to its Wine PID:

```bash
tony run
tony debug --pid auto
```

`auto` asks WineDbg for the Windows PID, so it does not confuse the host Linux PID with the PID WineDbg expects. For a manual attach, pass the Windows PID shown by `winedbg --command "info proc"`.

Without `--pid`, `tony debug` launches the target inside Xvfb by default, so the debugger cannot change the host display mode.

For a headless debugger/smoke run, wrap the launch in Xvfb instead:

```bash
tony play --headless
```

Both commands use the configured 16-bit llvmpipe Xvfb profile. The isolated display is still inspectable:

```bash
tony run --headless --screenshot build/debug/frame.png
tony debug --record build/debug/session.mp4
```

OpenTony prints the temporary display connection details for external X11 tools. Screenshot paths and recordings are not overwritten. Use `tony run` or `tony play` without `--headless` for visible gameplay in the Wine virtual desktop.

`re/gdb/bootstrap.py` is loaded into GDB and is the place for small interactive helpers. Once a runtime observation becomes repeatable, move the procedure into a `tony` command or an experiment definition.

Useful helpers in the current GDB session:

```text
tony-thps2                         list known addresses and build identity
tony-bp-thps2 cd_check             break at the CD-check helper
tony-bp 0x004bb240 temporary       set a one-shot address breakpoint
tony-bp-thps2 level_loop           break at the active level loop
tony-bp-thps2 physics_dispatch     break at the skater physics dispatcher
tony-modules                       show loaded Wine module ranges
tony-read32 0x004bb240             read a little-endian uint32
tony-hexdump 0x004bb240 32         print a memory region
tony-dump 0x004bb240 32 build/x.bin save raw memory for later analysis
```

The THPS2 address table is explicitly tied to the recorded retail executable hash. Do not reuse those addresses for another regional, demo, or patched build without recording the mapping first.

## 5. Experiments

`re/experiments/manifest.yml` tracks named scenarios. At the beginning these are objectives rather than fully automated tests.

The eventual shape is:

```text
original THPS2 trace.jsonl
           | 
           +---- compare ---- OpenTony reconstruction trace.jsonl
```

Use stable, versioned state schemas. Avoid tests that only say "it looked right".

## 6. Controlled mutations

When probing asset or data semantics:

- copy original data into `build/experiments/<name>/`
- change exactly one field/condition where possible
- record the input hash, output hash, mutation, and observation
- never patch the canonical file in `game/`

## 7. Reconstruction

Do not rush to `src/`. A subsystem should have enough evidence and trace coverage that a replacement can be compared against the original. The first good candidate is player movement/ollie state rather than a complete renderer.
