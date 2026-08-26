# Native reconstruction

The first native vertical slice is the level trigger runtime under `src/trg/`.
It is intentionally renderer-free and provides:

- bounded little-endian TRG loading with relative node views;
- type-6 command-point records and type-8 restart views;
- a variable-width command cursor matching the retail dispatcher alignment;
- verified pulse, visibility, gap, resource, timer, career, and condition hooks; and
- injectable `TriggerServices` plus the deterministic `LevelTriggerState` service
  for object flags, event order, timers, resources, restarts, and objectives.
- a PSX collision world that preserves blockmap-selected objects, world-space
  fixed-point faces using the retail `vertex * 0x1000 + object_position`
  placement, raw packed face metadata, surface flags, broad-phase cell
  queries, and segment hits;
- native PRE readers/catalogs for `LEVEL.PRE`, player, UI, and creation assets;
- a data-driven DirectInput keyboard-state to action-mask mapper with the
  confirmed movement defaults;
- confirmed movement action-mask history and a frame scheduler;
- retail action-record counters/analog movement thresholds, four-frame input
  snapshots, grounded turn accumulation, and a raw `PlayerState` handoff; and
- the recovered Q12 grounded yaw matrix and offset-oriented movement-basis
  handoff plus basis-to-motion correction integration; and
- the recovered position-commit axis fallback as an injectable collision
  service plus raw physics-dispatch ordering, without guessing the remaining
  skating constants.
- an executable `PlayerPhysicsFrame` that combines the confirmed history,
  correction, dispatcher, shared position integration, collision-commit, and
  post-dispatch velocity handoffs while leaving uncertain producers injectable.
- a deterministic `PlayerPhysicsReplay` for stable per-frame player snapshots
  used by a future retail-vs-native parity harness.
- a hit-aware PSX query adapter that carries face/normal/material metadata into
  the position fallback and applies the confirmed normal-component response.
- a retail-shaped `0..0x4000` collision hit parameter and signed-truncating
  contact-point reconstruction, plus `GameplayFrame` to connect input, TRG,
  and player physics in one deterministic headless iteration.
- the confirmed pure platform/bounce velocity producer for retail platform
  types 1--5, with discovery and presentation side effects left to callers.
- the retail action-record bank, exact KICK charge/release latch path, ollie
  vertical impulse producer, raw launch-state reasons, and the confirmed
  held-JUMP in-air vertical cancellation.
- the fixed-point response-vector damping producer from retail `0x0049d480`,
  including explicit random/mode-table seams and low-speed quantization.
- the grounded `0x0049df00` slope-dependent brake/stop producer and raw state-7
  transition, with surface/stat eligibility supplied by the caller; and
- the confirmed in-air Up/Down basis contribution to temporary motion
  correction, with the unresolved `+0x2dac` speed/stat source injected.

The native asset boundary also includes `src/assets/psx_asset.*`: it parses
the fixed-point PSX scene object/model tables, model-name hashes, geometry,
texture metadata/palettes, tags, and blockmaps. Trigger type-2/type-12 link
keys can be bound to those model hashes; Warehouse currently resolves 95
trigger objects to 95 PSX scene
instances and their fixed-point positions, with two deliberately unresolved
non-model keys reported as diagnostics.
`PsxArchive::decode_texture()` expands PSX 4bpp, 8bpp, and 16bpp texture
payloads to renderer-ready RGB storage.
`LevelSceneRegistry` composes those static instances with trigger-created
entities for the remaining factory records.
`LevelRenderSnapshot` converts that registry plus a PSX archive into a stable
renderer-facing list of entity state and raw model-face packets; camera,
projection, material policy, and backend upload remain separate.
`LevelRuntime` now owns the level-side ordering: TRG autoexec and node
construction, PSX/collision binding, PSX/PRE catalog resolution, frame ticking,
command-point pulses, and restart execution. Its scene entity IDs are the
handoff for a future renderer, collision world, and object factory.
`FixedStepDriver` provides the application-side accumulator around
`GameplayFrame`; its interval/catch-up values are explicit configuration until
the retail timer conversion is fully recovered.

Build and run its regression test with:

```sh
cmake -S . -B build/native
cmake --build build/native
ctest --test-dir build/native --output-on-failure
```

To exercise the native loader against an extracted retail asset:

```sh
build/native/opentony_trg_inspect /path/to/SKWARE_T.TRG
# Optional scene/object and Warehouse checklist binding:
build/native/opentony_trg_inspect /path/to/SKWARE_T.TRG /path/to/SKWARE.PSX --warehouse-gaps

# Native PSX structure check:
build/native/opentony_psx_inspect /path/to/SKWARE.PSX

# Resolve trigger factory resources against an extracted asset root:
build/native/opentony_trg_scene_inspect /path/to/SKPH_T.TRG /path/to/SKPH.PSX /path/to/assets

# C++ integration entry point:
# LevelRuntime(trg, psx, asset_root).initialize(); then tick/pulse/restart.
# GameplaySession owns LevelRuntime + PlayerState + fixed-step physics.
# PlayerPhysicsFrame::step(player, scheduler.input(), hooks) advances the
# confirmed renderer-independent player boundary.
```

The native runtime uses node indices and file-relative offsets so it remains
portable on 64-bit hosts. The retail 32-bit allocation offsets are recorded in
`re/notes/trg-cpp-runtime.md` and `re/evidence/functions/scripting.md`.

The boundary between this trigger slice and the remaining game systems is
tracked in `re/notes/trg-cpp-roadmap.md`. The broader playable-recreation
priorities are tracked in `re/notes/cpp-recreation-roadmap.md`; the next
critical native boundary is `PsxPositionCollisionProbe` feeding a future
player-state response loop.
