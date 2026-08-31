# PC retail player collision and recovery frontier

Status: static reconstruction recovered for the current Warehouse
collision/recovery frontier. The Windows PC retail executable is the primary
source for this document. The names below are semantic names where the call
contract is established by the disassembly; a name remains provisional where
the producer of a value is not yet identified.

## Connected call graph

The current connected control flow is:

```text
Skater_PhysicsFrame 0049e680
  -> outer floor/restart helper 00490730
  -> prephysics producers 00493370 / 0049b010 / 0049a280
  -> Skater_PhysicsDispatcher 0049db80
       state 0 / 7:
         Skater_GroundPreparation 0049dad0
         -> Skater_DoGroundPhysics 00496550
            -> Skater_ExitGroundCollisionRecovery 004956f0
            -> Skater_ApplyCollisionResponse 0049bad0
            -> Skater_RebuildOrientationRecovery 0049d080
            -> Skater_PositionWritePath 00496060
         -> Skater_GroundPostCollision 00495cc0
         -> Skater_RunGroundedBounceRecovery 0049d9c0
            -> Skater_GroundedBounceProbe 004957c0
               -> collision query 004624d0/00466090/0048ea80
               -> Skater_ApplyCollisionResponse 0049bad0
       state 2:
         Skater_DoGroundPhysics 00496550
       state 1 / 3 / 6:
         Skater_DoPhysicsInAir 00497f40
         -> Skater_UpdateAirBasis 00497df0
         -> Skater_AirCollisionRecovery 00497aa0
         -> Skater_ApplyCollisionResponse 0049bad0
         -> Skater_RebuildOrientationRecovery 0049d080
       state 4:
         Skater_LandingCleanup 004914d0
         -> Skater_RebuildOrientationRecovery 0049d080
```

Evidence for the dispatcher order is the switch at `0x0049db80`. The ground
case calls `0x0049dad0`, `0x00496550`, `0x00495cc0`, and `0x0049d9c0` in that
order. The state-2 case calls `0x00496550` without the final bounce helper.
The in-air cases enter `0x00497f40`, and the shared response/orientation
helpers are called from ground, air, rail, and swept-contact paths. The outer
wrapper at `0x0049e680` performs the frame setup and post-dispatch handoff;
it is not itself the owner of every collision predicate.

The order before dispatch is material. `0x0049e680` copies the persistent
response, invokes the grounded/action producers, clears the temporary
correction, and then enters `0x00490730` and the prephysics calls before the
state switch. After the state handler it restores the saved/current position
through `0x00496060` when the outer guard permits it. This is why the support
query, response correction, and final position write cannot be modeled as one
collision callback.

## ABI and function contracts

