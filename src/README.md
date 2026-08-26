# Native reconstruction

Do not choose a graphics/runtime framework yet. The first native slice is a
framework-free physics-state reference model that can be compared with the
Warehouse traces before any renderer or game loop is selected.

`physics_state_machine.hpp/.cpp` intentionally models only the recovered
boundary:

- raw `player+0x30b8` values, `+0x30c0` phase history, and `+0x30c4` auxiliary state;
- the ordinary gameplay state-request writer `0x004902bf`, kept distinct from
  lifecycle, special-action, replay, and network direct writers that also
  touch `player+0x30b8` in the retail executable;
- the complete 12-record action bank, including the configured JUMP record
  and bit `0x0010`, the KICK record at `+0x30`, and the grounded movement
  records Left/Right/Up/Down;
- the separate KICK action record at `+0x30` and mask bit `0x0040`, which the
  controlled retail capture shows reaching the direct ollie charge/latch gate;
- the second grounded leave-ground path after the case-0 helper sequence:
  supplied slope/recovery/frame inputs can request raw state `1` at
  `0x0049ddcf` with reason `0x2ba1`, including the following downward-Y clamp;
- the independent frontend trace's raw collision chain `0 → 2 → 4 → 1 → 0`,
  with request callsites/reasons `0x004972da/0x1ac9`,
  `0x004913dd/0x0b1c`, `0x004905ab/0x0715`, and `0x004991fe/0x1fd6`;
- dispatcher routing, including the raw-2 collision-transient case and its
  auxiliary flag, distinct raw states `1` and `3` sharing the in-air handler,
  raw states `1`, `3`, and `6` reaching the common in-air handler, the
  case-6 pre-air request/setup boundary, and the raw-3 timeout promotion to
  state `1`;
- provisional `EPhysicsState` semantic labels correlated from the bundled PSX
  symbol order, while retaining raw values as the authoritative replay key;
- case-6 velocity shaping from the published orientation basis and explicit
  random draws before its common in-air fallthrough;
- launch and accepted-contact ordering, including raw-3 launch grace and the
  recovered landing bookkeeping fields;
- state requests retain both the exact writer (`0x004902bf`) and, where the
  path is recovered, the invoking call instruction (`0x0049ac9b`,
  `0x0049ac7f`, `0x004991fe`, or `0x0049de9e`);
- the recoverable prephysics charge/latch/release half of `0x0049a280` behind
  explicit KICK-held, stale-latch, and skater/animation eligibility seams;
- the direct in-air jump-held effect: after action-record counter `+4 > 2`,
  vertical velocity and acceleration are zeroed at the recovered retail
  writer boundary;
- the fixed-point in-air position integration from `0x00497f40`, with the
  runtime frame delta supplied by the caller;
- the common in-air gravity accumulation at `0x004992f0`, applied after the
  motion/contact portion so it contributes to the following integration, and
  skipped when the accepted-contact branch jumps over that fallthrough;
- the recoverable in-air action-to-acceleration block from `0x00497f40`, with
  KICK, UP, DOWN, SPINLEFT, and SPINRIGHT axis terms applied in retail order,
  followed by the velocity/basis stabilization subtraction;
- the in-air preparation callback at `0x00497df0`, before those action-control
  terms consume the published orientation basis;
- the deterministic `0x00497df0` rolling-basis update, including the
  `0x00465f60` integer normalization, signed-short `0x004e2ff0` cross products,
  and `0x0049c850` orientation-short publication;
- the deterministic orientation recovery/rebuild at `0x0049d080`, including
  signed-short target interpolation, the two cross products, basis-field
  publication, and recovery-base update;
- the ordinary `0x004e2070` signed-16-bit scratch clamp kept separate from
  the live raw cross-product result;
- the outer-frame acceleration-to-velocity update at `0x0049f206`, before
  postphysics damping;
- the outer `0x0049e680` frame contract and position-history rotation;
- the frame-start gravity publication for `player+0x2dac`, with raw-state-2
  transient randomness and global modifiers supplied explicitly;
- the deterministic post-ground-action acceleration clear for raw states other
  than `1` (`0x0049e996`, `0x0049e999`, `0x0049e99e`);
- the control-blocked post-dispatch reset at `0x0049d8a0`, including its
  horizontal acceleration/launch-latch clears, optional descending-Y clamp,
  and component velocity decay;
- the fixed-point vertical ollie impulse formula from `0x0049a280`, with the
  retail random draws supplied by the caller, including separate cap,
  wallie-cap, and early-release streams; and
- the retail Q12 dot/scalar arithmetic, orientation-short basis permutation,
  frame-start gravity scalar, orientation-relative grounded acceleration tail,
  packed collision-normal decoding/projection (`0x00490610`/`0x00490680`),
  the selected-result ground collision handoff into acceleration/contact
  publication, and postphysics velocity damping (`0x0049d480`) as
  deterministic helpers
  with collision/global/random inputs supplied by the caller; and
- fixed-point position/velocity storage with callback boundaries for impulse,
  orientation-dependent acceleration, collision, and position math; and
- the raw in-air collision-result/material predicate immediately after
  `0x004624d0 -> 0x00466090 -> 0x0048ea80`, exposed as
  `standard_air_landing_accepted()` without pretending to select collision
  geometry; and
- the exact raw-2 collision-transient enter/exit request boundaries from
  `0x004972da` and `0x00497479`; and
- the selected-result in-air collision recovery branch at `0x00497aa0`,
  including its raw-2/raw-1 requests, horizontal velocity correction, and
  launch-recovery bookkeeping; and
