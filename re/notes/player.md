# Player

Status: input and raw position boundaries recovered; full skater state pending.

The observed skater object has live position words at `+0x08/+0x0c/+0x10`,
previous-position/history words at `+0xbc/+0xc0/+0xc4`, a collision/platform
response vector at `+0x4c/+0x50/+0x54`, physics-state dispatch at `+0x30b8`,
and a ground-update state at `+0x30c4`. These are raw 32-bit fields; their
fixed-point scale and final type names remain conservative.

`src/runtime/input_state.*` models the confirmed movement mask bits:
Left `0x8000`, Right `0x2000`, Up `0x1000`, Down `0x4000`, plus the observed
active/press-latch/held/inactive/frame counters. Retail `FUN_00489930` updates
each action record as a 16-byte state: active at byte `+0`, first activation
at byte `+1`, active/inactive counters at `+4`/`+8`, and update count at `+c`.
The poller also activates the four movement records from signed analog axes
when the digital bit is absent: left/up at `<= -0x29`, right/down at
`>= 0x29`. The native `InputState` and `LevelFrameScheduler` preserve those
thresholds while retaining the raw action mask.

`LevelFrameScheduler` exposes the recovered poll -> input-history ->
gameplay-tick order, with optional signed axes. `runtime/gameplay_frame.*`
extends that boundary so the same input snapshot reaches TRG and then
`PlayerPhysicsFrame` in one call. `src/runtime/player_state.*`
now preserves the raw player handoff: live position `+0x08/+0x0c/+0x10`,
previous position `+0xbc/+0xc0/+0xc4`, collision response `+0x4c/+0x50/+0x54`,
physics state `+0x30b8`, and ground-update state `+0x30c4`. Its
`begin_physics_frame()` and `commit_position()` methods model only the
confirmed history copy and shared position commit; they do not assign
unrecovered steering or acceleration constants. The raw response vector also
exposes the recovered Q12 projection (`FUN_00490610`) and inward-response
stage of `FUN_0049bad0`; orientation and state-transition stages remain
separate, with the caller-selected orientation stage now bounded independently.

`InputState` also retains four completed semantic frame snapshots, matching the
depth of the action-history copies in retail `FUN_00489cd0`. `ground_turn.*`
and `PlayerState::update_ground_turn()` connect the effective Left/Right
records to the confirmed signed turn accumulator: ordinary and alternate
profiles use the observed `0x3c`/`0x78` bases, Q8 frame scaling, and a
data-driven `0x2d000`/`0x5a000` grounded limit (with `0xa0000` reserved for the
separate state-1/2 branch). Release now applies the confirmed
ordinary `>>2` decay and near-zero quantization, with the separate alternate
`>>1` selector exposed in `GroundTurnConfig`; the analog lean and remaining
board predicates remain data-driven. This is the input-to-turn boundary, not
yet the full state-specific motion producer.

The action-record bank now covers all 16-bit action bits. The recovered KICK
record (`0x0040`) is separate from JUMP (`0x0010`):
`PlayerState::run_ollie_prephysics()` models KICK charge/latch/release,
vertical impulse arithmetic, and launch requests to raw state `1`/`3` with
reasons `0x245c`/`0x2457`. The in-air JUMP hold consumer clears vertical
response after more than two held updates. Shared random/stat and animation
eligibility inputs are still injected at the frame boundary.

The candidate in-air vector at `+0x310c/+0x3110/+0x3114` now has a native
scalar gravity producer. Retail `FUN_00497df0` subtracts `500` from its Y
component until the pre-update value is below `-0xe0c`; the terminal branch
writes `-0x1000` and clears X/Z. `air_motion.*` preserves this exact integer
branch and exposes it through an optional in-air frame hook. Its later basis
transform is now implemented as a separate normalized cross-product stage;
the candidate vector is still not the displacement velocity, so its final
position handoff remains open.

The post-dispatch response-vector damping at retail `0x0049d480` is also
available as `VelocityDamping`: over-limit rescaling, random per-component
decay, and the deterministic `/32` then `/4` low-speed quantization are
implemented with random/mode-table values supplied by the caller.