| PC | semantic name | static ABI | callers / relevant callees | confidence |
|---|---|---|---|---|
| `0x004956f0` | `Skater_ExitGroundCollisionRecovery` | `__fastcall(void *skater)` | `0x00496550`; calls `0x004900b0`, `0x0048f5f0`, animation request, `0x00491b80`, trick script start | confirmed |
| `0x004957c0` | `Skater_GroundedBounceProbe` | `__thiscall(int direction, int distance, int heading)` returning `int` | `0x0049d9c0`; calls collision query, `0x0049bad0`, collision debug/service helpers | confirmed |
| `0x00496550` | `Skater_DoGroundPhysics` | `__fastcall(void *skater)` | `0x0049db80`; calls the collision/recovery helpers in this document | confirmed |
| `0x00496060` | `Skater_PositionWritePath` | `__thiscall(x, y, z)` returning `void` | ground, air, and outer frame handoff; calls `0x004624d0`, `0x00466090` | confirmed |
| `0x00496360` | `Skater_GroundSurfaceResponseStep` | `__fastcall(void *skater)` | `0x00496550`; calls `0x0049c060`, `0x0049b500` | confirmed |
| `0x00497aa0` | `Skater_AirCollisionRecovery` | `__thiscall(void *query)` | `0x00497f40`; calls state request, response, and animation helpers | confirmed |
| `0x00497df0` | `Skater_UpdateAirBasis` | `__fastcall(void *skater)` | `0x00497f40`; calls fixed normalization/cross/publish helpers | confirmed |
| `0x00497f40` | `Skater_DoPhysicsInAir` | `__fastcall(void *skater)` | `0x0049db80`; calls air integration, collision query, `0x00497aa0`, `0x00491780`, `0x00497960` | confirmed |
| `0x00491780` | `Skater_AirCollisionOrientationRecovery` | `__fastcall(void *skater)` | `0x00497f40`; calls normal projection, history/basis helpers, state request | confirmed |
| `0x00497960` | `Skater_AirLandingOrientationRecovery` | `__fastcall(void *skater)` | `0x00497f40`; called by the accepted-contact landing branch | confirmed |
| `0x0049bad0` | `Skater_ApplyCollisionResponse` | `__thiscall(u32 packed_xy, i16 z, int heading)` | ground, air, rail, swept-contact callers; calls fixed dot/multiply, matrix rotation, `0x0049c7d0`, `0x0049c850` | confirmed |
| `0x0049d080` | `Skater_RebuildOrientationRecovery` | `__fastcall(void *skater)` | ground, air, rail, swept-contact callers; calls fixed normalization/cross/publish helpers | confirmed |
| `0x0049d9c0` | `Skater_RunGroundedBounceRecovery` | `__fastcall(void *skater)` | ground dispatcher case 0/7; calls `0x004957c0` and bounce service/debug path | confirmed |
| `0x0049db80` | `Skater_PhysicsDispatcher` | `__fastcall(void *skater)` | `0x0049e680`; calls state-specific routines | confirmed |
| `0x0049e680` | `Skater_PhysicsFrame` | `__fastcall(void *skater)` | gameplay frame caller; calls dispatcher and frame services | confirmed |

The `0x004957c0` parameter roles are direction (`-1`/`+1`), probe distance,
and the heading value passed to `0x0049bad0`. A zero heading is converted by
`0x0049bad0` to its default `0x19` value. The `0x00497aa0` query parameter is
the selected collision record; it is not a second player object.

## Grounded bounce probe: `0x0049d9c0` -> `0x004957c0`

At `0x0049d9c0`, the first read is skater `+0x2f64`. The x86 `neg/sbb` mask
then selects a probe distance of `0x0a` when the word is zero and `0x46`
(70) when it is nonzero. These are world-space probe lengths, not Q12
normalization constants.

The helper saves the live position, then runs this directional sequence:

1. Probe `-1` at the selected distance.
2. If it misses, probe `+1`; if that misses, return.
3. If the first probe missed and the second hit, probe `-1` again; on success
   restore the saved position and enter the bounce service path.
4. If the first probe hit, probe `+1`; on success restore the saved position
   and enter the bounce service path.

The bounce service path restores the saved position, calls the debug/service
producer with `0x898, 1, 0`, and returns. A failed second directional probe
returns without that restore path.

At `0x004957c0`, the first collision line is built from the current skater
position plus `+0x310c * 0x1e`, then an endpoint is formed by adding
`+0x3100 * direction * distance`. A hit is accepted only when

```text
-0x400 < dot_q12(hit_normal, skater_+0x310c) < 0x400
```

The helper restores the saved `+0xbc` position before the response stage.
When the caller's heading parameter is nonzero, the following gate can select
the `200` heading value before the response call:

```text
animation/state word +0xf6 != 0xc
FUN_004ca8f0() > 6
word +0x2f64 == 0
word +0x2c74 == 0
dot_q12(hit_normal, skater_+0x30f4) > 0xb50
```

The gate emits animation request `(0xc, 0x167f)` and calls `0x0046e1d0`
before invoking `0x0049bad0`. This is a service/input seam; it is not
equivalent to unconditionally choosing a yaw from the recorded normal.