- the frame-start `+0x2d8c` recovery-window timer and its signed action/heading
  gate; and
- the composed `accept_standard_air_collision()` boundary, which applies that
  predicate before entering the recovered contact-position/state-request
  ordering; and
- the recovered fixed-point grounded target/brake stage from `0x00493370`,
  exposed as `apply_ground_action_step()`.
- the case-6 dispatch ordering: an optional frame callback supplies the five
  retail random draws to `run_state6_preair_setup()` between the case-6
  dispatch observation and the common in-air action/motion callbacks.
- the deterministic grounded re-entry cleanup from `0x0049a280`: after a
  launch has returned through the phase word, it clears the in-progress/mode
  latch fields (`+0x2ddc`, `+0x2db8`, and `+0x2c68`, and conditionally
  `+0x29c8`) at the recovered retail stores.

The action bank is intentionally separated from the later orientation and
collision paths. Static and runtime evidence identifies `0x00493370` as the
first consumer: it converts directional action records into movement-target
fields, applies target clamps/decay and brake-mode velocity damping, then calls
`0x00492f20`. The native model now implements that deterministic target stage;
the helper's animation/lean side effects remain an explicit seam. The
orientation-dependent acceleration arithmetic is available through
`apply_ground_surface_acceleration()` once collision supplies the basis and
flags. The packed-normal vector transform is available through
`project_velocity_preserving_speed()` once collision supplies the two raw
normal words, and the selected-result field/acceleration handoff through
`apply_ground_collision_handoff()`. The extended `AirContactInput` boundary
also carries a landing identity and packed normal for the post-request
velocity projection, while `apply_velocity_damping()` takes the shared
postphysics random draws explicitly.

It deliberately does not replace raw values with unverified final PC enum
names or reproduce collision selection internals. The native dispatch result
therefore exposes a separate provisional `semantic_state` label correlated
from the PSX `EPhysicsState` order. Grounded target arithmetic and the
supplied-input surface/postphysics/vector-transform/contact-handoff arithmetic
are recovered; collision selection and animation side effects remain
caller-owned.

The selected-result recovery branch is available through
`handle_air_collision_recovery()`. It preserves the `0x00497aa0` raw-2/raw-1
request split and its optional horizontal velocity correction while keeping
the collision result itself explicit.

The JUMP/KICK relationship is likewise kept at the recovered boundary. The
input builder and action updater produce independent records (`+0x00/0x0010`
for JUMP and `+0x30/0x0040` for KICK); the direct ollie gate consumes KICK.
No direct alias is present in the input/update chain, so any later
user-facing gameplay mapping remains an explicit caller decision.

Compile and run the deterministic replay test from the repository root:

```bash
mkdir -p build/native
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -Isrc \
  src/physics_state_machine.cpp tests/physics_state_machine_test.cpp \
  -o build/native/physics_state_machine_test
build/native/physics_state_machine_test
```

`PhysicsStateMachine::step_frame` is the intended integration seam for a
future game loop. It preserves the observed prephysics, ground-preparation,
history-copy, collision-start, dispatcher, contact/commit, and postphysics
ordering. Collision results, orientation-dependent grounded acceleration, and
the unresolved user-facing JUMP-to-KICK/gameplay eligibility mapping remain
callbacks until new traces constrain them. The direct KICK-release-gated
launch boundary is explicit in the model, including the eligible-charge clear
of `+0x3144` at retail writer
`0x0049a669`. The verified `0x00493370` action/velocity stage is exposed as the
first `movement_action_step` callback, after action records update and before
prephysics. Call `apply_ground_action_step()` from that boundary with the
captured frame delta to reproduce target/clamp/decay/brake arithmetic; its
orientation-dependent acceleration and heading/animation helper remain
caller-owned. The air position step is explicit: call
`integrate_in_air_position()` with the captured `DAT_0056865c` frame delta before
supplying collision/contact results; `step_frame()` exposes this point as the
optional `air_motion` callback, after applying the recovered jump-held
vertical effect. For raw state 6, provide `state6_preair_setup` to execute
`0x004993f0` in its recovered position; the callback owns the random draws and
the following air callbacks observe the requested raw state 1. Set
`gravity_initialization` when the frame owns gravity publication; it runs before
`movement_action_step` and keeps the retail global modifiers/random draw
caller-owned. Accepted air contact also preserves the retail branch around
`0x004992f0`: the landing frame skips the common gravity-add fallthrough.
Set `air_preparation` when the caller owns the `0x00497df0` basis/global setup;
it runs after any case-6 setup and before the native air-action terms. The
callback can call `prepare_in_air_orientation()` for the recovered rolling
basis/normalization/cross-product path; the retail `0x004e2070` angle/clamp
side effect's exceptional/shared-global state remains caller-owned. The
ground/collision recovery boundary is available through
`apply_orientation_recovery()` once the caller has populated the signed-short
target and recovery progress/base fields.
Set `upright_correction` after `air_motion` when reproducing the common
handler's `0x0049c330` step. The callback supplies the shared global up vector
to `apply_upright_correction()`, which preserves the raw-state/auxiliary gate,
the ±`0x29` cross-dot thresholds, the retail ±`0x000b`/wrapped `0x0ff5` turn,
and the exact Q12 short-matrix publication.
Set `velocity_integration` to call `integrate_velocity()` with the captured
`DAT_0056865c` delta before postphysics damping.
Set `blocked_physics_reset` after air contact/gravity when reproducing the
control-blocked `0x0049d8a0` boundary; call `apply_blocked_physics_reset()`
with the captured decay divisor and clamp gate before velocity integration.
