# C++ recreation roadmap

Status: level-side native slice established; faithful Warehouse Free Skate
vertical slice pending.

The current C++ work has crossed the asset/runtime boundary. The remaining
work should be treated as independently testable services, with the retail
executable used as the behavioral oracle wherever names or constants are
still uncertain.

## Already usable

- Bounded native readers for TRG, PSX, and PRE data.
- Native PKR2 package loading with raw/BIBD/WIBD/ZLIB entry transforms and a
  real `ALL.PKR -> SKWARE.PSX` integration fixture.
- Native custom-park, replay/card, SC save/career-image, PC BMP, presentation
  text, and PSH part-manifest readers, each retaining the proven disk/runtime
  allocation or ownership boundary.
- Native custom-park generation state now expands the 0x10/0x2c runtime images
  and exposes the proven generated-piece -> 0x4c published-object and paired
  0x58 gap-member finalization seam with model lookup left as an explicit
  data input.
- Native SC/MMU manager state now preserves `.SAV` discovery records,
  case-insensitive lookup, 8192-byte free-block accounting, four aligned
  buffer slots, type-specific registration, and the common registered-payload
  write assembly.
- TRG node/script representation, command cursor, dispatcher, timers,
  resources, restarts, goals, gaps, and deterministic level state.
- Type-10/type-11 runtime-list state (position, raw flags, pulse/kill state)
  and type-12/type-14 registration/activation boundaries are now preserved.
- Type-1/type-7 factory option-byte lists are preserved, including the verified
  option-2/option-4 and type-7 object-flag boundaries.
- Type-12/type-14 activation preserves the verified asset-side `flags |= 4`
  and `+0x24 = 0x202020` mutation alongside the unresolved live-pointer seam.
- Concrete dispatcher field writers are retained as source-ordered native
  state for the current object/skater offsets (`0x99`/`0x9a`, `0xa0`,
  `0xa3`-`0xa8`, `0xac`, `0xad`, `0xb1`).
- PSX scene object/model binding and lazy factory-asset resolution.
- Package-backed `LevelRuntime` construction now decodes `ALL.PKR` TRG/PSX
  entries into owned images before building the same scene/object/collision
  runtime, closing the packaged-file variant of the level path.
- Package-backed level resource bindings now resolve TRG-requested names from
  `ALL.PKR` when no extracted catalog exists, so the package path exposes the
  same resource availability contract as loose files.
- Native PSX environment records now preserve the retail count-prefixed
  `0x4c` object allocation, relocated model-pointer table, and shared
  `0x2c` checksum/material records. Render snapshots carry the resolved
  runtime material index/checksum instead of stopping at a source texture
  index.
- Native TRG runtime lists now cover the proven type-13 camera-point registry,
  type-10/type-11 rail records, and type-5 powerup records, including the
  Warehouse subtype-to-`ITEMS.PSX` model bridge.
- Native traffic records now cover the proven type-1 `0x1e8` allocation,
  subtype-selected shared `C_*.PSX` regions, constructor fixed-point fields,
  and positional-sound metadata for loose-file and package-backed assets.
- Native trigger factory records now materialize the proven `0xcb`/`0x192`
  `0x1f4`/`0x218` allocations and synchronize their supported flag fields with
  the trigger state service.
- Native PSX post-model runtime readers preserve BITS type-`0x45` named
  groups and the shared animation `0x2a`/`0x2c` count/record/source-stream
  boundary, including raw hierarchy payload ownership.
- Native bounded WAV and FNT readers now cover the proven PCM sample-buffer
  and font/glyph allocation boundaries; platform audio and text backends are
  still adapters around those values.
- Native animation channel decoding/playback preserves the recovered packed
  stream formats, consumed-byte boundary, object playback fields, and modes
  0 through 4 with caller-supplied retail clocks.
- Native sound-bank state preserves the 0x34-byte description contract,
  `audio/<name>.wav` naming, 0x28-byte runtime slots, and post-start flag
  handoff; DirectSound device/voice objects remain platform adapters.
