# C++ recreation roadmap

Status: level-side native slice established; faithful Warehouse Free Skate
vertical slice pending.

The current C++ work has crossed the asset/runtime boundary. The remaining
work should be treated as independently testable services, with the retail
executable used as the behavioral oracle wherever names or constants are
still uncertain.

## Already usable

- Bounded native readers for TRG, PSX, and PRE data.
- TRG node/script representation, command cursor, dispatcher, timers,
  resources, restarts, goals, gaps, and deterministic level state.
- PSX scene object/model binding and lazy factory-asset resolution.
- Native PSX palette/texture expansion for 4bpp, 8bpp, and 16bpp payloads.
- PSX blockmap-selected collision geometry with fixed-point world placement.
- Retail-shaped segment-query metadata: hit face/object, hit point, normal,
  surface flags, nearest travel distance, and the `0..0x4000` fixed-point
  segment parameter used to reconstruct the contact point.
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
- A `GameplayFrame` coordinator that carries one input snapshot through the
  level/TRG tick and the player physics boundary in the recovered outer order,
  while passing the caller's fixed-point frame scale to both timing domains.
- A renderer-independent `LevelRenderSnapshot` at the recovered object/model
  submission boundary: scene entity state, PSX model faces, raw local vertices,
  normals, UVs, texture references, and object placement are available to a
  backend without coupling it to TRG offsets.
- A configurable deterministic `FixedStepDriver` around `GameplayFrame`, with
  partial-time accumulation, bounded catch-up, reported dropped time, and the
  keyboard-buffer input path. Its step interval and frame scale remain
  configuration seams until the retail timer conversion is fully isolated.
- A `GameplaySession` shell that owns `LevelRuntime`, `PlayerState`,
  `GameplayFrame`, and `FixedStepDriver`; its default physics hook queries the
  loaded PSX collision world and exposes the recovered scalar air-gravity and
  optional collision-response stages through one native entry point.
- `GameplayFrameResult` now carries the exact trigger-event delta appended by
  the level tick, preserving source node, target, opcode, value, and checksum
  at the fixed frame where a script effect occurs. This is the join needed for
  retail/native event traces; the cumulative `GameplaySessionObservation`
  counts remain available for long-lived state checks.
- TRG restart execution now crosses the complete level-to-player boundary:
  `GameplaySession::execute_restart()` dispatches the named restart stream,
  applies its fixed-point position, preserves the two auxiliary restart fields,
  clears transient motion, and resets the fixed-step clock.
- `GameplaySession::pulse_node()` and `pulse_checksum()` expose the recovered
  TRG event boundary to gameplay code and synchronize script mutations back to
  the scene registry.
- `GameplaySession::observation()` provides a stable end-to-end parity record
  for one frame: input/physics result, level time and trigger counts, player
  fixed-point vectors/orientation, restart fields, and scene counts.

These pieces are renderer-independent and are covered by native regression
tests. They are not yet a complete game executable.

## What still blocks faithful recreation

The work is now separated into three kinds of missing behavior:

1. **Required for a faithful first playable level:**

   - the remaining animation/local-profile/stat writers feeding the
     now-recovered `0x0049b010` ground-motion producer (the sixteen raw
     `+0x2ccc` action-profile slots, analog thresholds, turn flags, and
     transient response-normalization boundary are now represented natively);
   - complete contact filtering and response policy, including face masks,
     back-face/trigger rules, sweep/radius behavior, slope classification,
     rails, platforms, bounce, and landing/wall transitions;
   - the remaining state handlers for grounded rolling, airborne control and
     upright orientation, braking, launch velocity, facing, animation
     eligibility, and rail/special transitions; and
   - a camera and renderer backend, because a native physics/trigger slice is
     not a playable recreation until the level and player can be presented.

2. **Required for behavioral completeness after that slice:**

   - object factories and live object behavior for the TRG/PSX records;
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
order in which the queue is drained before `0x004be450` seeds or waits for the
next frame. It is still not proven to be the ordinary keyboard steering path,
and the `0x004e85a0` basis transform is kept as an explicit callback until its
coordinate convention is checked. Do not promote this special action command
into generic keyboard acceleration.

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
   represented in `ground_animation.*`, while animation asset/event dispatch,
   local-profile selection and B010's random/cooldown rearm side effects
   still need their real writers. The post-dispatch `+0x2dc8` threshold
   decay/reseed and the retail dot/sqrt speed metric are now native services;
   the RNG roll and special-state predicate remain injected. The grounded
   `0x0049f0e5`
   call remains a collision-safe commit, not the motion producer. The
   remaining state-specific motion work is gravity/stat tables, horizontal
   launch velocity, animation eligibility, trick/rail transitions, facing,
   and the remaining stat-driven damping mode selection plus the rest of the
   `0x0049df00` brake-mode state machine;
   `PlayerPhysicsFrame` now provides the call boundary for those producers.
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
Remaining content work is to classify unknown opcodes from retail traces,
complete object factories and special vehicles, resolve goal/career/two-player
state transitions, and bind level events to visible/audio/gameplay services.
Each addition should have one source record, one native state mutation, and
one replay fixture before it is generalized.

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