The second collision line is the important position contract. Both endpoints
are based on the restored previous position:

```text
start = previous_position + hit_normal * 1
end   = previous_position + hit_normal * direction * distance
```

If this second query misses, the endpoint is written directly to the live
position and `+0x3200` is set to one. If it hits, the helper follows the
debug/service restore path. The normal multiplication is a direct raw short
component multiply; it is not a Q12 rescale.

The exact callsite sequence and the `0x0a`/`0x46` selection are from the
instruction ranges `0x0049d9c0..0x0049dad0`. The query/response ordering is
from `0x004957c0..0x00495ae2`.

## Ground collision and recovery: `0x00496550`

`0x00496550` first clears its local collision/recovery accumulator and runs
the state-dependent pre-query producers. It integrates the candidate position
from persistent response `+0x4c/+0x50/+0x54` and temporary correction
`+0x58/+0x5c/+0x60`, then submits the ordinary movement segment. The selected
normal is the collision-query short triplet at `+0x120/+0x122/+0x124` in the
query record.

The later support/recovery segment is a separate line, not a vertical
“search eight thousand units” replacement:

```text
support_start = live_position + skater_+0x310c * 0x46
support_end   = support_start + skater_+0x310c * -0x100
```

Its surface bits come from `0x0048ea80` and are retained in the collision
globals. The relevant decoded values are:

| global | static expression | role in this routine |
|---|---|---|
| `CollisionResultFlagSurface40` (`DAT_0056b768`) | `(face[0xc] >> 16) & 0x40` | selects the surface-response branch |
| `CollisionResultFlagInverse23` (`DAT_0056b7a8`) | `(~face[0xc] >> 23) & 1` | inverse material/filter predicate |
| `CollisionResultFlagInverse24` (`DAT_0056b7b8`) | `(~face[0xc] >> 24) & 1` | inverse material/filter predicate |
| `CollisionResultFlagFace80` (`DAT_0056b7ac`) | `face[0] & 0x80` | face/service recovery predicate |
| surface class (`DAT_0056b7e8`) | `(face[0xc] >> 25) & 0xf` | raw class consumed by special paths |

The ordinary branch calls `0x004956f0` from three static sites
(`0x0049744e`, `0x004974d5`, and `0x00497571`). The routine does not have a
single “steep class 12” predicate. Its branch conditions combine the decoded
surface bits, state, the selected normal, the recovery timer, and the
response/threshold accumulators.

The support-hit exit predicate at the state-0 tail is explicit in the PC
instructions around `0x00496550 + 0x400` (`0x00496950..0x0049699b`):

```text
if Surface40 == 0:
    if Inverse24 == 0:
        call 0x004956f0
    else if state == 0:
        target_dot  = dot_q12(player+0x80, hit_normal)
        forward_dot = dot_q12(player+0x30f4, hit_normal)
        if 0 < target_dot < 3000 and forward_dot < 0 and player+0x50 < 1:
            call 0x004956f0
```

This is a face-bit and vector predicate, not a material-ID heuristic. The
Warehouse evidence distinguishes all three cases. The straight support hit
`[0,-2272,-3408]` on face word `0x1800` has target dot `2272` and takes the
orientation-gated exit; the idle support hit `[0,-3019,2768]` has target dot
`3019` and remains on the ordinary path; a later straight support hit
`[-4034,0,-711]` on `0x580` has `Inverse24 == 0` and takes the direct exit.
The native predicate is recorded in `collision_recovery.cpp` and receives
the target normal, forward basis, response, and decoded face bits as separate
inputs.

The state-2 special branch writes the recovery normal to the state-2 normal
words, sets the recovery progress to `0x19000`, removes the normal component
from the forward basis, rebuilds the orientation basis, and requests state 0
with reason `0x19bf`. The same function's ordinary state-0/state-1 branch can
request state 2 with reason `0x1ac9` after the recent-window test, or request
state 1 with reason `0x1ab6` after the long recovery response shaping. These
are separate branches and must not be collapsed into a surface-flag name.