- Native PSH matching uses the runtime trailing-part labels and reproduces the
  `SK2ANIM.PSH` to `HAWK2.PSH` index remap.
- Native skater-object ownership now includes the 0x3538 gameplay allocation,
  contained 0x674 camera allocation, region/model fields, and peer link.
- Native skater asset binding now owns a selected PSX region, joins PSH model
  and animation parts by name, resolves the model-table entry, and publishes
  the proven region/model fields into the 0x3538 skater allocation.
- Native player resource spool state now preserves the 0xa10 manager, 0x40
  0x28-byte queue entries, PSH/direct-PSX dispatch, parsed resource lifetime,
  and load/wait/completion transitions. The queue also decodes the same PSH
  and direct-PSX entries from package-only `ALL.PKR` sessions.
- Native `CameraRuntime` now owns the recovered value-level camera state and
  viewport commit, with explicit mode-21/22 inputs and an opt-in
  `GameplaySession` post-physics camera update boundary.
- Native renderer packet construction now consumes the PSX-backed render
  snapshot, applies the recovered Q12 object/camera transform, preserves the
  `0xb0` polygon and 3/4-vertex boundary, and normalizes textured UVs through
  an explicit material-dimension resolver; projection/raster submission stays
  a callback until its live viewport calibration is recovered.
- Native PC texture runtime now owns the proven `0x2c` material-image records,
  resolves the four Warehouse `NEWTEX` bitmap witnesses from loose files or
  PKR entries, and decodes inline PSX textures into the same device-independent
  image product. The session passes those dimensions into render packets while
  leaving Direct3D resource pointers as a presentation adapter.
- Native in-air contact now has a direct packed-face-field predicate boundary
  for the ordinary landing branch; wall, rail, and special-contact selection
  remain explicit policy inputs.
- Native pickup rendering now joins existing TRG type-5 entities to the
  finalized `ITEMS.PSX`/`SKMEDALS.PSX` region runtimes, preserving source-node
  identity, resolved checksum/model index, region-local materials, and the
  same `0xb0` packet path. Package-only `ALL.PKR` loading supplies the optional
  item regions as well; unmapped subtypes remain geometry-less.
- Package-only `ALL.PKR` level setup also retains the proven `BITS.PSX`
  type-`0x45` named-group runtime, keeping the effect-resource lookup on the
  same common package path as scene and item assets.
- Native PSX palette/texture expansion for 4bpp, 8bpp, and 16bpp payloads.
- PSX blockmap-selected collision geometry with fixed-point world placement.
- Retail-shaped segment-query metadata: hit face/object, hit point, normal,
  surface flags, nearest travel distance, and the `0..0x4000` fixed-point
  segment parameter used to reconstruct the contact point.
- The recovered retail collision face-word filter, including caller-supplied
  reject/accept masks, raw face-class gates, and the trigger-face override.
- The shared position-commit axis fallback and a PSX-backed occupancy probe.
- Input action records, analog movement thresholds, and the poll -> level tick
  -> downstream observer frame boundary.
- Raw horizontal/vertical controller axes retained in the current input
  snapshot and four-frame history; this is needed for the retail profile/lean
  transfer at `0x0046d6e0` without pretending the axes are acceleration.
- Fixed-step and session overloads that accept an action mask plus raw axes,
  preserving the controller values through the application shell for gamepad
  and replay callers.
- A data-driven DirectInput keyboard-state mapper with the confirmed movement
  scan-code/action-bit defaults and configurable non-movement actions.
- Four-frame semantic input snapshots and the grounded Left/Right turn
  accumulator handoff.
- The grounded Q12 orientation path: signed 12-bit yaw conversion, retail
  matrix multiplication, and the `+30f4/+3100/+310c` basis-column handoff.
- The confirmed grounded basis-to-motion projection and outer-frame Q8
  correction integration, with surface/ownership predicates still injected.
- The shared ground/air fixed-point position integrator using velocity `dt`
  plus correction `dt²/2`.
- A raw `PlayerState` handoff for live/history position, response vector, and
  physics-state fields.
