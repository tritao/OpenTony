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
- `0x0049f4c0` is a separate producer for the `+0x4c/+0x50/+0x54` vector. Its decompilation contains `platform.cpp` and `unknown bouncy platform type` diagnostics, switches on a platform type, and writes platform/bounce response values into those three fields. Static callers pass platform/object vectors rather than the keyboard action records. The `+0x4c` vector is therefore collision/platform response state, not the direct steering vector.
- The position probe now models these as caller-side callsites: at the breakpoint on the `call` instruction, the pushed arguments are at `ESP`, `ESP+4`, and `ESP+8`; a callee-entry argument helper must start at `ESP+4` only after the return address exists. The earlier probe implementation skipped the first pushed word and recorded `ESP+4`, `ESP+8`, and `ESP+12` instead.
- The repeatable `tony-position-commit 16` probe captured 16 stationary dispatcher calls with `action_mask=0` and `physics_state=0`, followed by calls through `0x0049917b` (`Skater_DoPhysicsInAir+0x123b`) after Left input. A correlated event recorded `action_mask=0x8000`, held key `203` (Left), and `physics_state=1`; its historical `argument_values` were collected before the callsite correction and are not actual X/Y/Z arguments. The complete trace is `build/debug/sessions/position-probe3/position.trace.ndjson`.
- A controlled Warehouse Left/Right comparison reproduced the input-to-commit handoff in two fresh isolated sessions. The Left trace (`build/debug/sessions/lr-compare4/lr.trace.ndjson`) recorded the action edge at `0x0056b078`, then an `in-air-position-3` call at `0x0049917b` with `action_mask=0x8000`, held key `203`, and `physics_state=1`. The Right trace (`build/debug/sessions/lr-compare6/lr.trace.ndjson`) recorded the action edge at `0x0056b088`, then repeated `physics-dispatch-position` calls at `0x0049f0e5` with `action_mask=0x2000`, held key `205`, and `physics_state=0`. These traces predate the callsite-argument correction, so their recorded `argument_values` are retained only as shifted stack samples, not as the actual callee arguments.
- The side-by-side result confirms the action-state mapping `Left -> 0x8000 / DirectInput 203` and `Right -> 0x2000 / DirectInput 205`. Both feed the shared `0x00496060` position commit routine, but these captures entered different state-specific callers: Left while airborne and Right through the dispatcher while grounded. The caller difference is therefore a state difference, not evidence of separate Left/Right position writers.
- A same-state ground comparison (`build/debug/sessions/ground-compare2/ground.trace.ndjson`) captured 31 Left and 30 Right position commits through the same `physics-dispatch-position` callsite `0x0049f0e5`, all with `physics_state=0`. Its action-state and callsite correlation remains valid, but the stored argument fields were collected before the callsite-argument correction and therefore represent `ESP+4`, `ESP+8`, and `ESP+12`, not X, Y, and Z. A replacement ground comparison should use the corrected probe before drawing axis-level conclusions.
- The corrected headless comparison (`build/debug/sessions/axis-compare4/axis.trace.ndjson`) captured 60 Left and 60 Right commits through `0x0049f0e5`, all with `physics_state=0`; Left held DirectInput key `203` and action mask `0x8000`, while Right held key `205` and mask `0x2000`. The actual callsite arguments were now recorded in X/Y/Z order. Left changed from `0x01a4ff50, 0x0006dd2b, 0x0273b78d` to `0x0194bf45, 0x00026520, 0x023a0a3e`; Right changed from `0x01e10507, 0x000fb62f, 0x025dcceb` to `0x01f28548, 0x0004fa9b, 0x021edc5d`. Both directional phases therefore alter the horizontal X/Z candidate components while remaining on the same grounded dispatcher path. The debugger was stopped after collection, so the trace footer is `complete: false`, but all 120 position records are present.
- The follow-up `axis-vector2` probe recorded 60 grounded Left commits through `0x0049f0e5` with `vector_4c` present on every event. Across that phase its fixed-point X/Z components ranged from `-2.329269` to `2.356400` and `-1.730164` to `2.320084`, while action state remained `physics_state=0`, mask `0x8000`, and held key `203`. These values vary during collision/platform handling and do not provide a stable Left/Right steering signature. The trace is `build/debug/sessions/axis-vector2/vector.trace.ndjson`; it was stopped after collection and has `complete: false`.

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
