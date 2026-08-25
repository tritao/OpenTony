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

The local Ghidra project is a cache. The real knowledge lives in Git.

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

`re/gdb/bootstrap.py` is loaded into GDB and is the place for small interactive helpers. Once a runtime observation becomes repeatable, move the procedure into a `tony` command or an experiment definition.

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