- A renderer-independent raw physics dispatcher with confirmed state routing,
  stage order, and state-7 position restore.
- An executable `PlayerPhysicsFrame` boundary that combines that dispatcher
  with history capture, correction reset, shared ground/air integration,
  axis-fallback collision commit, and outer correction-to-velocity handoff.
- A headless `PlayerPhysicsReplay` that records stable per-frame player
  snapshots from the same `InputState` history, suitable for native-vs-retail
  semantic trace comparisons.
- A hit-aware collision query path that feeds PSX face/normal/material
  metadata through the shared position fallback and removes the hit-normal
  velocity component using the confirmed Q12 primitive.
- The first stage of shared `0x0049bad0` inward collision response, including
  the negative-dot projection and literal `0xcd` normal bias, as a
  caller-selected frame hook. Its caller-selected orientation rewrite and
  normalized air-basis rebuild are now available as a separate frame hook.
- The confirmed pure bouncy-platform velocity producer for retail platform
  types 1--5, available to state callbacks without guessing platform discovery
  or presentation side effects.
- The opt-in `FUN_00490680` collision-velocity projection: exact normal
  removal followed by Q12 magnitude restoration, with caller-controlled branch
  selection.
- The confirmed action-record bank for non-directional inputs, exact KICK
  charge/release latch ordering, the recovered integer ollie vertical impulse
  producer, raw launch reasons `0x245c/0x2457`, and the held-JUMP in-air
  cancellation after more than two updates.
- The fixed-point response-vector damping producer from `0x0049d480`, with
  explicit random/mode-table seams and low-speed quantization.
- The grounded `0x0049df00` slope threshold/brake/stop producer, including
  raw state-7 request reason `0x2c56`, with surface eligibility still injected.
- The skater's initial B010 speed threshold (`+0x2dc8 = 0x2e9b6` from
  `0x0046c98c`), with the later random/stat update still injectable.
- A `GameplaySessionConfig::apply_ground_motion` opt-in that installs the
  recovered B010 correction path in the real level/session frame order while
  keeping the profile-table eligibility value explicit.
- A native eight-entry `GroundMotionProfileTable` with the retail mode-7
  selector XOR, plus B010's cooldown `0x14`, threshold rearm, pending event,
  and animation-speed writes. The session can exercise these with explicit
  collision-surface and RNG inputs; no host-time/random fallback is hidden.
- The retail Q12 magnitude/speed metric shared by braking and damping:
  dot-scale `1/4096`, integer square root, then caller `<< 6`. The native
  helper is shared by both consumers and covered by threshold regressions.
- The stateful non-animation `0x0049df00` ground-mode producer: separate
  `+0x2df8` mode storage, modes `0/1/3/4/5/6`, cooldown `2`, and state-7 reset
  reasons `0x2c0f`/`0x2c21`, exposed as an opt-in `GroundPhysics` frame hook.
- The scalar in-air gravity producer from `0x00497df0`: `-500` Y updates,
  terminal `-0x1000`, and X/Z reset when the pre-update Y is below `-0xe0c`.
  The candidate `+310c/+3110/+3114` vector is exposed in `PlayerState` and
  replay snapshots. Its confirmed normalization/cross-product basis handoff
  is also available as an explicit physics-frame stage; the movement handoff
  from this orientation vector into player displacement remains open.
- The confirmed in-air Up/Down correction producer from `0x00497f40`: the
  current `+30f4` basis is scaled by `(+0x2dac * 150) / 100`, shifted with
  retail arithmetic `sar 12`, then subtracted/added to temporary `+58`
  correction. `AirDirectionInput` exposes this exact operation. The confirmed
  `0x0049e680` `+0x2dac` stat/mode writer is also available through
  `compute_air_speed_scalar()`; only the state-two RNG result and mode flags
  remain service inputs.
- The first airborne ground-contact classifier: strict `normal.y > 0xccd`
  (Q12), installed by default in `GameplaySession` while face/material policy
  remains owned by the collision query.