The final correction tail runs after the selected collision branch. For state
0 it projects response onto `+0x3100` and, when its profile/speed gates are
open, applies the forward-basis term using frame scale. Surface class 3/5/6
has an additional term. The tail consumes already-published response and
normal data; it does not choose the collision candidate.

The common support tail writes the response-side working vector at
`+0x3118/+0x311c/+0x3120` and the response normal at `+0x3128/+0x312c` before
calling `0x00490680`. For state 2 the vector is transition-owned; for the
ordinary branch it is the local correction beginning with `0x1964` and
projected against the selected normal. This ordering is the contract consumed
by `0x00496360 -> 0x0049c060` on the next relevant frame.

## State-2 recovery exit: `0x004956f0`

The function has two exact entry branches:

```text
if skater_+0x30b8 == 2:
    FUN_004900b0(1, 0x1605)
    return

FUN_004900b0(1, 0x160b)
FUN_0048f5f0()
skater_+0x3068 = 0
skater_+0x306c = 0
skater_+0x2c68 = 0
skater_+0x2ddc = 1
skater_+0x2dd0 = 0
```

Only when `+0x2f64 == 0` does it continue into the action/animation cleanup:

```text
if +0x29d0 != 0 && +0x2dd4 != 0:
    FUN_00491b80(+0x29d0)
    +0x29d0 = 0
    +0x2dd4 = 0
    return

+0x29c8 = 0
if +0x2dd8 != 0:
    Animation_RequestStart(0x1a, 0x1628)
    +0x2dd4 = 0
    return

Animation_RequestStart(0x1b, 0x162a)
+0x2dd4 = 0
```

The `0x1605` state-2 exit therefore has no ordinary cleanup writes. The
`0x160b` branch owns the cleanup order above; animation, trick-stream, and
`0x0048f5f0` are external/service effects, while the listed skater words are
persistent state.

## Collision response and orientation: `0x0049bad0`

This is one ordered retail operation even though native code currently exposes
two helpers. Its static order is:

1. Convert the packed collision normal to three signed components and use
   `+0x4c/+0x50/+0x54` as the persistent response vector.
2. Compute the response/normal fixed dot. If it is negative, subtract the
   projected response component and add the normal scaled by `0xcd`.
3. Test the negative collision vector against the current forward basis
   `+0x30f4`. If the orientation predicate is negative, rebuild the temporary
   matrix, choose a signed quarter-turn from the supplied heading, rotate it,
   normalize the published basis, and call the orientation publisher.
4. Recompute the cross-product basis and publish the short orientation/basis
   fields through `0x0049c850`.

The heading is `0x19` when the callsite supplies zero. The orientation branch
uses `0x400 - heading` or `heading - 0x400`, masks the result to 12 bits, and
therefore treats `200` as an angle-table value, not degrees. Matrix scratch
words at `DAT_006a3e10` and nearby are temporary working state; they are not
player persistence. Player fields written by the final basis publication are
the nine short orientation words at `+0x2e58..+0x2e68` and the three basis
triplets at `+0x30f4`, `+0x3100`, and `+0x310c`.

The exact response-before-orientation ordering is visible in
`0x0049bad0..0x0049c03d`; the shared callers are `0x004957c0`, `0x00496550`,
`0x00497f40`, and `0x00498330`.

`0x00496360` is the adjacent grounded response step, not another collision
query. It consumes the correction timer at `+0x2d90`, calls `0x0049c060` with
the surface-response vector/normal contract when that timer is active, then
calls `0x0049b500` and updates `+0x3068/+0x306c`. The pre-query call order is
`0x00496360` before the ordinary movement line in `0x00496550`; the native
frame preserves that boundary even where the upstream profile/stat producer
is still an explicit input.

