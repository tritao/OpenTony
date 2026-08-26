# Canonical reverse-engineering knowledge

Everything under `re/` should be reviewable evidence or a reproducible input to analysis.

- `config/` — binary/tool/runtime configuration
- `symbols/` — names and addresses we want reapplied to generated Ghidra projects
- `types/` — recovered type layouts (initially documentation/schema)
- `experiments/` — named runtime experiments
- `evidence/` — evidence records and confidence rules
- `notes/` — subsystem narratives and open questions
- `gdb/` — tracked debugger helpers

Generated analysis belongs in `build/`, not here.

Function entries may carry an evidence-backed Ghidra signature using the same
canonical type grammar as `re/types/`:

```yaml
signature:
  calling_convention: cdecl
  return: i32
  parameters:
    - {name: query, type: "pointer<SLineInfo>"}
```

Global and data entries may carry a `type` expression. `tony types verify`
validates all such bindings, and `tony ghidra rebuild` applies them after
generating recovered structures. Do not add a signature merely to improve
decompiler output; its calling convention, order, and types need supporting
evidence.

Use the generated Ghidra project as disposable cached analysis:

```bash
tony ghidra rebuild                    # clean, complete deterministic analysis
tony ghidra rebuild --profile fast     # clean iteration-oriented analysis
tony ghidra sync                       # apply changed knowledge without reimporting
tony ghidra sync --function 0x00466090 # also reanalyze the function and direct callers
tony ghidra verify                     # check fingerprints, layouts, and bindings
tony ghidra inspect 0x004638d0         # emit one function's reconstruction context
tony ghidra gaps --limit 25            # rank missing tracked knowledge
```

`sync` checks executable, Ghidra-version, profile, knowledge, and importer
fingerprints before opening the JVM. An unchanged sync is therefore a cheap
no-op. Use `--force` only to repair or test generated state. The fast profile
disables Decompiler Parameter ID, Function ID, and discovered non-returning
function analysis; use the complete profile for milestone evidence.

`inspect` combines live Ghidra structure with tracked evidence, exact matching
ownership, stack variables, referenced globals, and unresolved pointer/field
accesses. Use `--output FILE` for stable JSON consumed by scripts or reviews.
`gaps` ranks tracked functions using missing Ghidra boundaries/signatures,
unresolved parameter types, incoming-reference relevance, and raw matching
ownership. Native status is reported as `not_recorded` until an explicit
function-to-native progress map exists; do not infer completion from filenames.

Before loading the GDB bootstrap manually, generate its dependency-free symbol
module with `tony gdb generate`. `tony debug` performs this step automatically.

The first structured runtime probes are available after loading the bootstrap:
`tony-frame-clock game_loop` arms the shared candidate frame clock, and
`tony-physics-probe [COUNT]` emits conservative JSON observations from the
physics dispatcher. The frame function is deliberately selected explicitly
until runtime evidence establishes a true rendered-frame boundary.

Object experiments use in-session snapshots: `tony-snapshot idle PLAYER 0x3200`,
`tony-snapshot moving PLAYER 0x3200`, then `tony-diff idle moving`. Use
`tony-trace-open FILE EXPERIMENT` before probes to capture a header, typed probe
events, watchpoint events, and a frame-count footer as JSONL. `tony-watch ADDRESS [SIZE]
[--limit COUNT]` logs writes and latches after 256 events by default; use
`tony-watch-once` to record one exact writer and continue. Run `tony-watch-clear`
while stopped to release managed watchpoints. The repeatable movement handoff
probe is `tony-position-commit [COUNT]`, which observes four stable callers of
the shared position commit routine and records its three caller-side arguments plus player state. These
breakpoints are on the call instructions, so their arguments are read from the pushed words at `ESP`;
function-entry probes use the separate `CallContext.arg()` convention. Continuous movement
observation should use `tony-physics-probe [COUNT]`, which uses a software
breakpoint. If the GDB/WineDbg proxy disconnects, the trace is closed with
`complete: false` and a recovery reason. The input sampler also records
the complete action-state bank alongside the action mask; the physics probe
additionally captures the first player-relative movement handoff at
`0x00493370`.

For the in-air landing boundary, `tony-air-collision-probe [COUNT]` samples the
raw result and material flags at `0x00498a7d`; arm it with the dispatcher,
in-air-handler, state-request, and state-writer probes to correlate a nonzero
cast result with the exact `1 -> 0` landing write.

Debug sessions are isolated and owned by their launcher. `tony sessions list`
marks records whose owned processes have disappeared as `stale`; they are safe
to remove with `tony sessions clean SESSION`. `tony sessions stop SESSION`
remains the normal command when a debugger is still running.

The debugger exposes four hardware watchpoint slots. `tony-watch-batch`
accepts up to four addresses and rejects an over-capacity group before it
reaches WineDbg; split larger layout experiments into batches.

Player movement words remain semantically unresolved. `PlayerView` and runtime
records preserve each word as raw `u32` and also expose `s32`, signed 16.16,
and IEEE float32 candidate interpretations. Use `position_raw`/
`velocity_raw` as authoritative until writer evidence establishes the format;
state records follow the same explicit `*_raw` naming.