- A `GameplayFrame` coordinator that carries one input snapshot through the
  level/TRG tick and the player physics boundary in the recovered outer order,
  while passing the caller's fixed-point frame scale to both timing domains.
- A renderer-independent `LevelRenderSnapshot` at the recovered object/model
  submission boundary: scene entity state, PSX model faces, raw local vertices,
  normals, UVs, texture references, and object placement are available to a
  backend without coupling it to TRG offsets. Pickup visual/motion bytes and
  the raw lifecycle timer/phase/fade inputs now cross this boundary too, so a
  renderer can reproduce the confirmed glow/fade state without reaching back
  into the trigger state service.
- A configurable deterministic `FixedStepDriver` around `GameplayFrame`, with
  partial-time accumulation, bounded catch-up, reported dropped time, and the
  keyboard-buffer input path. Its step interval and frame scale remain
  configuration seams until the retail timer conversion is fully isolated.
- A `GameplaySession` shell that owns `LevelRuntime`, `PlayerState`,
  `GameplayFrame`, and `FixedStepDriver`; its default physics hook queries the
  loaded PSX collision world and exposes the recovered scalar air-gravity and
  optional collision-response stages through one native entry point.
- `GameplaySessionConfig` defaults ordinary movement to the recovered retail
  collision masks and oriented plane test (`reject_mask = 0x200000`,
  `accept_mask = 0xffffffff` for the all-false startup inputs); special
  queries may still provide their own `PsxCollisionQueryOptions`.
- The ordinary session also enables the recovered `FUN_0049bad0` inward
  response stage with bias `0xcd`; state-specific frames retain the ability to
  disable or replace that hook.
- `GameplayFrameResult` now carries the exact trigger-event delta appended by
  the level tick, preserving source node, target, opcode, value, and checksum
  at the fixed frame where a script effect occurs. This is the join needed for
  retail/native event traces; the cumulative `GameplaySessionObservation`
  counts remain available for long-lived state checks.
- TRG restart execution now crosses the complete level-to-player boundary:
  `GameplaySession::execute_restart()` dispatches the named restart stream,
  applies its fixed-point position, preserves the two auxiliary restart fields,
  clears transient motion, and resets the fixed-step clock.
- `GameplaySession::initialize()` now also applies the restart selected by the
  retail autoexec, matching `Front_LoadGame -> FUN_004c4e30` before frame 1.
- `GameplaySession::pulse_node()` and `pulse_checksum()` expose the recovered
  TRG event boundary to gameplay code and synchronize script mutations back to
  the scene registry. Both paths now apply a restart generated by a command
  stream, including the KILLBRUCE/checksum path.
- `GameplaySession::observation()` provides a stable end-to-end parity record
  for one frame: input/physics result, level time and trigger counts, player
  fixed-point vectors/orientation, restart fields, and scene counts.

These pieces are renderer-independent and are covered by native regression
tests. They are not yet a complete game executable.

## What still blocks faithful recreation

The work is now separated into three kinds of missing behavior:

1. **Required for a faithful first playable level:**

   - the remaining profile/stat setup behind the now-recovered
     `0x0049b010` ground-motion producer, plus the first downstream writer
     that turns the materialized controller/profile values into persistent
     horizontal velocity. The sixteen raw `+0x2ccc` action-profile slots,
     analog thresholds, turn flags, and transient response-normalization
     boundary are represented natively; the separate
     `DAT_0056a3d8[+0x2cc4]` profile-table gate is an explicit native input;
     `0x00489a10 -> 0x00489930` input-profile materialization is no longer an
     open seam.
   - complete contact filtering and response policy, including the remaining
     back-face/trigger rules, sweep/radius behavior, slope classification,
     rails, platforms, bounce, and landing/wall transitions;
   - the remaining state handlers for grounded rolling, airborne control and
     upright orientation, braking, launch velocity, facing, animation
     eligibility, and rail/special transitions; and
   - a renderer backend, because a native physics/trigger slice is not a
     playable recreation until the level and player can be presented. The raw
     camera state/update and `GameplayPresentation` handoff now exist; the
     platform window, view/projection conversion, model submission, textures,
     depth/transparency, fog, and present path remain open.