The opt-in `VelocityProjection` helper reconstructs retail `0x00490680`:
after `FUN_00490610` removes a collision-normal component, it restores the
original Q12 vector magnitude using the same reduced multiply/divide scales.
This is exposed through `PlayerState::project_collision_velocity()` because
the recovered callers select it by collision/state branch rather than applying
it to every hit.

The grounded `0x0049df00` brake branch is now available as `GroundBrake`.
It preserves the slope-derived `0xa000` minimum threshold, the `*0x40`
response-speed metric, Q8 component braking, and the state-7 stop request
with reason `0x2c56`. `PlayerPhysicsFrame` accepts the unresolved surface
eligibility and raw normal/stat inputs through `ground_brake_input`.

The stateful part of the same routine is now separate `GroundPhysics` state.
It keeps the dispatcher `+0x30c4` field distinct from the brake mode at
`+0x2df8`, ports the recovered mode transitions `0/1/3/4/5/6`, cooldown `2`,
and state-7 reset reasons `0x2c0f`/`0x2c21`. `PlayerPhysicsFrame` exposes it
through an opt-in hook; surface/profile and animation-readiness predicates
remain caller-owned until their writers are identified.

`fixed_matrix.*` now covers the next confirmed grounded stage. Retail
`FUN_0049b500` masks the accumulated turn to a signed 12-bit angle, applies a
Q12 yaw transform to the existing nine-short matrix, and `FUN_0049c7d0` copies
its columns into the three basis groups at `+30f4`, `+3100`, and `+310c`.
`PlayerState::update_ground_turn()` mirrors that orientation/basis handoff and
retains the saved pre-frame matrix. For ordinary grounded state 0, the same
retail call's nonzero response phase now transforms `+0x4c/+0x50/+0x54` after
the candidate position add and restores its integer speed metric; the native
regression is the Warehouse sample `(-2066, 0, 192364)`. The matrix-vector
stage preserves retail truncation toward zero separately from the
matrix-matrix arithmetic shift. `PlayerState::prepare_ground_basis_correction()`
and `integrate_motion_correction()` then expose the confirmed basis-to-motion
boundary. `integrate_position()` now implements the shared ground/air
`velocity*dt + correction*dt^2/2` position producer. The actual acceleration,
gravity, horizontal launch response, and state-specific collision branches
remain open. Accepted in-air contact now has an explicit classifier hook that
commits the contact and requests ground state with reason `0x1fd6`.

`runtime/physics_dispatch.*` preserves the raw state dispatcher ordering and
the state-7 previous-position restore. Its stage callbacks are intentionally
address-oriented until the ground, in-air, rail, and special routines are
implemented behind them.

`runtime/physics_frame.*` combines the confirmed portions of the outer physics
frame with those stage callbacks. `runtime/physics_replay.*` can run a fixed
input sequence repeatedly and record live/history/response/state/basis snapshots;
this is the native side of the parity harness, not a claim that the missing
stat-driven motion producers are complete.

`GameplaySession::initialize()` now applies the restart selected by the level
autoexec before the first fixed step, matching the retail front-end load path;
manual restart and KILLBRUCE/gap restart events use the same player boundary.
The player boundary also ports the facing portion of retail
`FUN_004c4e30 -> FUN_004c4d10`: the high word of the restart auxiliary field is
converted to `((word - 0x800) & 0xfff)`, used to rotate the `-X`/`-Z` Q12 seed
vectors, and installed as the negative-identity yaw basis. This keeps restart
orientation separate from the standalone identity used by unit-level math
tests while matching the retail skater initialization convention.

`src/runtime/position_commit.*` models the confirmed shared position commit
fallback, including the PSX hit's fixed-point `0..0x4000` segment parameter
and signed-truncating contact reconstruction. Steering, acceleration, gravity, horizontal launch response, trick,
animation, and camera behavior still require the input-to-skater producer and
per-state physics traces described in
[`re/evidence/functions/physics.md`](../evidence/functions/physics.md).
