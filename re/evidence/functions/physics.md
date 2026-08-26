# Skater physics dispatch and ground/in-air update

Status: confirmed PSX collision-query to skater-response handoff, input-to-steering/target producer, shared position-commit path, and state-gated acceleration writes; physics-state semantics remain partial
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x00489930`, `0x00489a10`, `0x00492ed0`, `0x00492f20`, `0x00493370`, `0x00496060`, `0x0049b010`, `0x0049bad0`, `0x0049db80`, `0x00497f40`, `0x0049e97d`, `0x0049f0e5`, `0x004ca9f0`, `0x004cabf0`, `0x004624d0`, `0x00466090`, `0x0048ea80`

The bounded state-machine record is maintained in
[`physics-states.md`](physics-states.md). This note retains the earlier
ground/in-air and position-commit analysis; the newer record is authoritative
for the reproducible `0 → 1 → 0` transition and the action-state timing.

## Observation

- `0x0049db80` dispatches on the object field at `param_1 + 0x30b8`, with cases that call `0x00496550`, `0x00497f40`, `0x00494210`, `0x00499710`, and `0x004993f0`.
- The dispatcher decompilation separates the state paths: state `0` calls `0x0049dad0`, `0x00496550`, `0x00495cc0`, and `0x0049d9c0`; state `2` calls `0x00496550`; states `3` and `6` enter `0x00497f40`; state `4` calls `0x00494210`; state `5` calls `0x00499710`; and state `8` calls `0x004995d0`. This makes `0x00496550` a ground/collision routine, not the input-to-steering producer.
- After the case-0 helper sequence, the dispatcher has a separate grounded leave-ground predicate: raw state must still be `0`, `player+0x3110 > 1000`, and either `player+0x3130 > 0x5000` or the signed frame age from `+0x2d98` must be less than `(DAT_0056865c * 6) >> 8`. It requests raw state `1` at `0x0049ddcf` with reason `0x2ba1`, then clamps negative `player+0x50` at `0x0049ddd9`. The native `try_ground_to_air()` method models this supplied-input boundary separately from the KICK/ollie request.
- An independent frontend trace adds an exact non-ollie collision chain: `0 → 2` at `0x004972da` / reason `0x1ac9`, `2 → 4` at `0x004913dd` / reason `0x0b1c`, `4 → 1` at `0x004905ab` / reason `0x0715`, and `1 → 0` at `0x004991fe` / reason `0x1fd6`. The dispatcher handlers for the middle states are `0x00496550`, `0x00494210`, and `0x00497f40`; all requests commit through writer `0x004902bf`. The native model preserves these raw request boundaries without implementing the state-4 rail/collision geometry body.
- The same dispatcher reads/writes state fields around `param_1 + 0x30c4`, clears or updates velocity-related fields, and resets position from the object offsets `+0xbc`, `+0xc0`, and `+0xc4` in one state.
- `0x00497f40` references `physics.cpp` diagnostics `P_O_I in DoPhysicsInAir` and `mOldPos wrong at start of DoInAirPhysics`. Runtime entries independently confirm it is reached for raw state `1`; the dispatcher also routes distinct raw state `3` to the same callee. It reads the skater object position at `+8`, `+0xc`, and `+0x10`, performs collision/ground calculations, and writes updated movement state.
- The source symbol dump contains the matching concepts `DoInAirPhysics`, `DoOnGroundPhysics`, `DoOnRailPhysics`, and `EPhysicsState`, but those names are treated as cross-build hypotheses until runtime mapping confirms them.
- A one-shot Warehouse write watch on the first `+0xbc` word fired at debugger PC `0x0049ea81` (`Skater_PhysicsDispatcher+0xf01`) for player `0x05f39530`. The preceding store at `0x0049ea7f` writes the first word of the `+0xbc` vector.
- Static disassembly around that store copies the three words at `player + 0x08`, `+0x0c`, and `+0x10` into `player + 0xbc`, `+0xc0`, and `+0xc4`. This is the first concrete movement-state handoff: the `+0xbc` vector is a previous-position/state-history candidate, not yet confirmed as velocity.
- A three-watch Warehouse run watched `player + 0x08`, `+0x0c`, and `+0x10` simultaneously, using three of the four x86 hardware slots. It captured 621 writes before the WineDbg proxy failed while a bounded watch was being removed; the trace is `build/debug/sessions/pos-writes-auto2/xyz.trace.ndjson`.
- The same writes repeatedly trap after the contiguous stores in `0x00496060`: `0x00496257` writes Y, `0x00496260` writes X, and `0x00496263` writes Z. This is the strongest current candidate for a position commit path because all three components are written to the skater object in one routine.
- Other confirmed writers include the vector-add helper at `0x004ca9f0` (post-store PCs `0x004ca9ff`, `0x004caa0d`, and `0x004caa16`) and a physics-dispatch path at `0x0049f0cc`, `0x0049f0d4`, and `0x0049f0e2`. The post-store PC is one instruction beyond the actual write, as expected for x86 debug traps.
- Static callers of `0x00496060` include the in-air candidate `0x00497f40` at `0x00498cf5`, `0x0049905c`, and `0x0049917b`, the dispatcher path at `0x0049f0e5`, and additional state routines at `0x00496550` and `0x00499710`. The common call shape loads three scalar arguments and invokes the routine with the player object in `ecx`.
- Static disassembly resolves the grounded dispatcher handoff. At `0x0049ea68`, `ebx` is set to `player + 0x08`. The sequence at `0x0049f0be`–`0x0049f0e5` gates on the global at `0x0056a86c`, `player + 0x2e90`, and physics state `8`; loads the updated `player + 0x08`, `+0x0c`, and `+0x10` words into `eax`, `ecx`, and `edx`; copies the history words at `player + 0xbc`, `+0xc0`, and `+0xc4` back into the live position; then pushes `edx`, `ecx`, and `eax` before calling `0x00496060` with `ecx=player`. The callee writes its three entry arguments back to `player + 0x08`, `+0x0c`, and `+0x10`, confirming the handoff order as X, Y, Z.
- The decompilation of `0x00496550` shows the ground/collision path deriving short collision deltas from `0x004cabf0`, passing them to `0x0049bad0`, applying surface-normal checks, and damping the three words at `player + 0x4c`, `+0x50`, and `+0x54`. It is a collision-response/state-resolution routine; no confirmed Left/Right action-state read occurs in this path.
- The ground path's asset-facing query boundary is explicit. It builds a six-word fixed-point segment/AABB candidate, initializes it with `0x004624d0`, submits it through `0x00466090` (a direct wrapper over the PSX zone broadphase), and then calls `0x0048ea80` to read selected-face flags before applying the result. The query initializer clears the hit object at `+0x68`, hit face at `+0x80`, model index at `+0x84`, and hit-distance/output words at `+0x40` and `+0x8c`; `0x00462a20` fills those outputs when a PSX face intersects.
- `0x0048ea80` independently reads the selected runtime face record: it extracts bit `0x40` from face `+0x0c`, two high-bit booleans from that word, a four-bit field from bits `25..28`, and bit `0x80` from the face's packed word at `+0x00`. These are recorded as surface/collision flags without assigning gameplay names. This proves the PSX face result crosses from the static blockmap consumer into physics-state branching.
- `0x0049bad0` then consumes a short collision vector and updates both the response vector at `+0x4c/+0x50/+0x54` and the player-relative source/basis fields at `+0x30f4` onward. It rebuilds the three orientation/basis triplets at `+0x2e58..+0x2e68` through the shared fixed-point basis helper. The exact surface response enum remains open, but the asset-query → selected-face flags → skater response sequence is confirmed.
- The first proven action-dependent movement producer is `0x00493370`, reached from the main skater frame by the call at `0x0049e97d` (return address `0x0049e982`). Static disassembly shows it reading the player-relative action bank pointer at `+0x2ccc`, directional records at `+0x80`/`+0x90`, and heading input at `+0x31a1`/`+0x31a2`; it writes movement targets at `+0x3144`/`+0x3148`, steering state at `+0x2e7c`, and brake state at `+0x2e78`.
- The immediate post-action path in the main frame clears `player +0x58/+0x5c/+0x60` at `0x0049e99e`, then calls `0x0049b010` at `0x0049e9a1`. Static disassembly of `0x0049b010` reads the action bank, brake/steering fields, physics state, and a player-relative source vector at `+0x30f4/+0x30f8/+0x30fc`; its normal-state branches write the three acceleration words at `+0x58/+0x5c/+0x60`. This is the first statically proven post-action acceleration writer, although the source vector's final gameplay meaning remains open.
- The acceleration write policy is now exact. For animation/frame states `+0xf6 == 2` or `3`, frames `+0xf4` in `(10, 16)`, and non-state-2 physics, `0x0049b010` writes `-source_vector * 4`, or `-source_vector * 8` when the action-bank flag at `+0x10` is set, subject to the response-speed threshold. For `+0xf6 == 0` or `5..7`, it writes `-source_vector` under the normal distance/vertical-speed gate; for `+0xf6 == 0x5e` and frames `16..19`, it writes `-source_vector * 8`. The same scale branches appear in both sides of the helper's steering/action gate. These are stable fixed-point recreation rules even though the public names of the source vector and state values remain open.
- `0x00493370` calls `0x00492f20` after updating the movement targets. `0x00492f20` reads the Z target at `+0x3148`, heading deadband at `+0x31a2`, action-bank state, and mode fields, then interpolates a u16 player field at `+0xf4` through `0x00492ed0`. The helper's exact interpolation is now recovered: it moves toward the target by 5 units when the remaining difference exceeds 12, by 3 units when it exceeds 3, and by 1 unit otherwise, preserving the sign.
- The controlled trace `build/debug/sessions/movement-step2/movement.trace.ndjson` records `0x00493370` once per frame for the live Warehouse player. Around the isolated Left hold, frame 620 has action mask `0x8000` and the Left record at `0x0056b078` becomes active; on frame 621 the target pair becomes `-0x7800/-0x7800` and `steering_active=1`. It ramps by `0x7800` per frame to `-0x2d000`, then after release decays `-0x2d000 -> -0x21c00 -> -0x19500`, with steering inactive. This is the first direct runtime edge from an input action record to steering/target state.
- The same trace records velocity from `player +0x4c/+0x50/+0x54` and acceleration from `player +0x58/+0x5c/+0x60` at the action step. The function also contains static stores to the acceleration words in its brake/response branch, so these are movement-state snapshots rather than a claim that every acceleration update is owned by one branch.
- The collision query chain is now constrained by static decompilation. `0x004624d0` prepares a fixed-point segment query and endpoint bounds; `0x00466090` wraps the blockmap walker at `0x004660b0`; `0x004638d0` expands blockmap object lists into face bounds; and `0x00462a20` tests candidate triangle/quad faces. A hit is written to the query at offsets `+0x68` (object/hit marker), `+0x6c/+0x70/+0x74` (fixed-point hit position), `+0x80` (face pointer), `+0x84` (object index), `+0x8c` (nearest travel value), and `+0x40` (scaled travel value), while the source face normal is published through the collision globals. `0x00463d50` then copies the normal/orientation result into the query result fields. The native `PsxCollisionWorld`/`PsxPositionCollisionProbe` implements the portable geometry side of this boundary; exact face filters and skater response remain open.
- `0x00489930` is the action-record updater called by `0x00489a10`. Each record is 16 bytes: byte `+0` is active, byte `+1` records that activation has occurred, words at `+4` and `+8` count active and inactive updates, and the word at `+0xc` counts total updates. `0x00489a10` maps movement bits `0x8000/0x2000/0x1000/0x4000` to left/right/up/down and enables a digital action from the signed analog axis at thresholds `<= -0x29` or `>= 0x29` when its bit is absent. Native `InputState` now preserves this effective-action behavior, while `PlayerState` provides the raw position/history/state handoff consumed by later physics handlers.
- The same `0x00489a10` loop writes the normalized controller bytes into the first player's input record at `0x0056b140..0x0056b143` (the `+0x148..+0x14b` fields relative to `0x0056aff8`), after `0x00489990` maps the raw device range. `0x0046d6e0` copies the first two bytes of that record to skater `+0x31a2/+0x31a1`, with the observed two-player sign inversion on the lean byte. The native fixed-step/session shell now has a direct action-plus-axis path so this transfer is not lost when the frontend uses a gamepad or a replay fixture; no acceleration is synthesized at this boundary.
- `0x00490610` is an exact reusable response primitive: it forms a Q12 dot product between the response vector and a collision delta, then subtracts `delta * dot` componentwise. The first stage of `0x0049bad0` uses the same Q12 helpers when the dot is negative, then adds `delta * 0xcd`; its later orientation rewrite is now ported as a separate caller-selected stage. Native `fixed_math.*`, `collision_response.*`, and `PlayerState::apply_collision_response()` cover the response arithmetic, while `PlayerState::apply_collision_orientation()` preserves the recovered orientation/basis sequence without claiming a universal caller policy.
- `0x00493370` consumes the grounded controller records at `player+0x2ccc+0x80/+0x90` and updates the signed turn accumulator at `+0x3144`; Left takes priority if both records are active, and the result is mirrored to `+0x3148`. The ordinary state-0/7 base is `0x3c`, the profile-1 base is `0x78` (other nonzero profiles select `0xb4`), the increment is frame-scaled by `DAT_0056865c`, and the ordinary state-0/7 limit is `+/-0x2d000`. A high-speed surface/board branch can double the step and use `+/-0x5a000`; the separate state-1/2 branch uses `+/-0xa0000`. Native `ground_turn.*` and `PlayerState::update_ground_turn()` model the bounded input-to-turn handoff and the optional signed `+31a1` lean target, then feed the recovered Q12 yaw/basis path.
- The no-record branch of `0x00493370` is also confirmed for the ordinary profile: `+3144` decays by its arithmetic `>>2`, with a small remainder quantized to zero; the alternate/surface path uses `>>1`. Native `GroundTurn::update()` preserves this release behavior and accepts the recovered signed lean field (`+31a1`) as an explicit configuration input; board/surface predicates remain outside the default boundary.
- The angle-table path is `0x0049b500` -> `0x004e80e0` -> `0x004e7de0`/`0x004e3130` -> `0x0049c7d0`. `0x0049b500` masks the turn angle to `0xfff`, stores it as a signed short, rotates the existing nine-short matrix at `+0x2e58..+0x2e68`, then `0x0049c7d0` copies matrix columns to `+0x30f4..+0x3114`: `[2,5,8]`, `[0,3,6]`, and `[1,4,7]`. The yaw transform is `[cos,0,-sin; 0,0x1000,0; sin,0,cos]`; `0x004e3130` multiplies signed products and uses `sar 12`. Retail converts the signed angle with float constants `0x39800000` (`1/4096`) and `0x40c90fdb` (`2*pi`) and truncates the Q12 sine/cosine through `0x005004f4`. Native `fixed_matrix.*` and `PlayerState` preserve these signs, shifts, signed-angle behavior, and offset-oriented basis copy.
- The grounded basis consumer is the tail of `0x00496550`: it computes `dot(+4c,+3100)`, reconstructs that basis component with Q12 multiplies, and subtracts it from the temporary correction vector at `+0x58/+0x5c/+0x60`. A separate branch computes `dot(+4c,+30f4)`, scales its reconstructed component by the observed factor `8`, and subtracts it when the surface/ownership condition permits. The outer `0x0049e680` frame later applies the temporary correction back to `+4c/+50/+54` through `DAT_0056865c` and an arithmetic `sar 8`. Native `PlayerState::prepare_ground_basis_correction()` and `integrate_motion_correction()` implement these confirmed operations while leaving the branch predicate data-driven.
- Both `0x00496550` and `Skater_DoPhysicsInAir` begin their state-specific collision work with the same helper chain: multiply `+58/+5c/+60` by `DAT_00568804`, arithmetic-shift by eight, divide by two, multiply `+4c/+50/+54` by `DAT_0056865c`, arithmetic-shift by eight, add the two vectors, and add the result to live position. Static initialization computes `DAT_00568804 = DAT_0056865c * DAT_0056865c >> 8`, making this `position += velocity*dt + correction*dt^2/2` in the retail fixed-point representation. Native `PlayerState::integrate_position()` now preserves that shared ground/air position producer.
- `0x00493370` contains a second, earlier position-writing path for a queued three-component impulse at `player + 0x2ca0/+0x2ca4/+0x2ca8`. It drains those values using the per-axis rate words at `+0x2c94/+0x2c98/+0x2c9c` and the frame scale, transforms the drained vector through the current orientation, and adds the result directly to live `+0x08/+0x0c/+0x10` before ground collision. The producer is now identified as opcode `0x2b` in `0x004be450`, the action-stream interpreter called from `0x00492ea0` after the queued drain. That case reads signed 16-bit `axis`, `amount`, and `rate` values through `0x004be3c0`, writes `amount` to `+0x2ca0 + axis*4` and `rate` to `+0x2c94 + axis*4`, and when `rate == 0` clears the matching pending and accumulated words at `+0x2ca0` and `+0x2cac`. The native `action_commands.*` and `queued_motion.*` implement this verified producer and scalar drain. The `0x004e85a0` basis transform and direct live-position application remain an explicit callback because its coordinate convention is not yet fully checked.
- The neighboring opcode `0x2c` at `0x004bedc2` is a verified queue barrier. It checks the three pending `+0x2ca0` words and, when any is nonzero, reaches `0x004bf544`, decrements the action-stream cursor by one byte, and returns. When all three are zero it falls through to the next command. Native `kWaitQueuedMotionOpcode` preserves this yield/retry behavior, which is necessary for action records that schedule motion and then wait for it to finish.
- The helper `0x004e85a0`, used by the queue's later orientation/pivot path, is a row-major 3x3 signed-short matrix multiply: each output is the sum of one matrix row times the three input components, followed by the retail arithmetic `sar 12`. Native `q12_transform_vector()` now covers this reusable primitive. The surrounding `0x004e8460` translation-slot copies and `0x004e80e0`/`0x004e3130` pivot rotation are intentionally still separate from the queue callback.
- The native `PhysicsDispatcher` now preserves the confirmed `0x0049db80` state routing and stage ordering: state `0/7` ground preparation -> collision -> post -> final, state `7` restoring live position from the previous-position vector between post and final; states `1/3` in-air, `2` ground collision with `+0x30c4=1`, `4/5/8` their raw special branches with `+0x30c4=0`, and state `6` special branch followed by in-air. Callbacks supply the still-unrecovered gameplay bodies.
- The action-record bank now mirrors the recovered 16-byte records for all
  16-bit action bits. `InputState::action(0x0040)` is the KICK record used by
  the confirmed `0x0049a280` charge/release path; it is intentionally distinct
  from JUMP `0x0010`, which the in-air handler consumes only for the held
  vertical cancellation after more than two updates.
- `PlayerState::run_ollie_prephysics()` now executes the confirmed KICK path:
  blocked frames return first; held KICK increments `+0x2de8`, arms the latch
  and pending fields in grounded/special states; release clears pending,
  rejects stale/cancelled latches, applies the recovered vertical impulse, and
  requests raw state `3` with reason `0x2457` for the ordinary grounded mode or
  raw state `1` with reason `0x245c` for the other launch path. The shared RNG,
  animation gate, and horizontal/stat inputs remain explicit seams.
- `compute_ollie_vertical_impulse()` ports the integer low/high-slope branches,
  speed/height adjustment, and raw-state-5 wallie offset from `0x0049a280`.
  `PlayerPhysicsFrame` exposes this as an opt-in `ollie_input` hook so callers
  can supply the shared retail random/stat stream without inventing one.
- The in-air frame now exposes an explicit contact-classification hook. When a
  caller accepts a hit as ground, `PlayerState::accept_air_contact()` commits
  the contact position and requests raw state `0` with reason `0x1fd6`; the
  hook is not automatic, so walls/rails do not become landings merely because
  the collision query reported a hit.
- The in-air handler also calls `0x00497df0` before its later orientation and
  collision work when the mode flag permits it. That helper updates the
  candidate vector at `+0x310c/+0x3110/+0x3114`: if its Y component is below
  `-0xe0c`, it writes `Y=-0x1000` and clears X/Z; otherwise it subtracts
  exactly `500` from Y. Native `air_motion.*` and
  `PlayerState::apply_air_gravity()` preserve that comparison and update
  ordering. The subsequent shared basis/orientation transform is now bounded
  too: normalize `+310c/+3110/+3114`, form `at_3100 = normalize(at_30f4 x
  air)`, then `at_30f4 = air x at_3100`, and copy `[at_3100, air, at_30f4]`
  back through `FUN_0049c850`. The movement handoff from this direction into
  player displacement remains an explicit seam.
- The same in-air disassembly now gives one safe action-to-motion producer.
  When the controller record at `player + 0x2ccc + 0xa0` (Up, action mask
  `0x1000`) is active, `0x00497f40` reads the current forward basis at
  `+0x30f4/+0x30f8/+0x30fc`, computes `(+0x2dac * 0x96) / 100`, multiplies
  the three basis components by that scalar, applies arithmetic `sar 12`, and
  subtracts the result from the temporary correction at `+0x58/+0x5c/+0x60`.
  The Down record at `+0xb0` (`0x4000`) repeats the same operation but adds the
  vector; both active records therefore cancel in execution order. Native
  `AirDirectionInput` ports this exact fixed-point correction as an opt-in
  hook. The `0x0049e680` `+0x2dac` writer is also recovered separately: it
  starts from the signed stat at `+0x29f4` and computes `stat * 13000 / 100`;
  state `2` applies `((500 - FUN_0048f3a0(1)) * scalar * 0x14) / 10000`,
  followed by the `0xc`/`0xe` mode modifiers (`150%`/`50%`) and the global
  `50%/200%` slowdown. Native `compute_air_speed_scalar()` preserves that
  order while keeping the RNG result and mode flags as service inputs. The
  unrelated ground-motion source remains unresolved.
- Retail `0x0049d480` runs after the outer correction is folded back into the
  response vector. It can rescale an over-limit vector toward a random target,
  apply per-component random decay, then apply deterministic `/32` and `/4`
  small-speed decay when the mode table allows it. `VelocityDamping` ports
  those fixed-point operations; random draws and the mode-table result are
  explicit inputs, and `PlayerPhysicsFrame` can invoke it through an optional
  post-dispatch hook.
- Retail `0x0049df00` contains a separate grounded brake/stop branch. With the
  surface/state predicates satisfied, it derives
  `threshold = max(0xa000, ((0x1000 - max(0x300, (-normal_y * 0x1000) >> 12)) * 0x1800) / 0xd0)`,
  compares that with `speed_metric = magnitude(response) * 0x40`, and either
  damps each response component or clears the vector and requests state `7`
  with reason `0x2c56`. `GroundBrake` and the optional
  `PlayerPhysicsFrameHooks::ground_brake_input` expose this proven behavior
  while keeping material eligibility and raw normal/stat fields injected.
- Retail `0x00490680` copies a response vector, applies the `0x00490610`
  normal projection, computes the original and projected magnitudes, and
  rescales the projected vector by `(original_magnitude >> 2) /
  (projected_magnitude >> 2)` when the reduced projected magnitude is positive.
  Native `velocity_projection.*` and `PlayerState::project_collision_velocity()`
  preserve that integer operation as an opt-in helper; the state-specific
  callers still decide when it is active.
- The shared `0x0049bad0` response producer is now split at its confirmed
  boundary. Its first stage computes `dot(response, surface_delta)` and, for
  a negative dot, subtracts that projected component and adds the literal
  Q12 bias `0xcd` along the same delta. Native `PlayerState::apply_collision_response()`
  implements that arithmetic. `PlayerPhysicsFrame` can invoke it through
  `collision_response_bias_q12` for a caller-selected stage/hit; it remains
  opt-in because retail ground, in-air, and special-state callers do not all
  use the same post-hit policy. Its later orientation rewrite is also available
  as a separate caller-selected stage: it aligns the current basis to the
  inward hit delta, applies the recovered yaw-angle branch, then rebuilds the
  normalized air basis. The exact yaw supplied by each retail caller remains
  open.
- `PlayerPhysicsFrame::step()` is the executable native boundary for the confirmed portion of `0x0049e680`: it captures the previous position, drains queued motion, applies the bounded `+3144/+3148` turn handoff only for grounded states `0` and `7`, updates the state/frame animation boundary, clears the temporary correction at the retail post-turn/pre-B010 point, runs the ground-motion hook, dispatches the state path, applies the shared position integrator at ground-collision/in-air stage entry, sends the desired point through the recovered `0x00496060` axis fallback, and applies the outer correction-to-velocity Q8 handoff after dispatch. The collision probe and branch-specific producers remain injectable because exact face filters, gravity/stat tables, and special-state responses are not yet proven. `physics_frame_test` covers a blocked integrated move and a ground correction producer end to end.
- The hit-aware frame path now accepts a start/end collision query, reuses it for every `0x00496060` axis-fallback candidate, preserves the first hit's face/material/normal metadata, and applies the confirmed `0x00490610` normal-component removal to `+4c/+50/+54` before invoking an optional state-specific collision callback. `PsxPositionCollisionProbe::query()` widens PSX signed-short Q12 normals into the runtime fixed-point vector without coupling `PlayerState` to the asset parser. The inward `0x0049bad0` bias and orientation stages remain opt-in/branch-specific; the independent `0x00497df0` air-basis handoff is now implemented.
- The native hit record also carries the retail-style `0..0x4000` segment parameter (the `q+0x8c` field in the recovered collision path). The PSX adapter computes contact coordinates with signed fixed-point truncation from that parameter, rather than host floating-point rounding.
- The retail face-cache builder at `0x004638d0` stores each model vertex as `vertex * 0x1000 + object_position`; the native PSX collision world now uses this same scale. The packed source face word at face `+0x0c` is also preserved through the hit record for the later `0x0048ea80`/face-mask policy.
- The face-mask reads around `0x0048ea80` are now represented without
  semantic overreach: `(face_word >> 16) & 0x40`, inverse bit `23`, inverse
  bit `24`, `(face_word >> 25) & 0xf`, and `face_flags & 0x80`. Native
  `PsxCollisionMaskView` and `PositionCollisionHit` carry those predicates so
  later state handlers can reproduce the observed branches directly.
- `0x0049f4c0` is a separate producer for the `+0x4c/+0x50/+0x54` vector. Its decompilation contains `platform.cpp` and `unknown bouncy platform type` diagnostics, switches on a platform type, and writes platform/bounce response values into those three fields. Static callers pass platform/object vectors rather than the keyboard action records. The `+0x4c` vector is therefore collision/platform response state, not the direct steering vector.
- The confirmed `0x0049f4c0` writes are now executable in `PlatformResponse::bouncy_velocity()` and `PlayerState::apply_bouncy_platform_response()`: types `2/4/5` use `(source >> 1) + source` for X/Z and `Y=-0x50000`; types `1/3` preserve X/Z and use `Y=-0x20000` below source magnitude `0x28`, otherwise `Y=-0x40000`; unknown types preserve X/Z and use `Y=-0x40000`. Object flags, audio, random orientation, and platform discovery remain outside this pure producer.
- The position probe now models these as caller-side callsites: at the breakpoint on the `call` instruction, the pushed arguments are at `ESP`, `ESP+4`, and `ESP+8`; a callee-entry argument helper must start at `ESP+4` only after the return address exists. The earlier probe implementation skipped the first pushed word and recorded `ESP+4`, `ESP+8`, and `ESP+12` instead.
- The repeatable `tony-position-commit 16` probe captured 16 stationary dispatcher calls with `action_mask=0` and `physics_state=0`, followed by calls through `0x0049917b` (`Skater_DoPhysicsInAir+0x123b`) after Left input. A correlated event recorded `action_mask=0x8000`, held key `203` (Left), and `physics_state=1`; its historical `argument_values` were collected before the callsite correction and are not actual X/Y/Z arguments. The complete trace is `build/debug/sessions/position-probe3/position.trace.ndjson`.
- A controlled Warehouse Left/Right comparison reproduced the input-to-commit handoff in two fresh isolated sessions. The Left trace (`build/debug/sessions/lr-compare4/lr.trace.ndjson`) recorded the action edge at `0x0056b078`, then an `in-air-position-3` call at `0x0049917b` with `action_mask=0x8000`, held key `203`, and `physics_state=1`. The Right trace (`build/debug/sessions/lr-compare6/lr.trace.ndjson`) recorded the action edge at `0x0056b088`, then repeated `physics-dispatch-position` calls at `0x0049f0e5` with `action_mask=0x2000`, held key `205`, and `physics_state=0`. These traces predate the callsite-argument correction, so their recorded `argument_values` are retained only as shifted stack samples, not as the actual callee arguments.
- The side-by-side result confirms the action-state mapping `Left -> 0x8000 / DirectInput 203` and `Right -> 0x2000 / DirectInput 205`. Both feed the shared `0x00496060` position commit routine, but these captures entered different state-specific callers: Left while airborne and Right through the dispatcher while grounded. The caller difference is therefore a state difference, not evidence of separate Left/Right position writers.
- A same-state ground comparison (`build/debug/sessions/ground-compare2/ground.trace.ndjson`) captured 31 Left and 30 Right position commits through the same `physics-dispatch-position` callsite `0x0049f0e5`, all with `physics_state=0`. Its action-state and callsite correlation remains valid, but the stored argument fields were collected before the callsite-argument correction and therefore represent `ESP+4`, `ESP+8`, and `ESP+12`, not X, Y, and Z. A replacement ground comparison should use the corrected probe before drawing axis-level conclusions.
- The corrected headless comparison (`build/debug/sessions/axis-compare4/axis.trace.ndjson`) captured 60 Left and 60 Right commits through `0x0049f0e5`, all with `physics_state=0`; Left held DirectInput key `203` and action mask `0x8000`, while Right held key `205` and mask `0x2000`. The actual callsite arguments were now recorded in X/Y/Z order. Left changed from `0x01a4ff50, 0x0006dd2b, 0x0273b78d` to `0x0194bf45, 0x00026520, 0x023a0a3e`; Right changed from `0x01e10507, 0x000fb62f, 0x025dcceb` to `0x01f28548, 0x0004fa9b, 0x021edc5d`. Both directional phases therefore alter the horizontal X/Z candidate components while remaining on the same grounded dispatcher path. The debugger was stopped after collection, so the trace footer is `complete: false`, but all 120 position records are present.
- The follow-up `axis-vector2` probe recorded 60 grounded Left commits through `0x0049f0e5` with `vector_4c` present on every event. Across that phase its fixed-point X/Z components ranged from `-2.329269` to `2.356400` and `-1.730164` to `2.320084`, while action state remained `physics_state=0`, mask `0x8000`, and held key `203`. These values vary during collision/platform handling and do not provide a stable Left/Right steering signature. The trace is `build/debug/sessions/axis-vector2/vector.trace.ndjson`; it was stopped after collection and has `complete: false`.
- A fresh decompilation of `0x0049c060` shows it is not the missing keyboard-to-motion producer. It clears `player + 0x3124`, compares the response magnitude at `+0x4c` with the temporary correction vector at `+0x3118`, derives a signed correction from the collision normal at `+0x3128/+0x312a/+0x312c`, and stores that result back at `+0x3124` while updating the surface-response mode at `+0x2e24`. Its only recovered callers are the ground-turn/air-contact paths, so it belongs to surface response and rotation rather than direct Left/Right displacement.
- Static disassembly of the skater object update at `0x0046dd60` narrows the immediate pre-physics chain. After the mode-dependent update at `0x004cd750` and the position/orientation history helpers at `0x0048ec00`, `0x004cc0e0`, and `0x0048f590`, the method calls `0x0046d6e0` immediately before `0x0049e680`. The recognized `0x0046d6e0` body only copies profile bytes at `player + 0x2ccc + 0x148/+0x149` into the skater's `+0x31a2/+0x31a1` analog/lean fields, with a two-player sign inversion. It does not write the response vector, live position, or a horizontal acceleration. The neighboring `0x0049e480`, `0x0049e430`, and `0x00492ea0` calls are mode/timer, action-animation, and state bookkeeping paths. This excludes the attractive immediate hook as the missing speed producer and identifies the next trace target as the first writer of the profile/controller fields and the argument setup feeding `0x00496060`.
- The complete normal-frame order is now bounded more tightly. `0x0049e680` copies the transient `+0x4c/+0x50/+0x54` vector, runs `0x00493370` (grounded turn/stance), clears `+0x58/+0x5c/+0x60`, then runs `0x0049b010`, `0x0049a280`, `0x00492ea0`, and `0x0049df00` before entering `Skater_PhysicsDispatcher` and its state handler. For ordinary ground state `0`, that handler is `0x0049dad0 -> 0x00496550 -> 0x00495cc0 -> 0x0049d9c0`. The shared `0x0049f0e5` handoff occurs after this state work and only collision-checks/commits the already-produced candidate through `0x00496060`.
- In `0x00496550`, the first vector chain combines temporary correction (`+0x58/+0x5c/+0x60`) and the transient response vector (`+0x4c/+0x50/+0x54`) using the fixed-step scales, then derives a ground/collision displacement and tests it through `0x004624d0 -> 0x00466090`. The later `0x00496060` calls add short surface deltas to that candidate. This explains why the corrected Left/Right traces alter the X/Z commit arguments without making `+0x4c` a stable steering field.
- The other candidate producers are state-specific or exceptional: `0x0049df00` damps/zeros the response vector for slope/brake/stop modes, `0x0049f4c0` writes platform bounce responses, `0x0049d8a0` damps replay/demo response state, and `0x00490730` can replace position during recovery/restart collision tests. `0x00469910` initializes the response words from the initial basis and then clears them. These remain separate from the normal `0x0049b010` correction path.
- `0x0049b010` is the ordinary grounded temporary-correction producer. After `0x00493370` has updated the facing basis, it computes the response speed metric (`magnitude(+0x4c) * 0x40`) and, under the profile/animation/speed gates, writes `-basis(+0x30f4) * {1,4,8}` to `+0x58/+0x5c/+0x60`. The outer `0x0049e680` frame later integrates that correction into `+0x4c/+0x50/+0x54`; this is the action-to-facing-to-acceleration path, not a direct position write. Native `ground_motion.*` reproduces these raw correction branches, and `ActionProfileState` now supplies the verified `+0x2ccc` profile slots to its frame hook. The animation writer is now represented by `ground_animation.*`: `FUN_00492ed0`'s exact 4/2/1 easing and `FUN_00492f20`'s state/frame targets are connected at the frame boundary. The wide-turn response-normalized write is also modeled; retail then clears the temporary correction vector before B010, so it is deliberately transient in the native frame result. Animation asset/event dispatch, local-profile selection, and B010's cooldown rearm side effects remain explicit.
- The surrounding threshold writer is also isolated. After the state dispatch, `0x0049e680` updates `+0x2dc8`: with `+0x2dd8==0`, it samples `FUN_0048f3a0(3)`, computes `(roll+0xaa)*0x2d000/0x118`, and decrements the old threshold by one only when that target is lower; with `+0x2dd8!=0`, it replaces the threshold with `(roll+0xdc)*0x2d000/0x118`. Native `GroundMotionThreshold` and `PlayerState::ground_motion_threshold()` preserve this stateful boundary, while the RNG roll and special-state predicate remain hooks. Native `PlayerState::ground_motion_speed_metric()` mirrors the retail dot/sqrt/`*0x40` sequence used by B010.
- The remaining B010 correction gates are written by the preceding `0x00493370`/`skater-action-3370.c` turn producer. It clears `+0x2e7c` at the start of its turn handoff, sets it when the turn accumulator is actively moved, and sets `+0x2e78` when the lean/profile branch selects the wide turn limit and applies the response-normalized correction. Native `GroundTurnResult` and `PlayerState` now carry those two verified flags, and the frame derives the wide `+/-0x5a000` limit from the retained profile/lean inputs. The normalized side effect and cooldown countdown are modeled; the remaining unresolved pieces are the local-profile table and B010 cooldown rearm conditions.
- `0x0048f5f0` is not the ordinary motion writer despite setting `+0x2d90 = 0xf`. Its callers are reset/recovery and ground-contact failure paths; the function also clears/refreshes animation and action state. `+0x2d90` is therefore a transient contact/turn latch, not a safe generic acceleration field.
- Consequently, the remaining faithful C++ work is to connect the recovered `0x0049b010` producer to the real profile/animation/stat writer and validate the candidate against corrected grounded traces. The queued-motion producer remains a separate verified special/action-stream path; it must not be promoted to generic keyboard acceleration. Native `PlayerPhysicsFrame` now exposes both seams in the retail order, including the post-turn temporary-correction reset.
- `0x0049b500` contains a dormant/caller-selected response-rotation branch when its third argument is nonzero, but every recovered retail caller currently passes zero, including the in-air callsite and ordinary grounded `0x00496360`. It is therefore not part of the confirmed first-playable path and is not exposed as native behavior yet; the proven function remains the orientation/basis writer, not a source of speed.
- The in-air decompilation closes an important ambiguity: `Skater_DoPhysicsInAir` uses the same `+0x4c/+0x50/+0x54` response plus `+0x58/+0x5c/+0x60` correction chain for position integration at `0x004cac30`/`0x004cacd0`/`0x004cabb0`. The `+0x310c/+0x3110/+0x3114` air-motion vector is updated by `0x00497df0` and consumed by the upright/orientation helper `0x0049c330`; it is not a second direct position velocity. Native should therefore keep air displacement on the response path while completing the missing air-control/orientation and launch-response branches.
- Native `GameplayFrameResult` now records `trigger_event_count_before/after` and copies the `LevelTriggerState` event delta generated by the level tick. This is the stable per-fixed-frame join between a retail script effect and the native object/timer/gap mutation; the full level event log remains authoritative.
- Native `PlayerPhysicsFrame` now drains the recovered queued-motion words before the action-stream callback, matching the retail producer/consumer order. The callback receives the local delta so the still-unverified `0x004e85a0` orientation transform can be supplied or tested independently; opcode `0x2b` can then seed the following frame without accidentally moving the current one.

## Interpretation

`0x0049db80` is the confirmed raw physics-state dispatcher and `0x00497f40`
is the confirmed shared in-air handler for raw states 1, 3, and 6. The object
layout and original public enum names remain unconfirmed.
The dynamic position watches and static cross-references identify `0x00496060` as a shared position commit path. The strongest state-specific caller is the in-air routine at `0x00497f40`; `0x0049f0e5` is the dispatcher handoff that supplies the prior-position/current-position vectors. `0x004ca9f0` is best treated as a reusable `Vector3_Add` helper rather than the owner of the player state.
The live probe ties both directional action edges to the shared handoff, and the corrected static callsite analysis identifies the actual X/Y/Z argument order. The `+0x4c` field is now excluded as the direct steering vector by both its platform/collision producers and the runtime variation. `0x00493370` is the first confirmed input-dependent producer before the state-specific position handoff; exact facing and acceleration semantics remain open.

## Dispatcher state contract

The second switch in `0x0049db80` provides a stable runtime contract for the
numeric physics state at `player +0x30b8`:

| state | direct handler sequence | observed role |
| ---: | --- | --- |
| 0 | `0x0049dad0`, `0x00496550`, `0x00495cc0`, `0x0049d9c0` | ground/collision path |
| 1 | `0x00497f40` | in-air path |
| 2 | `0x00496550` | ground/collision variant; writes `+0x30c4 = 1` |
| 3 | `0x00497f40` | in-air path with timeout check |
| 4 | `0x00494210` | separate rail/special path |
| 5 | `0x00499710` | separate special path |
| 6 | `0x004993f0`, then `0x00497f40` | transition into in-air handling |
| 7 | ground helpers, then copies `+0xbc/+0xc0/+0xc4` into position | reset/history ground path |
| 8 | `0x004995d0` | separate special path |

The state number is therefore not a free-form animation or collision flag:
it selects the state-specific position/response producer. The dispatcher also
uses `player +0x30c4` as a secondary state/result word, clearing it for states
0, 4, 5, and 8 and setting it to one for state 2.

The input-dependent producer `0x00493370` keeps the two movement-target words
`+0x3144` and `+0x3148` bounded and finally copies the X target into the Z
target before calling `0x00492f20`. With no directional action it decays the
X target by one quarter per update (one half while braking), and moves the Z
target toward zero in steps of at least `0x400`. Directional action changes
the target by a frame-scaled step and clamps it to the observed ±`0x2d000`
normal range or ±`0x5a000` braking range. This is a concrete recreation rule
for the steering-target integrator; it does not yet assign world-facing axes
or a public “turn speed” name.

The following heading/animation transition rules are also fixed by
`0x00492f20`, although the numeric states remain operational labels. With the
ordinary mode bit clear, a nonzero target and no `+0x2dd8` override map the
target magnitude to a maximum heading frame of `0x16` (22); positive and
negative targets select animation states 6 and 7 respectively, resetting
`+0xf4` when entering a new direction and approaching the frame target through
`0x00492ed0`. With the mode bit set, the sign-to-state selection is reversed.
When `+0x2dd8` is present, the transition uses maxima 12 and 15 and promotes
states 6/7 to states 9/10 at the corresponding sign; completion at frame 12 or
15 reaches `0x00496280`. A zero target emits the state-specific reset/idle
callbacks for states 6/7 or 9/10. These rules are enough to reproduce the
target-to-heading state machine without naming the original animation enums.

## Runtime physics-state contract

The raw state at `player +0x30b8` is a stable eight-way dispatcher contract.
The following semantic labels are provisional recreation names, corroborated by
the matching handler order and bounded Warehouse transitions; native code should
retain the raw numeric value:

| Raw value | Operational label | Handler/behavior |
| ---: | --- | --- |
| `0` | on ground | Ground/collision helper sequence and normal position commits |
| `1` | in air | `0x00497f40`; airborne position updates until accepted contact |
| `2` | collision transient | `0x00496550`, with `+0x30c4 = 1` during the response |
| `3` | in air / stick-to variant | Shared `0x00497f40` path with separate timeout recovery |
| `4` | on rail | `0x00494210` rail/special handler |
| `5` | wallride | `0x00499710` special handler |
| `6` | footplant transition | setup through `0x004993f0`, then shared in-air handler |
| `7` | stopped/reset | restores the live position from `+0xbc/+0xc0/+0xc4` |
| `8` | handplant | `0x004995d0` special handler |

The direct air-to-ground transition is also fixed. In the accepted-contact
branch of `0x00497f40`, the game commits the contact position through
`0x00496060`, records the contact-side state, and calls the state-request helper
at `0x004991fe` with requested state `0` and reason `0x1fd6`; the actual raw
state store is `0x004902bf`. Raw state `3` uses the same in-air handler but
defers the ordinary state-0 request during its launch-grace frame window.

## Cross-build physics-state labels

The PC import does not contain a directly named `EPhysicsState`, but the
bundled PSX symbol data provides the original declaration order in
`build/assets/all-pkr/files/data/SKATE2.TAG` (`physics.h` lines 20–28):
`PHYSICS_ON_GROUND`, `PHYSICS_IN_AIR`, `PHYSICS_ON_INVISIBLE`,
`PHYSICS_IN_AIR_STICK_TO`, `PHYSICS_ON_RAIL`, `PHYSICS_IN_WALLRIDE`,
`PHYSICS_IN_FOOTPLANT`, `PHYSICS_STOPPED`, and `PHYSICS_IN_HANDPLANT`.

The PC dispatcher has the corresponding cases in the same order. Its case 0
uses the grounded helper sequence; cases 1 and 3 use `0x00497f40`; case 2
uses the collision/transient ground helper and sets `+0x30c4`; case 4 uses a
dedicated rail-path handler; case 5 uses a wallride-path handler; case 6 uses
the footplant pre-air setup `0x004993f0` before falling through to the common
air handler; case 7 restores position history through the stopped-path
sequence; and case 8 uses the handplant-path handler. The source symbol
function order in `EDITEDMAP.TXT` independently lists the matching PSX
handlers (`DoOnRailPhysics`, `DoOnGroundPhysics`, `DoInAirPhysics`,
`DoFootPlantPhysics`, `DoHandPlantPhysics`, and `DoWallRidePhysics`).

This is strong cross-build corroboration for provisional semantic labels, not
direct PC symbol evidence. Runtime raw values, transition writers, and
dispatcher handlers remain authoritative. The native `classify_physics_state()`
and `DispatchResult::semantic_state` keep these labels separate from the raw
integer and from the generic `DispatchKind::CollisionTransient`/state-path
categories.

## Movement action handoff

Static decompilation identifies `FUN_00493370` as the per-frame action/velocity
step called from the main physics frame at `0x0049e982`, before prephysics and
the `0x0049db80` dispatcher. It consumes the action-record bank through the
player's `+0x2ccc` pointer. In grounded raw states `0` and `7`, the routine
reads the Left record at `+0x80` and Right at `+0x90`, updates the player
movement-target candidates at `+0x3144` and `+0x3148`, and then calls the
heading/steering helper `0x00492f20`. Its non-ground path also consults the
spin records at `+0x40` and `+0x60`. This establishes the consumer boundary;
it does not establish the final steering coordinate system or acceleration
formula.

The bounded runtime trace
`build/debug/sessions/movement-step2/movement.trace.ndjson` confirms the
boundary in Warehouse. The probe at `0x00493370` recorded 1,200 current-player
calls with the full 12-record action bank. A delayed action injection at
`0x00489a15` supplied Left (`0x8000`) for 60 updates:

| Frame | Action record / state | Handoff fields |
| ---: | --- | --- |
| 619 | mask `0`, Left byte `+0=0`, raw physics state `0` | targets `+0x3144/+0x3148 = 0` |
| 620 | mask `0x8000`; Left record at `0x0056b078` becomes held with edge latch | state remains `0`; targets still `0` |
| 621–625 | Left held; `+0x04` counts `2..6` | `+0x2e7c=1`, `+0x2e78=0`; both targets move to `-30720,-61440,...,-153600` |
| 626–679 | Left held; counter reaches `60` | both targets saturate at `-184320`; heading input/deadband bytes remain `0` |
| 680 | live mask returns to `0`; Left held byte clears | steering remains active for the release/update boundary, then decays in later calls |

The runtime caller is consistently `0x0049e982`, and the dispatcher remains on
case `0` throughout this controlled window. The trace therefore confirms
action-record → `0x00493370` → movement-target handoff without conflating it
with the later grounded position commit at `0x0049f0e5`.

The target arithmetic is now recovered from the same function. With the
runtime fixed-point delta `dt = DAT_0056865c`, the normal ground profile uses
`step = (0x3c * 0x100 * dt) >> 8`; profile `+0x29b7 == 1` uses `0x78`, and
other nonzero profiles use `0xb4`. Raw states `0` and `7` use target limit
`0x2d000`. If `heading_deadband >= 0x1f` or Down is held, the brake path doubles
the step, raises the limit to `0x5a000`, and damps velocity into acceleration
using the integer speed length. Otherwise Left subtracts the step and Right
adds it, with saturation. With no digital direction, the target decays by a
quarter (or half in brake mode); analog heading input converges toward
`limit * heading_input / 128`. The final store copies `+0x3144` to `+0x3148`
before the call to `0x00492f20`.

The native `apply_ground_action_step()` implements this target/brake portion,
including the observed Warehouse `dt=0x200` Left sequence (`-0x7800` per
update, saturating at `-0x2d000`). The helper's animation/lean side effects,
the orientation-dependent acceleration preparation in `0x0049b010`, and
collision response remain explicit boundaries rather than being inferred from
the target trace alone.

## Fixed-point basis and acceleration contracts

The retail vector arithmetic used by the later ground and postphysics stages
is now resolved at the helper level. `0x004f5f90` computes

```text
truncate_toward_zero((a.x*b.x + a.y*b.y + a.z*b.z) / 4096)
```

and `0x004f5fc0` computes the same truncate-toward-zero Q12 conversion for a
scalar/vector component. Both use the x87 conversion helper at `0x005004f4`;
the constant at `0x00519908` is `1/4096`. `0x004f53b0` is an x87 square root
followed by the same integer conversion, so the grounded brake and
postphysics speed metrics use the Q12 dot result before taking the length.

`0x0049c7d0` is a basis-field copy, not a general-purpose normalization:

```text
+0x30f4/+0x30f8/+0x30fc <- object +0x2e5c/+0x2e62/+0x2e68
+0x3100/+0x3104/+0x3108 <- object +0x2e58/+0x2e5e/+0x2e64
+0x310c/+0x3110/+0x3114 <- object +0x2e5a/+0x2e60/+0x2e66
```

The tail of `0x00496550` is now also arithmetic-complete when its collision
inputs are supplied. In raw state `0` with `+0x2f64 == 0`, it projects velocity
off the `+0x3100` basis using `0x004f5f90`/`0x004f5fc0`, then computes the
velocity dot `+0x30f4` basis. It subtracts the resulting vector scaled by
coefficient `8` when the indexed collision flag is enabled and `+0x2d94 == 0`;
contact identities `5`/`6` add coefficient `0x78`, identity `3` adds `0xb4`,
and `+0x2dd4 != 0` adds its `+0x2dfc` coefficient. Every term is multiplied by
`DAT_0056865c` and converted with `>> 8`.

The packed-normal path is also resolved statically. `0x00490610` receives the
velocity pointer followed by two packed words and sign-extends
`(word0.low16, word0.high16, word1.low16)` as the Q12 normal. It subtracts
`dot(v,n) * n` using the same `0x004f5f90`/`0x004f5fc0` helpers without first
normalizing the supplied components. `0x00490680` copies the velocity,
applies that plane projection, then rescales each projected component by
`((original_length * 0x40) >> 8) / ((projected_length * 0x40) >> 8)` using
integer component multiply/divide. The native `decode_surface_normal()`,
`project_vector_off_surface()`, and
`project_velocity_preserving_speed()` helpers model this contract; collision
selection remains caller-supplied, while the selected-result publication
handoff is modeled below.

In the surrounding `0x00496550` collision-result branch, the already selected
normal is reused twice: `0x00490610` projects the state-2 vector
`+0x2da8..+0x2db0` or fallback `(0,0x1964,0)` into the acceleration handoff,
then `0x00490680` applies the same plane projection/speed restoration to
velocity. The projected vector is published at `+0x3118..+0x3120` except for
raw state `2`; the raw packed words are stored at `+0x3128/+0x312c` in both
cases. This is the boundary represented by native
`apply_ground_collision_handoff()`.

The frame-start gravity scalar at `+0x2dac` is likewise recovered from
`0x0049e680`: base `(+0x29f4 * 13000) / 100`, raw state `2` transient scaling
`((500-random) * base * 20) / 10000`, then sequential `150%`, `50%`, and
`50%/200%` global modifiers. The native helper accepts those global flags and
the raw-state-2 random draw explicitly.

Finally, `0x0049d480` is a deterministic postphysics velocity stage once its
shared RNG draws are supplied. It computes `speed = sqrt(dot(v,v))*0x40`,
compares against a cap `((rand(3)+500)*0x2d000)/0x118`, independently rescales
the three components if capped, recomputes speed, then applies component drag
when the threshold `((rand(3)+0x186)*0x2d000)/0x118` is exceeded. The drag is
`v -= (truncate_toward_zero(100*v/4096) * dt) >> 8`. When the indexed player/global flag
is zero, it additionally applies the `<0x10000` / `<0x2000` low-speed damping
and zeroes components below `0x10`. The native
`apply_velocity_damping()` helper takes the five draws and that final gate as
inputs; it does not invent the shared retail random stream.

## Open questions / falsifiers

- Use the stable callsites at `0x00498cf5`, `0x0049905c`, and `0x0049917b` as software-breakpoint targets when the WineDbg proxy refuses to stop at the shared callee entry.
- Trace the values feeding `0x0049917b` across a longer Left hold and compare them with the action-state record at `0x0056b078`.
- Inspect the argument setup immediately before `0x0049f0e5` when validating
  the target/acceleration result against a same-state replay.
- Compare a delayed Right injection against the Left handoff using the same
  `0x00493370` probe; retain the raw target values before assigning axes or
  signs.
- Follow the animation/heading side effects through `0x00492f20`, then recover
  the orientation-dependent acceleration preparation at `0x0049b010`.
- Use a runtime rail/wallride/plant capture to independently confirm the
  cross-build semantic labels; the current controlled traces only directly
  observe grounded, airborne, and collision-transient raw values.
- Do not assign final names to the object vectors until repeated runtime observations support them.