2. **Required for behavioral completeness after that slice:**

   - the remaining object factories and live behavior around the now-modeled
     TRG/PSX records, including runtime pointer/list ownership and powerup
     update/glow state;
   - goal, gap, level-event, restart, two-player, and career state transitions;
   - all action records, animation/trick state, vehicles, pickups, audio,
     effects, and unknown script opcodes; and
   - frontend, save/profile/configuration, movie, music, and exit behavior.

3. **Required to keep the recreation faithful while it grows:**

   - deterministic retail/native traces for the same input and fixed-frame
     timing;
   - snapshots at the player, collision, trigger, object, and render seams;
   - source-record-to-native-mutation fixtures for each script command; and
   - a replay corpus covering stationary, grounded turn, ollie, landing,
     collision, restart, trigger pulse, and goal completion cases.

The newly confirmed `0x00493370` queued-impulse branch now has a verified
indirect writer: action-stream opcode `0x2b` in `0x004be450` reads an axis,
amount, and rate and writes the `+2ca0/+2c94` queue. Its neighboring opcode
`0x2c` waits by rewinding the stream cursor while any pending amount remains.
The native implementation preserves both scalar producer/drain and the retail
order in which the queue is drained, transformed through the live Q12 basis,
and committed before `0x004be450` seeds or waits for the next frame. It is
still not proven to be the ordinary keyboard steering path, so do not promote
this special action command into generic keyboard acceleration.

Action-stream case `0x0f` is now also bounded: `0x004be450` reads three signed
16-bit values, shifts them by twelve, and writes the live response vector at
`+0x4c/+0x50/+0x54`. Native `PlayerState::dispatch_action_command()` applies
that effect, and `PlayerState::run_action_sequences()` now feeds it from a
generated-table record and the native 32-entry `0x004925e0` history matcher.

The action cursor lifecycle is now persistent as well. Retail
`0x00491b80` sets `+0x29c8` and installs `+0x29cc`; `0x07` clears the active
flag, while `0x2c` rewinds the cursor and yields until the queued motion is
drained. Native `PlayerState` stores the signed stream-relative identity and
stream-local cursor, resumes active streams before matching new history, and
resets the confirmed `+0x29c0` value to `0x7b` at stream start. The
`PlayerPhysicsFrame` two-frame regression covers this yield/resume path.

The retail asset bridge is now bounded enough to prioritize: level setup calls
`0x00492a90` to load `tricks.bin`, whose signed-16-bit header offsets seed the
20-entry player-input-table selector, source tables, and per-player
sequence-table destinations.
`0x004bd1e0` builds those tables, `0x00492d50` resolves their stream offsets
directly against the loaded image and walks them through `0x004bf6c0`, and
`0x004925e0` selects a stream from the live action history before
`0x00492290` installs it at the player action cursor `+0x29cc`. Native
`assets::TricksBinView` now implements the bounded header/offset-table/direct
image-relative stream boundary, exposes the 596/27-record shipped source
tables, and the action runner preserves the recovered `0x004bf6c0` cursor
widths for unknown commands. The asset bridge is the
`0x004bd1e0`/`0x004bcf00` builder, now represented natively, that fills the
zero-filled `0x7990`/`0x7d90` table sections from the metadata/resource
sections. Static recovery now pins
that bridge to the `0x140c` heap object from `0x004bb4f0`, its count at
`+0x1404`, `0x28`-byte records beginning at `+0x04`, and stream/flag metadata
at record `+0x20/+0x26`. The deterministic ordinary/static/mapped passes are
now native in `runtime/action_sequence_builder.*`, including final-action
type classification and the raw-flag post-pass. The neighboring `0x51`
stream metadata is retained separately and is not the selection filter. The
native selection pass now also reproduces `FUN_004bbf00`: it resolves
section-0 trailing stream keys to source-record IDs, fills the 0x2b direct
static-combo slots, and fills the five mapped resource/index pairs. The
selection view is at player record `+0xcc`; its mapped fields are
view-relative `+0x2b/+0x30`, i.e. physical normal-slot offsets `+0xf7/+0xfc`.
Static setup fixes the slot stride at `0x104`, with independent
`DAT_00540e30` mapping indices. Native `RetailActionResourceSelection` now
mirrors both the direct-slot writer and mapped pair lookup/update boundary;
the live loaded-resource name/index mapping in the trick-selection path at
`0x004c36d0` still needs to feed it, and that path is not the TRG level
loader.
The catalog side of that seam is now pinned further: `0x004c2a10` creates the
36 mapped catalog records from `DAT_00540e30`, storing their ordinal as the
runtime key, while `0x004c2c10` creates the direct catalog records from
`DAT_00540cb8` with sequential runtime keys. The loaded-resource pointer list
is the 0x60-entry array at object `+0x9fc`; its child `+0x08` key is compared
against the active catalog key, and mapped types then call the selection pair
helpers at `0x00416340`/`0x00416380` through the selection view at `+0x2154`.
The native trace preserves those catalog and pair semantics. Automatic
TRICKS.BIN selection now covers the section-0/source-key and direct-slot
population path without deriving IDs from coincidental file offsets.
`GameplaySessionConfig::tricks_path` loads the real archive and runs this
automatic pass by default; explicit arrays remain available for fixtures and
for the still-unresolved live catalog/special-resource paths.

