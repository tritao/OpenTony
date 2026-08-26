# Reconstruction workflow

This document is the canonical process for turning retail THPS2 behavior into
exact matching source and maintainable native OpenTony code. It complements
the observation workflow in `docs/WORKFLOW.md` and the byte-ownership rules in
`match/POLICY.md`. For the compact operational entry point, use
`docs/SLICE_WORKFLOW.md`.

## Separate outcomes

OpenTony deliberately maintains three layers:

```text
retail THawk2.exe
        |
        v
match/  exact-byte implementation and verification oracle
        |
        v
re/     evidence, symbols, recovered layouts, traces, and confidence
        |
        v
src/    portable semantic recreation and native tests
```

They answer different questions:

- `match/`: can we reproduce the retail instructions exactly?
- `re/`: what do we know, how do we know it, and how certain is it?
- `src/`: can maintainable code reproduce the observed behavior?

Do not copy native C++ into `match/cpp` unless the pinned matching compiler
emits the exact retail bytes. Do not treat matching assembly as the final
native implementation.

## Work by slice

A slice is the smallest independently reviewable reconstruction outcome. It may
cover one function, a call chain, a data format, or a vertical gameplay/engine
feature; it does not need to be an entire subsystem. Do not select functions
merely because they are adjacent or easy to transcribe.

For each target, establish:

- exact function boundary and byte ownership;
- calling convention, arguments, return behavior, and stack cleanup;
- callers, callees, object fields, globals, and important branches;
- the evidence and native subsystem that consume the result.

Compiler/runtime glue and import thunks may remain matching-only. Gameplay,
collision, input, scripting, and asset behavior normally need both exact
evidence and a native implementation.

Represent active vertical work in `re/slices/`. Before substantial work, claim
the slice locally and use its command-generated checklist:

```bash
tony slice claim ID
tony slice show ID
tony ghidra gaps --slice ID
```

Release the claim when the session ends. Claims under `build/slices/leases/`
coordinate parallel sessions only; committed manifests remain the reviewable
scope and completion contract.

## Reconstruction stages

### 1. Analyze before transcribing

Use Ghidra as a hypothesis generator, not a source-code oracle. Confirm
important claims with exact disassembly, callers/callees, runtime probes,
debug symbols, data traces, and existing tests.

Keep generated decompilation under `build/`. Promote only reviewed conclusions
to `re/evidence/`, `re/symbols/`, and `re/types/`. Never paste unreviewed Ghidra
output and mark it reconstructed C++.

### 2. Recover accessed retail fields

Before attempting matching C++, record every object-relative field accessed by
the target function in `re/types/`. Confirm offset, width, signedness, and
read/write behavior from instructions and related call sites. Use a neutral
name such as `field_3200` when the offset is observed but its meaning is not.

Recover layouts incrementally; do not block a function on reconstructing an
entire large object. Validate the corpus with:

```bash
tony types verify
```

`tony ghidra rebuild` then generates fixed Ghidra structures from the validated
corpus. It applies only observed or confirmed fields, leaving weaker claims as
opaque gaps, and records omissions in `build/ghidra/recovered-types.json`.
Never edit the generated Ghidra types as a substitute for updating the YAML and
its evidence.

During iteration, use `tony ghidra sync` instead of rebuilding the project.
Pass `--function ADDRESS` to reanalyze that function and its direct callers
after applying knowledge. Finish reviewed analysis changes with
`tony ghidra verify`; reserve a complete clean rebuild for milestones.
Use `tony ghidra inspect ADDRESS` to capture the combined analysis/evidence
context for a target, and `tony ghidra gaps` to choose the next missing
signature or function boundary from a deterministic ranked queue.

A structure-using module should not become matching `cpp` while its accessed
offsets remain undocumented. Retail layouts do not replace native domain types.

### 3. Record a semantic model

Express the understood behavior independently of compiler matching. Depending
on scope, this may be pseudocode, a small reference model, portable code under
`src/`, or a differential fixture.

The model should capture observable decisions, constants, state transitions,
and unresolved seams. Add tests before source-shape experiments obscure the
underlying behavior.

### 4. Attempt matching C or C++

For compiler-shaped functions, ordinary higher-level C or C++ is the preferred
matching target. Compile with the pinned VC6 toolchain and compare the emitted
COFF text:

```bash
tony vc6 compare text_XXXXXXXX
```

Iterate on evidence-backed source properties such as:

- signature, calling convention, types, and signedness;
- expression evaluation order and temporary values;
- loop and branch shape;
- recovered structure fields;
- compiler flags.

A module is `cpp` only when its body is genuine higher-level code, contains no
naked inline assembly, passes semantic tests, and emits the retail bytes.

### 5. Time-box compiler matching

Do not guess indefinitely at register allocation or instruction scheduling. If
semantics are understood but higher-level matching stops producing useful new
evidence, preserve the exact implementation using the status defined in
`match/POLICY.md`:

- prefer standalone `asm` for large instruction-level reconstructions;
- use `vc6_asm` for small thunks, ABI adapters, compiler/runtime glue, or a
  temporary VC6-specific representation;
- use `hybrid` while only a documented portion is understood.

Assembly remains an oracle beneath the native implementation. Converting all
retail code to naked inline assembly is not a project milestone.

### 6. Implement portable native behavior

Native code under `src/` should use maintainable types and interfaces. It does
not need to reproduce VC6 instruction selection, object addresses, or platform
APIs when an observable contract can be preserved more cleanly.

Recovered layouts in `re/types/` describe retail memory. Native domain types
remain C++ source. Translate between them with explicit adapters rather than
leaking provisional packing into gameplay code.

### 7. Validate both axes

Exact matching validation:

```bash
tony types verify
tony split rebuild
tony split verify
tony vc6 compare text_XXXXXXXX  # when the module uses matching C/C++
```

Semantic validation:

```bash
pytest -q
cmake --build build/native
ctest --test-dir build/native
```

Use the commands relevant to the current tree and subsystem. The strongest
completion criterion is a deterministic comparison of retail and native
observations at the same semantic boundary.

## Choosing the implementation route

| Target | Preferred route |
|---|---|
| Simple compiler-generated logic | analysis, semantic test, direct matching C/C++ |
| Gameplay or state logic | evidence, semantic model, matching-C++ attempt, native parity test |
| Large uncertain function | analyze and trace before any broad transcription |
| Tiny thunk or ABI glue | `asm` or `vc6_asm` |
| Known compiler/runtime routine | identify upstream algorithm, then matching C/C++ |
| Understood function with stubborn code generation | exact assembly plus separate native C++ |
| Replaced platform service | retain exact oracle and implement a portable native adapter |

## Progress and completion

Track exact matching and native recreation independently. Matching statuses are
defined only in `match/POLICY.md`. Native progress should distinguish at least:

```text
unmodeled -> modeled -> tested -> trace-validated -> integrated
```

Record function-level native progress in `re/native/functions.yml` and validate
it with `tony native verify`. A mapping must cite its source, tests, and evidence
without treating a partial portable boundary as complete retail behavior.

Do not mark a subsystem complete merely because its bytes are transcribed, or
because a native approximation passes isolated tests.

A completed gameplay slice should normally include:

- exact build/address identity;
- reviewed evidence and confidence;
- recovered retail layouts needed by the boundary;
- exact matching source for the critical retail functions, where valuable;
- maintainable native behavior;
- unit tests and at least one retail/native parity fixture;
- documented unresolved behavior.
