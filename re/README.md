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
[--limit COUNT]` logs writes and stops at 256 events by default; use
`tony-watch-once` to stop at the first exact writer. Continuous movement
observation should use `tony-physics-probe [COUNT]`, which uses a software
breakpoint. If the GDB/WineDbg proxy disconnects, the trace is closed with
`complete: false` and a recovery reason. The input sampler also records
the four movement action-state records alongside the action mask.

The debugger exposes four hardware watchpoint slots. `tony-watch-batch`
accepts up to four addresses and rejects an over-capacity group before it
reaches WineDbg; split larger layout experiments into batches.

Player movement words remain semantically unresolved. `PlayerView` and runtime
records preserve each word as raw `u32` and also expose `s32`, signed 16.16,
and IEEE float32 candidate interpretations. Use `position_raw`/
`velocity_raw` as authoritative until writer evidence establishes the format;
state records follow the same explicit `*_raw` naming.