## Shared position writer: `0x00496060`

The function is `__thiscall` with proposed X/Y/Z stack arguments and
`ret 0x0c`. With `+0x3200 != 0` it writes the proposal directly to
`+0x08/+0x0c/+0x10`. Otherwise it collision-tests the proposal and tries the
component candidates in this exact order:

```text
proposed XYZ, current X, current Z, current Y,
current Y/Z, current X/Y, current X/Z, current XYZ fallback
```

The calls to `0x004624d0`/`0x00466090` are temporary query outputs, while the
selected position is persistent player state. `0x0049e680` can invoke this
writer after the dispatcher with the saved/current position handoff. Ground
movement-hit and support-hit branches therefore decide which candidate is
passed to this shared writer; the writer itself does not infer the reason for
the candidate.

## Orientation recovery: `0x0049d080`

This helper runs only while `+0x3130 < 0x18001`. At exact progress
`+0x3130 == 0x18000` it uses the target short normal at `+0x80/+0x82/+0x84`.
Otherwise it advances each component of the current recovery base
`+0x3134/+0x3138/+0x313c` toward those target shorts by an arithmetic shift
right of two. It then normalizes the target, crosses it with the current
forward/tangent vector, crosses again to form the paired tangent, and
publishes the basis.

This function writes temporary basis/orientation state first and publishes
the resulting persistent short matrix and `+0x30f4/+0x3100/+0x310c` vectors
last. Its callers at `0x00494210`, `0x00496550`, `0x00497f40`, and `0x00498330`
share the contract. The terminal equality and the strict `< 0x18001` gate
are material; replacing them with an approximate progress threshold changes
which normal is published.

## Air, landing, and orientation handoff: `0x00497f40`

`0x00497f40` integrates the same response `+0x4c/+0x50/+0x54` and temporary
correction `+0x58/+0x5c/+0x60` chain as the grounded routine, then submits its
air collision line. An accepted ordinary landing first forms the contact
plus `+0x310c * 0x1e` candidate and applies the contact gate using the decoded
`Inverse24`, `Surface40`, `Face80`, state, recovery mode, action table, and
animation clock. The accepted state transition is state 0 with reason
`0x1fd6`; the state-3 grace condition permits the landing while its age is at
most `0x1e`. The ordinary branch clears `+0x3144` and `+0x29dc`, stores the
surface class at `+0x30b0`, and runs the landing cleanup/service calls before
the next frame.

The air recovery subpaths are ordered, not interchangeable:

* `0x00497aa0` selects state 2/reason `0x1caa` for the recent or face-bit
  recovery case and state 1/reason `0x1cb1` otherwise, then shapes response
  and clears the recovery marker.
* `0x00491780` removes the contact-normal component from a local response,
  requires the speed metric to exceed `0x1e000`, and uses the strict basis
  thresholds `abs(dot) > 0xfc1`, `< 0xb50`, and the deeper `< 0x666` history
  branch. The history delta is `position + 0xbc - +0x2e00`; after the
  fixed-point shift/normalization it supplies the handedness for the rebuilt
  forward/up basis and possible state request.
* `0x00497960` accepts the recovery orientation window only when the target
  dot is between `-2000` and `2000` and either `+0x50 > 150000` or the dot is
  below `200`. It publishes the reversed forward/up basis, writes response
  `(-normal) * 10`, and clears `+0x50`.
* `0x00497df0` updates the air basis scalar by subtracting `500` per frame to
  a floor of `-0xe0c`, then normalizes and republishes the paired basis.

After the landing state request, the non-state-1 path removes the selected
normal from response, seeds `+0x80/+0x3134/+0x3138/+0x313c`, resets `+0x3130`,
and calls `0x0049d080`. The state-2 to state-1 recovery exit at
`0x004956f0` uses reason `0x1605`; the ordinary exit uses `0x160b` and its
separate cleanup writes. These reason codes are state-machine contracts, not
surface class labels.