## Priority 1: player state and collision response

This is the critical path to a playable level.

1. Define a native `PlayerState` around the observed object fields, preserving
   raw fixed-point values until scale and ownership are confirmed. The current
   anchors are live position `+0x08/+0x0c/+0x10`, history `+0xbc/+0xc0/+0xc4`,
   response vector `+0x4c/+0x50/+0x54`, physics dispatch `+0x30b8`, and ground
   update state `+0x30c4`.
2. Connect the recovered `0x0049b010` ground-motion producer after the
   now-modeled action-history, sixteen-slot action-profile map, grounded turn,
   Q12 basis handoff, and KICK/ollie state boundary. Static audit places the
   immediate pre-physics transfer at `0x0046d6e0`, but that function only
   copies profile analog/lean bytes; the native shell now preserves the source
   axes and exposes the materialized profile slots to the ground-motion hook.
   `0x0049b010` writes the basis-scaled temporary correction that is integrated
   into the persistent response before `0x00496550`; its stateful
   `0x00492f20` animation writer and exact `0x00492ed0` easing are now
   represented in `ground_animation.*`, while animation asset/event dispatch
   and the real profile/stat writer behind local-profile selection still need
   their native owners. B010's profile source boundary is now represented by
   `GroundMotionProfileRecords`: the retail `0x00487c30` path reads the `+0x10`
   fields of the objects at `+0x244/+0x248`, applies the exact `==1`
   conversion into `0x0055fc2c/0x0055fc34`, and `0x00413c10` copies those
   eight entries to the runtime `0x0056a3d8/0x0056a3e0` arrays. The owner
   setup around `0x00487490` creates the four `0x54`-byte and two `0x104`-byte
   records behind the skater's `+0x244/+0x248` pointer fields; `0x00488160`
   mutates their `+0x10` values and `0x004882e1` resets them through
   `0x00413f30`. The upstream profile/stat or save-data reader is still not
   identified. B010's cooldown and random rearm side effects are now
   represented as explicit services. The post-dispatch `+0x2dc8` threshold
   decay/reseed and the retail dot/sqrt speed metric are now native services;
   the RNG roll and special-state predicate remain injected. The grounded
   `0x0049f0e5`
   call remains a collision-safe commit, not the motion producer. The
   remaining state-specific motion work is gravity/stat tables, horizontal
   launch velocity, animation eligibility, trick/rail transitions, facing,
   and the remaining stat-driven damping mode selection plus the rest of the
   `0x0049df00` brake-mode state machine;
   `PlayerPhysicsFrame` now provides the call boundary for those producers.
   The earlier B010 animation-event branch is also represented: callers can
   supply the raw `+0x107` enable flag and receive the observed `0x2537`/
   `0x2531` event reason, parameter, and state-1 `+0x108` speed write without
   confusing that event service with the correction producer.
   The complete Warehouse trace `build/debug/sessions/ground-motion-final4/ground.trace.ndjson`
   observed source initializers and runtime primary-table values of `1` at
   startup and later frames `123`, `2528`, and `4249`, with no source-table
   rewrite during that run. The startup writer is `0x004e4690`, the
   `InitDirectInput()` path that calls `0x00413f30`, so this is an input-system
   default fixture rather than an identified TRG/profile asset. Do not turn it
   into a universal stat default.
