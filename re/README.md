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
the four movement action-state records alongside the action mask.

Camera/render traces can additionally use `tony-camera-probe`,
`tony-camera-timing-probe`,
`tony-camera-point-probe`, `tony-camera-point-state-probe`, and
`tony-camera-collision-probe`,
`tony-view-probe`, `tony-actor-probe`, and the deliberately raw
`tony-geometry-probe [COUNT]`. For projection calibration,
`tony-view-perturb [COUNT]` alternates view-input word 6 between its observed
baseline and half-scale and records the mutation before downstream probes. It
accepts only the normalized single-player view fixture; add `--freeze` to
restore the complete baseline view-input record on every hit and isolate the
projection response from camera motion. The geometry probe accepts only submissions
The geometry probe accepts only submissions
with a live player-owned camera, so frontend/menu geometry does not consume
the bounded level observation count.

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