## Side-effect ownership

| category | fields/globals | owner |
|---|---|---|
| persistent player state | `+0x4c`, `+0x50`, `+0x54`; `+0x30b8`; `+0x30c4`; `+0x2f64`; recovery words; position/history | state-specific retail helper and outer frame in the listed order |
| temporary working state | local query records; matrix scratch `DAT_006a3e10..`; local dot/cross products | current helper only |
| collision-query outputs | query hit body, `+0x120/+0x124` normal, line parameter, selected face | collision query plus `0x0048ea80`; consumers must not recompute material bits |
| causal service inputs | `FUN_004ca8f0`, `FUN_0046e1d0`, `FUN_0048f5f0`, animation requests, `FUN_00491b80`, trick script start, shared random/stat values | caller/service boundary; recording already carries these where qualified |
| derived outputs | decoded surface flags, basis vectors, recovery target/progress, direct bounce endpoint | the helper that produces them; later consumers read them without reclassification |

## Static/native gaps and dynamic policy

The collision/recovery implementation now follows the recovered connected
ordering: bounce geometry and 10/70 selection, movement query, support sweep,
face-bit exit predicate, response-before-orientation, shared position writer,
and air/landing handoff. No frame-number rule or new recorder hook was added.
The replay adapter's ollie mapping consumes the existing generic random-call
records at the statically identified `0x0049a280` callsites; it does not add a
causal channel. The outer threshold call at static `0x0049eae9` is likewise
fed from the existing shared-service observation recorded at its return-site
alias `0x0049eaed`; the normal decay call remains `0x0049eb25`. This preserves
the retail `+0xdc` threshold replacement without treating the post-frame
threshold as an input.

## Verification checkpoint

The current native replay matrix establishes the following boundary for this
static chunk:

| recording | result |
|---|---|
| `warehouse-idle` | 256/256 strict frames |
| `warehouse-straight` | 256/256 strict frames |
| qualification `warehouse-straight` | 256/256 strict frames |
| qualification `warehouse-ollie-land` | 256/256 strict frames |
| qualification `warehouse-turn-ollie` | first mismatch at frame 20, position |
| qualification `warehouse-turn` | first mismatch at frame 20, collision response |

The two turn fixtures share the same grounded steering/turn producer boundary.
At frame 20 the `warehouse-turn-ollie` retail position is
`[23929391,-5902336,11568131]`, while native is
`[23929460,-5902336,11568128]`; the turn accumulator and basis have already
diverged in that producer. The non-ollie turn fixture retains the same position
through its first mismatch but differs in the derived response by
`[1561,0,130401]` versus `[1560,0,130419]`. Static evidence currently places
both differences in the grounded turn/fixed-point producer boundary, before a
new collision/recovery branch is justified.

The remaining qualification mismatches are outside the already matching
idle/straight collision frontier:

* The `warehouse-ollie-land` mismatch previously observed at frame 52 is
  resolved: the static `0x0049eae9` `+0xdc` threshold draw was associated with
  its existing `0x0049eaed` observer alias, and the recording now matches all
  256 frames. The selected contact at the former boundary was the flat ground
  normal, confirming that no collision class or recorder channel was missing.
* the turn fixtures first diverge at frame 20 in the grounded steering/turn
  producer and its fixed-point response rounding, before a collision recovery
  branch can explain the difference.
* the outer `0x00490730` floor/restart helper is statically bounded but only
  partially represented by the renderer-independent native frame; its
  vertical and fallback query ownership remains an explicit adjacent seam.

These are narrow static questions: complete the `0x0049b010` animation,
profile/table, and `0x0049ca8f` service contract outside the qualified
Warehouse path; identify the grounded turn fixed-point producer; and finish
the `0x00490730` caller handoff. Dynamic debugging is justified only if static
bytes leave two plausible call ordering or indirect-dispatch interpretations.
Replay remains the verification oracle, not the source of new collision
behavior.