3. Extend the new hit-aware query into full response inputs: exact face filtering,
   back-face/trigger-zone rules, normal orientation, slope/ground tests,
   player sweep/radius, caller-specific use of velocity projection, and the
   rail/platform/bounce branches.
4. Reproduce the grounded, airborne, rail, and special movement states as
   separate state handlers. The first airborne launch/landing seam now exists,
   including the explicit contact classifier and landing request `0x1fd6`; add
   gravity and the horizontal/animation portions. Validate each with fixed
   input traces and position/velocity snapshots against retail.

The recovered `FUN_004624d0` -> `FUN_00466090` -> `FUN_004638d0` ->
`FUN_00462a20` -> `FUN_00463d50` chain is the starting point for this work;
the unresolved part is response policy after the hit is found.

## Priority 2: a real frame/application shell

`GameplaySession` now connects the loaded level, fixed-step input, collision
queries, player state, restart application, and the renderer snapshot boundary.
The playable program still needs:

- platform window/message pumping and the retail timer conversion;
- keyboard/gamepad mapping for all action records, including press/release
  behavior and two-player ownership;
- a resource lifetime/cache service for PRE/PSX/model/texture assets;
- audio, music, movie, and frontend callback services;
- save/profile/configuration state and a stable main-loop exit path.

These should remain adapters around the portable runtime so the game logic can
be tested headlessly.

## Priority 3: renderer and presentation

The PSX parser exposes geometry, texture metadata, palettes, model hashes, and
placement, and `LevelRenderSnapshot` now exposes those inputs at the confirmed
object/model submission seam. The recreation still needs a renderer backend
that matches the retail coordinate convention, model transforms, face winding,
texture/palette upload, material flags, visibility/fog, camera behavior,
animation, and HUD/UI composition. The backend should consume the snapshot and
never parse TRG offsets directly.

## Priority 4: gameplay/content completeness

The level runtime currently covers the verified subset of script behavior.
Raw `0xa3/0xb1` skater writes now cross the complete native boundary:
dispatcher -> `LevelTriggerState` -> `GameplaySession` -> `PlayerState` and
the presentation snapshot. Remaining content work is to classify unknown
opcodes from retail traces, complete object factories and special vehicles,
resolve goal/career/two-player state transitions, and bind level events to
visible/audio/gameplay services. Each addition should have one source record,
one native state mutation, and one replay fixture before it is generalized.

## Priority 5: parity harness

Before broad implementation, add deterministic record/replay fixtures for:

- input action transitions and frame timing;
- player position/history/state fields;
- collision query candidates and hit metadata;
- trigger pulses, timers, resources, goals, and restarts; and
- rendered scene/entity snapshots.

The player-side fixture now exists as `PlayerPhysicsReplay`; the remaining
fixture work is to feed it corrected retail position/velocity samples and to
add trigger/collision/render snapshots around the same frame index.

The harness should compare native and retail observations at stable semantic
boundaries rather than compare allocation addresses. This is what will keep a
faithful recreation from becoming a visually plausible but behaviorally
different game.

## Stop conditions for a first faithful vertical slice

The first useful target is not every menu or every trick. It is one Warehouse
Free Skate session with deterministic input, a rendered level, player movement
over the native collision world, one trigger-created object, one gap/goal, and
retail-vs-native traces for the same input sequence. After that milestone,
vehicles, career progression, frontend, audio, and the remaining script
surface can be added without changing the core boundaries.
