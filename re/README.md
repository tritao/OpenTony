# Canonical reverse-engineering knowledge

Everything under `re/` should be reviewable evidence or a reproducible input to analysis.

- `config/` — binary/tool/runtime configuration
- `symbols/` — names and addresses we want reapplied to generated Ghidra projects
- `types/` — recovered type layouts (initially documentation/schema)
- `native/` — explicit retail-function to native-progress mappings
- `slices/` — committed vertical work scope and completion criteria
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
ownership. Native status comes only from `re/native/functions.yml`; do not
infer completion from filenames.

For normal repository validation use `tony verify`; before committing matching
or generated-analysis changes use `tony verify --all`. The specialized
`types`, `native`, `split`, and `ghidra` verification commands remain available
for focused diagnostics.

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

For the grounded motion producer, `tony-ground-motion-probe [COUNT]` samples
the raw inputs at `0x0049b010`, including all sixteen profile slots, the
indexed local-profile table value, animation cursor, cooldown/threshold,
turn gates, basis, response, and surface-side fields. Pair it with
`tony-ground-motion-profile-probe [COUNT]` to trace the profile source flags
through `0x0055fc2c` into `0x0056a3d8`, and
`tony-ground-motion-writers [COUNT] [--correction] [--control]` to capture
the exact correction/rearm stores and B010 random return sites.

`tony-rng-probe [COUNT]` traces every call to the shared `0x0048f3a0`
service, including caller, selector, frame ordinal, and return value. Its
state fields are explicitly marked unestablished until the state-advancing
routine is identified.

Dedicated state-handler observation uses `tony-special-physics-probe [COUNT]`.
It arms entry probes for state 4 (`0x00494210`), state 5 (`0x00499710`),
state 6 (`0x004993f0`), and state 8 (`0x004995d0`). Each event records the
raw state and caller, motion vectors, basis/orientation words, contact fields,
state bookkeeping, and the complete action-state bank. The probe is
observational: it does not force a state or assign a semantic name to a
handler.

For handler smoke tests, `tony-force-physics-state STATE` performs one
synthetic `player+0x30b8` write from a grounded dispatcher entry before the
dispatcher reads the field. It emits a `physics_state_force` trace event and
then leaves the normal dispatcher and handler probes to observe the retail
body. This characterizes handler side effects and fallthrough behavior only;
it does not establish the natural action/collision predicate or a canonical
gameplay transition writer.

For the in-air landing boundary, `tony-air-collision-probe [COUNT]` samples the
raw result and material flags at `0x00498a7d`; arm it with the dispatcher,
in-air-handler, state-request, and state-writer probes to correlate a nonzero
cast result with the exact `1 -> 0` landing write.

Animation probes are available for controlled Warehouse runs:
`tony-animation-sample COUNT FILE [--force]` samples the generated player's
cursor at `0x00480fa0`; `tony-animation-request-sample COUNT FILE [--force]`
logs `RunAnim` requests at `0x00480730`; and
`tony-animation-selector-sample COUNT FILE [--force]` samples steering state at
`0x00492f20`. `tony-key-loop SCAN PRESS RELEASE CYCLES` synthesizes bounded
DirectInput scan-code press/release cycles, and `tony-key-clear [SCAN]`
releases and disables them. For gameplay animation traces, arm the probes
with `tony-skip-movies` and `tony-force-level warehouse`; the level-force
breakpoint stops once at launch, so send `continue` again to reach gameplay.

Collision experiments use `tony-collision-probe [COUNT]` for the shared query
record, `tony-collision-flags-probe [COUNT]` for face metadata decoding, and
`tony-collision-dynamic-cull-probe [COUNT]` plus
`tony-collision-dynamic-probe [COUNT]` for the linked-object broad and face
passes. `tony-collision-transform-probe [COUNT]` captures the object tail and
temporary matrix around the `0x0200` matrix-transform branch.
`tony-collision-model-kind-probe [COUNT]` captures the object selector and
kind-strided model/cache slot initialization at the loader boundary.
`tony-trg-type192-probe [COUNT]` captures the type-192 command word, the
post-constructor cursor movement, and collision-facing object fields.

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

For deterministic frontend or camera-mode setup, `tony-action-sequence MASK...`
writes raw low-word action masks at the post-poll publish boundary
`0x004e4650`. Use explicit masks (for example `0x8000`, `0`, `0x10`, `0`) and
record the sequence in the trace; this drives the retail action-state
consumers without relying on synthetic X/DirectInput keyboard events.
`tony-frontend-play` is a separate level-entry control: it preserves the real
frontend selection helper, then forces the returned main-menu result to
`PLAY_GAME` at the verified caller result slot. Use it with a short trace when
the headless frontend cannot be advanced reliably by input alone.

`tony-camera-viewport-probe [COUNT] [AFTER_FRAME]` is a bounded calibration
probe for the raw `Camera_Update` viewport/framing controls. It varies one
control family per accepted camera update, records the pre-mutation camera and
global values, holds them through view preparation, and restores them at the
present boundary. Use `AFTER_FRAME` to skip startup camera calls; this probe is
for producer identification, not a gameplay zoom feature.

For ordinary model-path projection capture, `tony-transformed-vertices [COUNT]`
samples the `0x004d29e0` transform contract at its post-transform return tail
`0x004d2d9e`, reading the seven-word records at `0x00570878`. It reports raw
words plus the current projected-X/Y/Z and reciprocal-depth interpretation,
bounded to 256 vertices per observed call. For accepted calls it also retains
the raw eight-byte source-vertex block (three signed shorts plus the packed
flags word), allowing projection calibration against model-space input. It
also records a rejection reason for up to eight pre-level calls when the live
player/camera or scratch range is not readable; those diagnostics do not
consume the requested gameplay count.
For accepted calls it also records the raw object-basis, active-view-basis,
and relative-translation inputs assembled by the ordinary submitter, so the
per-object transform producer can be compared separately from the projection
consumer.
This is the preferred calibration probe; `tony-geometry-probe`'s raster-tail
records belong to the separate indexed/special path.

For bounded camera-mode validation, `tony-camera-force-mode MODE [HOLD]`
writes the raw camera mode at `camera + 0x504` for the requested number of
camera updates, records the before/after mode, and restores mode `1` on the
next accepted update. Use it only with a live level and a short trace; it is a
probe for mode handoffs, not a gameplay-mode implementation.

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
