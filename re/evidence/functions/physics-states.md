# Physics state machine: standing → ollie → airborne → landing

Session B was run against the Warehouse level (level index 12) in the dedicated
worktree `/home/joao/dev/OpenTony-physics-states`, using canonical retail build
SHA256 `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`.
Runtime launches use the generated `THawk2.nocd.exe`; its current SHA256 is
`03d5ba74dbc909e3a417f1f5b854a836c8c9e8bd31e9d1c331f01ab613a94778` and it
differs from the canonical image only at the six-byte CD-check patch described
in `re/evidence/cd-check.md`. Addresses and static decompilation here are for
the canonical image; runtime observations use that no-CD derivative.

Primary bounded trace:

`build/debug/sessions/physics-states7/ollie.trace.ndjson`

Companion zero-mask state-0 baseline:

`build/debug/sessions/physics-states6/ollie.trace.ndjson`, frames 653–685
(state `0`, action mask `0`). This is a state baseline, not a stationarity
claim; the position was still changing in that run.

The player in this trace is `0x05f39530`; the live `Player` global is
`0x0056a858`.

Independent frontend-driven corroboration:

`build/debug/sessions/physics-states17/ollie.trace.ndjson`

This second run reached Warehouse through the normal frontend and loaded the
entry, request, writer, dispatcher, and position probes together. It contains
runtime entries at `0x00497f40` with raw state `1`, and repeated live
`1 → 0` requests from the in-air contact path (`0x00499203` return address,
exact request instruction `0x004991fe`, reason `0x1fd6`). It also records the
configured action edge at `0x00489a15`. The player was already interacting
with Warehouse geometry when that run was started, so its intermediate
`0 → 2 → 4 → 1 → 0` path is corroboration of the handler/landing wiring, not a
replacement for the clean bounded ollie sequence below.

## Decisive action-to-ollie comparison

Two controlled Warehouse runs use the same player (`0x05f39530`), the same
post-poll injection point (`0x00489a15`), and the same delayed edge/hold
window. They separate the configured JUMP action from the action record that
actually reaches the prephysics ollie gate:

| Trace | Injected action | Direct launch result |
| --- | --- | --- |
| `build/debug/sessions/physics-states32/jump.trace.ndjson` | JUMP, mask `0x0010`, record base `+0x00`, edge at physics frame `620` | The record becomes held at frame `621`, but no `0x0049a751` latch-consume event and no causal raw-state `1` follow. The later `0 → 2 → 0` path is an ordinary collision transient. |
| `build/debug/sessions/physics-states33/kick.trace.ndjson` | KICK, mask `0x0040`, record `+0x30`, edge at physics frame `620` | The held KICK record reaches `Skater_PrePhysicsOllie`; the launch latch is consumed at frame `660`, raw state `0 → 1`, and the player enters `0x00497f40`. |

The negative JUMP result is as important as the positive KICK result. The
configured keyboard binding and bit are confirmed, but this direct action-mask
experiment does not show that bit arming `player+0x2de0`. In the observed
retail path, the direct prephysics charge/latch input is the KICK subrecord;
the upstream gameplay mapping, animation gate, or naming that makes this the
user-facing ollie action remains a separate seam.

## Working state table

| Raw `player+0x30b8` | Runtime behavior in controlled captures | Dispatcher case / handler | Confidence |
| ---: | --- | --- | --- |
| `0` (`PHYSICS_ON_GROUND`) | Normal grounded/standing path; repeated position commits with no state transition. The separate five-sample Warehouse idle baseline is documented in `game-loop.md`; ordinary movement also remains on this raw state. | `0x0049db80` case 0; ground helpers `0x0049dad0`, `0x00496550`, `0x00495cc0`, `0x0049d9c0`; common commit callsite `0x0049f0e5` | Runtime observed; semantic label cross-build corroborated |
| `1` (`PHYSICS_IN_AIR`) | The airborne interval in the bounded ollie: position advances over several updates, then returns to `0` | `0x0049db80` case 1 → `0x00497f40` (`Skater_DoPhysicsInAir`); static case 3 also calls the same handler | Runtime observed; semantic label cross-build corroborated |
| `2` (`PHYSICS_ON_INVISIBLE`) | Short transient ground/collision path seen in repeated Warehouse captures; paired with `player+0x30c4` becoming `1`, then returning to `0` | Case 2 sets `+0x30c4=1` and calls `0x00496550` | Raw value runtime observed; semantic label inferred from matching enum order/handler role |
| `3` (`PHYSICS_IN_AIR_STICK_TO`) | Not reproduced in the bounded sequence; separate alternate in-air value, not collapsed into state 1 | `0x00497f40` (same in-air handler as case 1); after `frame - +0x2da0 > 0x19`, call `0x0049de9e` requests state 1 with reason `0x2bf2` | Static; semantic label cross-build corroborated |
| `4` (`PHYSICS_ON_RAIL`) | Reproduced in the independent frontend probe as a prolonged transient path after the injected edge; rail semantics come from the matching handler slot | `0x0049db80` case 4 → `0x00494210` | Raw value runtime observed; semantic label cross-build inferred |
| `5` (`PHYSICS_IN_WALLRIDE`) | Not reproduced in the bounded sequence | `0x00499710` | Static; semantic label cross-build inferred |
| `6` (`PHYSICS_IN_FOOTPLANT`) | Not reproduced in the bounded sequence; static case 6 reaches the common in-air handler and therefore uses the ordinary landing branch (the launch-grace exception is raw 3 only) | `0x004993f0`, then case 1 → `0x00497f40` | Static; semantic label cross-build inferred |
| `7` (`PHYSICS_STOPPED`) | Not reproduced in the bounded sequence; handler restores live position from `+0xbc/+0xc0/+0xc4` before the final ground helper | Ground helpers, then position copy from `+0xbc/+0xc0/+0xc4` | Static; semantic label cross-build inferred |
| `8` (`PHYSICS_IN_HANDPLANT`) | Not reproduced in the bounded sequence | `0x004995d0` | Static; semantic label cross-build inferred |

The PC executable does not carry a directly usable `EPhysicsState` symbol in
the current import, so these labels remain a cross-build semantic layer rather
than a replacement for the raw replay key. The bundled PSX symbol data lists
the constants in `physics.h` lines 20–28 as `PHYSICS_ON_GROUND` through
`PHYSICS_IN_HANDPLANT` in exactly the numeric order above. The PC dispatcher
has the matching eight cases: ground helper sequence, common in-air handler,
collision-transient helper, shared in-air handler, rail handler, wallride
handler, footplant pre-air setup plus in-air fallthrough, stopped position
restore, and handplant handler. That convergence is strong enough for
provisional labels, while runtime captures still remain authoritative for the
PC raw values and transitions.

Values seen in other captures, especially `2`, are therefore not erased or
renumbered in the native model. `2` is retained as a collision-transient
dispatch kind while `classify_physics_state()` exposes the cross-build
`PHYSICS_ON_INVISIBLE` label separately.
Raw `2` is nevertheless a reproducible dispatcher state: the JUMP-only
negative trace repeatedly enters it from raw `0` through the ground-collision
request call at `0x004972da` with reason `0x1ac9` (the runtime probe's
post-call return address is `0x004972df`), runs case 2 through `0x00496550`,
and returns to raw `0` through request call `0x00497479` with reason `0x1b19`.
During the case-2 interval `+0x30c4` is written `0 → 1` by the dispatcher and later cleared;
the native model therefore retains both the `CollisionTransient` dispatch kind
and the auxiliary field without assigning a final gameplay enum name. The native
`enter_collision_transient()` and `exit_collision_transient()` methods preserve
the observed request reasons and callsites while leaving the collision response
that decides when to invoke them outside the state machine.

### Additional grounded leave-ground transition

The case-0 tail contains a second, non-ollie path from raw state `0` to raw
state `1`. After the grounded helper sequence has run, the canonical
instructions at `0x0049dd6b`–`0x0049dd91` require the state still to be `0`,
`player+0x3110 > 1000`, and either `player+0x3130 > 0x5000` or
`DAT_005685f4 - player+0x2d98 < (DAT_0056865c * 6) >> 8`. The request is made
at callsite `0x0049ddcf` with reason `0x2ba1`; the immediately following store
at `0x0049ddd9` clamps `player+0x50` to zero when it is negative.

This is distinct from the KICK-gated ollie request at `0x0049ac9b`: it is a
post-ground-helper leave-ground condition, with slope/recovery/frame values
supplied by collision and outer-frame code. The native
`try_ground_to_air()` boundary models the predicate, request metadata, and
vertical clamp without claiming ownership of those producers. After that
request it now also models the shared `FUN_004904d0(0x14, 0)` call at
`0x0049dde1`: the helper resets its deterministic player/trick fields,
re-requests raw state `1` with reason `0x0715` at `0x004905ab`, and the caller
writes `+0x3204 = 0x28` at `0x0049dde6`. The second request is a same-state
write, so it matters for phase/history and writer traces even though it is not
a new semantic transition. Animation, sound, and speed-table side effects
inside `FUN_004904d0` remain explicit external seams. It is covered by a
focused native regression test, but no additional semantic “jump” name is
assigned to this transition.

### Shared off-ground reset (`0x004904d0`)

The helper is broader than the state-4 label. Its two arguments are the
mode/speed selector and the value copied to `+0x2f60`; the deterministic
player-side stores are now retained in the native `OffGroundBookkeeping`
record using raw offset names where semantics are not established:

| Retail fields | Reset performed |
| --- | --- |
| `+0x2c64`, `+0x2c8c` | zeroed |
| `+0x302c` | incremented |
| `+0x29dc`, `+0x29e0`, `+0x2bd8`, `+0x2ec8`, `+0x2e90`, `+0x29d4`, `+0x29d0` | zeroed |
| `+0x29f0`, `+0x29f4` | set to `0`, `100` |
| `+0x2dd0`, `+0x2dd4`, `+0x29c8` | zeroed |
| `+0x2f60`, `+0x2f68`, `+0x2f64` | copy second argument, zero, set to `1` |
| `+0x2de0`, `+0x2de8`, `+0x2c68`, `+0x2e94` | zeroed |
| `+0x29f2`, `+0x3034` | consumed as the recovery mode, then cleared/incremented by the nested landing-recovery branch |

It then requests raw state `1` through `0x004900b0`, with reason `0x0715`
and request instruction `0x004905ab`. The mode-dependent calls to
`0x004de010`, `0x0048fb20`, `0x004904c0`, `0x00491b80`, and `0x004be450`
remain external because they drive animation, sound, or speed-table state.
This prevents the native transition from silently treating those effects as
part of the verified physics enum.

### Observed state-4 collision path

The independent frontend trace also contains one bounded raw transition chain
that is different from the clean KICK ollie: `0 → 2 → 4 → 1 → 0`. The request
records are duplicated because the trace has both request and writer probes
armed, but the ordered state changes and metadata are unambiguous:

| Raw transition | Request instruction | Reason | Runtime probe return | Dispatcher handler |
| --- | ---: | ---: | ---: | --- |
| `0 → 2` | `0x004972da` | `0x1ac9` | `0x004972df` | case 2 → `0x00496550` |
| `2 → 4` | `0x004913dd` | `0x0b1c` | `0x004913e2` | case 4 → `0x00494210` |
| `4 → 1` | `0x004905ab` | `0x0715` | `0x004905b0` | case 1 → `0x00497f40` |
| `1 → 0` | `0x004991fe` | `0x1fd6` | `0x00499203` | accepted in-air contact |

The request helper's state writer is `0x004902bf` for all four changes. The
five-byte difference between each exact call instruction and the recorded
return value is the x86 `call rel32` instruction length; the return values
must not be recorded as alternate writers. The final `1 → 0` branch commits
the contact position through `0x00496060` before the request, as in the clean
ollie path.

Raw state `4` is provisionally aligned with `PHYSICS_ON_RAIL` because the
dispatcher selects the dedicated `0x00494210` handler and the bundled PSX
`EPhysicsState` order puts rail at value 4. This trace is strong evidence for
the raw transition path, but it does not by itself prove that every state-4
entry is a rail entry or that `0x00494210` is fully equivalent across builds.
The native `enter_state4_from_collision()` and `leave_state4_to_air()` methods
therefore preserve the exact request and shared off-ground reset boundaries;
rail geometry,
collision selection, and the state-4 handler's large orientation/contact body
remain explicit caller-owned seams.

### Static case-6 pre-air setup

Dispatcher case `6` is not just a second label for case `1`. Its first callee,
`0x004993f0`, computes `player+0x2de8 = 0xf - (rand(2)+rand(0))/0x14`,
requests raw state `1` with reason `0x2058` at callsite `0x00499445`, and
sets `player+0x2ddc = 1`; the switch then falls through to the common
`0x00497f40` handler. Before the fallthrough it measures the preexisting
velocity after `>>12`, replaces velocity with the negated `+0x30f4` basis times
that integer length, adds five basis units to X/Z, and applies three more
type-2 random draws to Y. In instruction order, the Y terms are equivalent to
`A = (3300*(r1+0x21c)+0x166e30)/10000`,
`B = (3300*r2+0x166e30)/10000`,
`C = (-5000*(r3+0x21c))/10000`, followed by
`Y += 3 * ((C*0x400)/10 - (((A-B)*charge*0x400)/10)/charge)` with the
intermediate integer divisions preserved. The native
`run_state6_preair_setup()` models those operations with the random draws
explicit, without treating case 6 as an ordinary KICK launch.

The first grounded action stage is now partly recovered: `0x00493370`'s
fixed-point target step, clamp, decay, brake mode, and velocity-damping branch
are implemented in the native reference. The later heading/animation helper
`0x00492f20`, ground acceleration preparation, and collision response remain
separate seams because they consume orientation/surface fields not yet
validated by a same-state replay.

## Exact writers

### `player+0x30b8`

The transition helper begins at `0x004900b0`. Its actual state store is:

```asm
0x004902b9  mov edx,[esi+0x30b8]
0x004902bf  mov [esi+0x30b8],ebp    ; actual writer
0x004902c5  mov [esi+0x30c0],edx
```

The hardware watchpoint reports the post-store PC `0x004902c5`. In the bounded
sequence this writer performed:

- frame 659: `0 → 1`
- frame 666: `1 → 0`

Thus the exact code writer is `0x004902bf`; `0x004902c5` is the debugger trap
location, not the instruction that changes the state.

That statement is scoped to the normal gameplay request path. A static scan of
the canonical executable finds these additional direct stores to the same
field:

| Writer PC | Context | Values/role | Scope |
| --- | --- | --- | --- |
| `0x0046c9cb` | `FUN_0046c720` skater construction | writes `0` while initializing the object | lifecycle |
| `0x0044f332` | `FUN_0044eac0` frontend/restart reset loop | writes `0` for live skaters and clears related reset fields | lifecycle |
| `0x004beb1a` | `FUN_004be450`, called by `0x00492ea0` for a nonzero `+0x29c8` action record | writes raw `8` in a special-action command branch | special-action path |
| `0x004c6b90` | `FUN_004c5dc0` script/trigger reset sequence | writes `0` while rebuilding a skater object | script/lifecycle |
| `0x004cd041` | `FUN_004ccd70` replay frame decode | copies serialized state field `4` into `+0x30b8` | replay |
| `0x004df022` | `FUN_004deff0` network teardown/reset | clears the network player's state | network |
| `0x004e0c01` | `FUN_004e02d0` network timeout | clears the active network player's state | network |
| `0x004e0cd1` | `FUN_004e02d0` network cleanup | clears the second network player's state | network |
| `0x004e107e` | `FUN_004e0f20` network frame decode | copies a received state field into `+0x30b8` | network |

These stores were resolved statically from the binary and were not observed in
the bounded Warehouse physics trace. The native `request_state()` models the
ordinary `0x004900b0 → 0x004902bf` gameplay path; a full recreation must keep
the lifecycle/replay/network writers as separate ingress paths rather than
turning them into additional physics transitions.

### `player+0x30c4`

The dispatcher writes this auxiliary field directly:

- `0 → 1`: actual store `0x0049dea8`; watchpoint trap `0x0049deb2`
- `1 → 0`: actual store `0x0049dd21`; watchpoint trap `0x0049dd27`

Those transitions were observed in the repeated Warehouse captures around the
case-2 path. `+0x30c4` remained `0` throughout the bounded state-0 → state-1
ollie excerpt, so it is not required to explain that particular transition.

`+0x30c4` is therefore an auxiliary dispatcher-written field, not a safe
replacement for the raw physics state. The static dispatcher writes `1` in
case 2 and writes `0` in cases 0, 4, 5, and 8. Its final semantic name is still
open.

## Jump input and action-state record

The configured keyboard binding is in `build/runtime/TH2_OPT.CFG`:

```ini
[KEYBOARD]
JUMP=PAD2,SPACE,RETURN
KICK=PAD6,B
```

`PCInput_LoadBindings` (`0x004e4d10`) parses JUMP and KICK as separate
configured controls. `PCInput_BuildActionMask` (`0x004e42c0`) emits JUMP as
bit `0x0010` and KICK as bit `0x0040` in global action mask `0x006a3f1c`. The
jump action record is at `0x0056aff8` (base `0x0056aff8`):

| Record field | Observed meaning |
| --- | --- |
| byte `+0` | held state |
| byte `+1` | press-edge latch/candidate |
| u32 `+4` | held counter; increments while held, clears on release |
| u32 `+8` | release/inactive counter; clears while held, increments after release |
| u32 `+0xc` | update counter; increments on every record update |

The action bank uses the same record layout for KICK at base `+0x30` with mask
bit `0x0040`. The KICK-only comparison was instrumented with the same held,
press-edge, release, and counter probes; its edge occurs at frame 620 and its
held counter reaches 40 before release. This is the action record consumed by
the direct prephysics charge/latch gate, not the configured JUMP record at
base `+0x00`.

The update routine is `0x00489930`. Relevant actual stores and watchpoint PCs
are:

- press edge: actual `0x0048994d`, trap `0x00489951`, byte `+1: 0→1`
- press/held: actual `0x00489963`, trap `0x00489966`, byte `+0: 0→1`
- release: actual `0x00489968`, trap `0x0048996a`, byte `+0: 1→0`

In the bounded trace:

- frame 623: injected action-mask edge; jump record became `byte0=1,
  byte1=1`
- frames 626–662: mask `0x0010`, `byte0=1`, `byte1=0`, and `u32 +4`
  advanced from 4 through 40
- frame 663: mask cleared; `byte0=0`, `byte1=0`, `u32 +4=0`, and
  `u32 +8` began incrementing

The trace records `jump_edge_injection` at `0x00489a15`, immediately after the
poll/build stage. Because headless Wine DirectInput did not accept a real
keyboard event reliably, the final capture injected the already-resolved
action bit at that point and held it for 40 action updates with a GDB helper.
This is an action-mask-level input trace, not a claim that a physical keyboard
scan was observed.

The player constructor stores the action-record pointer at `player+0x2ccc`:
`0x0046cac9` selects `0x0056aff8` for the first skater and `0x0056b164` for the
second. This closes the pointer/layout gap between the global action records
and the physics object. The in-air handler consumes that pointer directly at
`0x00497fff`: when record byte `+0` is held and u32 `+4` is greater than `2`,
it writes zero to acceleration Y (`player+0x5c`, actual writer
`0x00498009`) and velocity Y (`player+0x50`, actual writer `0x0049800c`) before
the position integration. `0x004914d0` also checks the held byte during
postphysics/landing recovery and can clear `player+0x2e30` when jump is no
longer held.

`0x0049a280` does load `player+0x2ccc`, but its charge gate reads record byte
`+0x30`, not the configured JUMP record at base `+0x00`. The action updater's
mapping makes that subrecord the KICK record (`0x00489a10` consumes mask bit
`0x0040` there). The exact charge/latch gate is therefore a distinct action
path: KICK-held and `+0x2f64==0` lead to the charge counter, while the
JUMP-held effect above is an in-air consumer of the base record. The remaining
upstream handoff that maps the user-facing configured JUMP concept to this
action path is still an explicit seam rather than an invented direct
jump-bit-to-latch write. The controlled pair above makes the runtime boundary
stronger: JUMP-only at frame 620 produces no launch-latch consume, whereas
KICK-only at the same frame does.

### JUMP-record consumer audit

The static reference sweep does not find a hidden JUMP-to-KICK alias. The base
JUMP record has these distinct consumers:

| Consumer | Record access | Effect |
| --- | --- | --- |
| `0x00489a10` → `0x00489930` | base `+0x00`, mask `0x0010` | updates held/edge/release counters; no player-physics write |
| `0x00492190` → `0x00491c90` | base `+0x00` byte `+0` at `0x00492254`–`0x0049225e` | emits action-history event index `0x0c` into the ring at `+0x2a14`, with pressed state and frame timestamp |
| `0x00497f40` | base `+0x00` byte `+0` and u32 `+4` at `0x00497fff` | when held and counter `>2`, clears velocity/acceleration Y at `0x0049800c`/`0x00498009` |
| `0x004914d0` | base `+0x00` byte `+0` at `0x004914e6` | postphysics cleanup can clear `+0x2e30` when jump is no longer held |

By comparison, `0x0049a280` reads the action pointer at `player+0x2ccc` but
tests its `+0x30` byte, which is the independently mapped KICK record. The
other nearby action-history inputs are also separate records: GRAB `+0x20`,
GRIND `+0x10`, NOLLIE `+0x50`, SWITCH `+0x70`, and the spin/directional slots.
This closes the JUMP-record consumer audit without claiming that the later
action-history/trick system cannot make KICK the user-facing ollie control.

### Action-history ring (`0x00492190` → `0x00491c90`)

The action-history path is now modeled as a separate, exact bookkeeping seam.
`FUN_00492190` calls `FUN_00491c90` in this order: indices `1` through `8`
compare the opaque `FUN_00492120` result, then indices `9`, `10`, `11`, `12`,
`14`, and `16` read GRAB, GRIND, KICK, JUMP, NOLLIE, and SWITCH respectively.
Indices `0`, `13`, and `15` are not touched by this updater. The writer
compares each value against the previous byte at `+0x2b18 + index`; unchanged
values do not create events. A changed value writes index, pressed byte, and
`DAT_005685f4` to the next eight-byte slot at `+0x2a14 + ring_index*8`, then
increments `+0x2b14` modulo `0x20`.

The native `update_action_history()` preserves this call order and ring
behavior. Its `physics_action` argument is deliberately the unresolved
`FUN_00492120` result, so this addition captures action edges without claiming
that the producer is the jump/ollie transition. The regression covers a
physics-action change, KICK release, and JUMP press with their exact event
indices and frame timestamps.

## Dispatcher, handler, and position commit

The dispatcher entry is `0x0049db80`. Static case selection and the dynamic
trace agree on the important part of the bounded sequence:

```text
state 0: 0x0049db80 case 0 → grounded helper set
state 1: 0x0049db80 case 1 → 0x00497f40
```

`0x00497f40` is the in-air handler candidate (`Skater_DoPhysicsInAir`). The
decisive KICK trace contains dispatcher records with
`dispatcher_case=1` and `handler_candidates=["0x00497f40"]` for frames
661–680, immediately after the launch request. The earlier bounded trace and
the independent frontend run corroborate the same callee entry. The position
commit routine is `0x00496060`; the common observed callsite in the historical
sequence is `0x0049f0e5`. A second relevant static callsite inside the in-air
code path is `0x0049917b`, which appears at the landing/recovery boundary in
the earlier capture set.

The exact air-to-ground request is also present in the in-air handler itself,
not only in the separate ground helper. `Skater_DoPhysicsInAir` first accepts a
contact result after its collision tests, commits the contact position through
`0x00496060`, then reaches the call instruction at `0x004991fe`:

```asm
; accepted contact branch in Skater_DoPhysicsInAir
call 0x004914d0
mov  [player+0x30b0], ...
push 0x1fd6              ; landing/contact reason
push 0                   ; requested raw state
call 0x004900b0          ; state request helper
```

For the bounded raw-state-1 ollie this is the code-level landing predicate:
the collision result is accepted, contact position is committed, and the
request helper changes `player+0x30b8` from `1` to `0`. The exact store remains
`0x004902bf`; the watchpoint trap at `0x004902c5` is the following instruction.
The ground/collision helper has additional recovery requests, but it is not
necessary to explain this observed air → ground transition.

The static contact branch also exposes a small landing handoff that the native
slice preserves: ordinary accepted contact sets `+0x2ec0 = 1`, calls the
landing cleanup helper, then clears the shared movement target `+0x3144`,
clears `+0x29dc`, stores the landing frame at `+0x2d98`, and copies a
collision-side contact identity into `+0x30b0`. The exact writer PCs are
`0x004991a4`, `0x004991b3`, `0x004991d4`, `0x004991f8`, and `0x00499255`,
respectively. The collision identity and contact-normal work remain
callback-owned. For raw state `3`, contact
position is still committed, but the normal state-0 request is deferred while
`frame - +0x2f34 <= 0x1e`; this launch-grace distinction is now covered by the
native replay test.

### Landing cleanup (`0x004914d0`)

`FUN_004914d0` is both an in-air landing-branch helper and an outer-frame
post-dispatch helper (called every frame except raw state `8`). It begins by
calling the collision/grounding preparation routine `0x004aaf70`; that routine
is kept as a caller-owned callback because its broadphase and cast selection
are outside this state-machine slice. Its deterministic cleanup boundary is
now explicit in the native model. It first clears `+0x2e34` when JUMP is
released, clears `+0x2e30` when JUMP is released and `+0x2e90` is set,
decrements nonzero `+0x2ec4`, and then consumes a positive landing marker at
`+0x2ec0`: the marker is decremented and `+0x2e90`/`+0x2e94` are cleared before
the contact identity or landing request is published. This is why the native
accepted contact path calls `apply_landing_cleanup()` immediately after
writing `+0x2ec0 = 1`.

After the marker drains, the exact early gates clear `+0x2e94` when JUMP is
not held, control is blocked (`+0x2f64`), raw state `2` has positive Y
velocity, or `+0x2e34` is set. The deeper recovery/animation dispatch uses
external animation and trick state and remains outside the native boundary.
The outer-frame callback is placed after dispatch/blocked cleanup and before
the final velocity integration; the native regression covers that ordering.

The post-marker recovery decision is now split at its deterministic seam. An
angle-window flag is true when `+0x2c88` is set and the low 12 bits of
`+0x2c8c` are above `0x12c` for nonpositive `+0x2c90`, or below `0xed4` for
positive `+0x2c90`. That flag, or the condition `+0x2c68 != 0` with
`+0x2c6c == 0`, enters the recovery branch. If `+0x2e2c == 0`, or its signed
byte/short comparison against `+0x100`, `+0x101`, and `+0xf4` passes, the
branch calls `FUN_004904d0` at `0x004916a9`, clears short `+0x29f2`, increments
`+0x3034`, and returns. The native result exposes this as
`recovery_reset_requested` and retains the nested off-ground request.

If that reset is not selected, `+0x2c70` is set when `+0x2c68` is nonzero, then
`FUN_004925e0` scans the action-history/trick data. A nonzero `+0x3064` would
dispatch `FUN_00490ef0` with a deterministic geometry hint. The native model
reports both external calls and the hint but leaves their action/animation
effects caller-owned.

### Raw in-air contact result

The supplemental capture
`build/debug/sessions/physics-contact2/air-collision.trace.ndjson` arms the
bounded `tony-air-collision-probe` at `0x00498a7d`. The static call sequence is:

```text
0x00498a4e  call 0x004624d0   ; initialize cast record
0x00498a61  call 0x00466090   ; run collision query
0x00498a6e  call 0x0048ea80   ; decode material/context flags
0x00498a73  load result into EAX
0x00498a7a  add esp, 0x1c
0x00498a7d  cmp EAX, 0        ; probe boundary
```

At the probe boundary, EAX is the raw local result loaded from the pre-cleanup
`[ESP+0x10c]`; after the cleanup the same local is `[ESP+0xf0]`. It is not a
boolean in the retail representation. In no-contact updates both values are
zero. On the observed Warehouse contact updates they are the same nonzero,
pointer-like value, for example `0x05f365f8`, and the handler then reaches the
existing accepted-contact call at `0x004991fe` with reason `0x1fd6`. The probe
does not call this value a geometry pointer or a stable contact identity; it is
only a raw result token until repeated object ownership is established.

The probe also records the material/context globals populated by
`0x0048ea80`: `0x0056b768`, `0x0056b7a8`, `0x0056b7ac`, `0x0056b7b8`, and
`0x0056b7e8`, plus a bounded raw stack window containing the cast payload. The
ordinary landing branch's predicate is now explicit in the native reference:
it first requires a nonzero result, then applies the material/blocked/jump
release/recent-surface gate, and finally requires the transient material flag
(or the raw-state-3 alternate material case). The native helper is
`standard_air_landing_accepted()`; collision geometry selection and the
special wallride/contact alternatives remain outside it.

The native `accept_standard_air_collision()` boundary composes that predicate
with the accepted-contact path: it supplies the live raw state, blocked flag,
JUMP held/inactive fields, and frame counter, then sets the supplied contact as
accepted only when the predicate passes. The contact path commits position
before publishing the decoded `material_type` as the contact identity and
requesting raw state `0`, so
the collision selector and its payload ownership remain caller-owned.
Because the accepted branch jumps to `LAB_004992ff`, it also skips the common
`FUN_004ca9f0(+0x2da8)` gravity-add fallthrough at `0x004992f0` on that frame;
the native frame loop preserves this landing-frame distinction.

The selected-result recovery helper at `0x00497aa0` is a separate contact
path. If the launch mode latch `+0x2db8` is still set and the recovery window
`+0x2d8c` is zero/recent (within four frames), or the contact material flag
`0x0056b7ac` is set, it requests raw state `2` with reason `0x1caa`; the
request call instruction is `0x00497ad9`. Otherwise it requests raw state `1`
with reason `0x1cb1` at call instruction `0x00497aec`. On the latter path,
held UP or heading deadband below `-0x28` subtracts the selected result's
signed-short X/Z direction scaled by `(floor(length(velocity))*0x40)>>2`.
It then clears `+0x2db4` and sets `+0x2ddc` when the recovery latch
`+0x2c68` permits it. The native
`handle_air_collision_recovery()` exposes these writes and request callsites;
the selected collision result remains caller-owned.

The outer frame maintains the recovery timestamp used by that branch at
`+0x2d8c`. At the frame-start condition in `0x0049e680`, it clears the field
when UP is not held while heading deadband is above `-0x29`, when Left or
Right is held, or when `abs(+0x31a1) > 0x31`. Otherwise it initializes the
field once to the current frame. The native
`update_collision_recovery_window()` mirrors this signed-byte gate, and
`step_frame()` updates it after the native frame counter advances.

This separates three facts that should not be conflated in a recreation:
the cast result exists, the ordinary landing predicate accepts it, and the
accepted branch requests raw state `0`. The controlled KICK trace supplies the
launch edge and raw `0→1`; the supplemental probe supplies the raw contact
result and the following `1→0` correlation.

The in-air assignment is directly observed in the controlled KICK run and the
independent frontend run: the breakpoint at `0x00497f40` records
`function=Skater_DoPhysicsInAir` while raw state is `1`, with changing
position samples. The KICK run supplies the action-to-launch causality; the
older clean bounded trace supplies the same ground → air → ground shape but
cannot by itself be used to attribute the transition to the configured JUMP
bit.

The distinction between raw states 1 and 3 matters for a recreation: the
dispatcher has two separate cases, but both call `0x00497f40`. Case 3 also
checks a frame counter and requests state 1 with reason `0x2bf2` if the raw
state remains 3 for more than `0x19` updates. Do not collapse the two raw
values merely because they share the in-air callee. The ordering is also
material: the case-3 handler call returns first, then the dispatcher tests
whether raw state is still `3` before issuing the timeout request at
`0x0049de9e`. The native standalone `dispatch()` closes that post-handler
check immediately; `step_frame()` defers it until after the common air
callbacks, matching the retail handler-before-timeout boundary.

## Transition callers and reason tags

`0x004900b0` is the `__thiscall` state-request helper. At entry its first
stack argument is the requested raw state and its second is a callsite reason
tag. The helper commits through `0x004902bf`, then copies the old state to
`+0x30c0`. The native `StateRequest` keeps the invoking call instruction
separate from the helper entry, so a trace can distinguish (for example)
launch callsite `0x0049ac9b` from landing callsite `0x004991fe`. The reason
values below are recovered from static callsites; they
are useful trace labels, not final semantic enum names.

| Caller / path | Requested state | Reason tag | Static interpretation |
| --- | ---: | ---: | --- |
| `0x0049a280` ollie/trick prephysics, call `0x0049ac9b` | `1` | `0x245c` | ordinary launch/trick path candidate |
| `0x0049a280` special ollie branch, call `0x0049ac7f` | `3` | `0x2457` | alternate launch path when the prephysics condition is set |
| `0x00496550` ground/collision response | `1` | `0x1ab6` | collision response / leave-ground candidate |
| `0x00496550` ground/collision response | `1` | `0x1ae4` | steep vertical condition (`+0x2d9c < -0x400`) |
| `0x00496550` ground/collision response, request call `0x004972da` | `2` | `0x1ac9` | transient collision-response path |
| `0x00496550` ground/collision response, request call `0x00497479` | `0` | `0x19bf`, `0x1b19` | ground recovery paths |
| `0x00497f40` in-air contact branch | `0` | `0x1fd6` | accepted contact after collision tests; exact call `0x004991fe` |
| `0x0049db80` dispatcher case 0 | `1` | `0x2ba1` | terminal/ground condition that requests a leave-ground state |
| `0x0049db80` dispatcher case 3 timeout | `1` | `0x2bf2` | raw state-3 timeout recovery |
| `0x0049df00` ground-physics path | `0` | `0x2c0f`, `0x2c21` | state-7 recovery paths |
| `0x0049df00` ground-physics path | `7` | `0x2c56` | special ground/rail transition candidate |

The complete static call scan also finds request sites outside the bounded
ollie and collision captures. The high-confidence constant-target sites are
kept here as ingress inventory; they are not silently merged into the minimal
ground/air machine:

| Request callsite | Target | Reason | Static context |
| ---: | ---: | ---: | --- |
| `0x00490e8a` | `8` | `0x09b6` | special-action path; separate from normal frame physics |
| `0x0049418b` | `1` | `0x1370` | dedicated state-recovery helper |
| `0x00495703` | `1` | `0x1605` | `FUN_004956f0`, when current raw state is `2` |
| `0x00495714` | `1` | `0x160b` | `FUN_004956f0`, complementary branch |
| `0x00498c6c` | `5` | `0x1eca` | in-air wallride entry |
| `0x0049906f` | runtime-selected (`EDI`) | `0x1f83` | in-air contact/recovery branch; current decomp resolves the active path to `1` |

The scan also finds dynamic-target request sites at `0x004952fc` (target in
`EDI`, reason `0x1584`), `0x0049a058` (target in `ESI`, reason `0x21c1`), and
`0x0049a13a` (target in `ESI`, reason `0x21eb`). These remain generic
`request_state()` inputs in the native model because the requested value is
selected by the collision/action result at runtime. All ordinary gameplay
requests in this inventory still commit through the common writer
`0x004902bf`; direct lifecycle/replay/network stores listed above are a
separate writer class.

The decisive dynamic KICK trace supplies the following exact causal slice:

```text
frame 620: KICK edge, mask 0x0040, action record +0x30 held
frame 660: 0x0049a751 consumes +0x2de0 (pre-value 1), charge = 12
           request helper callsite 0x0049aca0 requests raw state 1,
           reason 0x245c
frame 660: 0x004902bf writes raw state 0 → 1
frames 661–680: dispatcher case 1 → 0x00497f40
frame 681: 0x004991fe requests raw state 0, reason 0x1fd6
frame 681: 0x004902bf writes raw state 1 → 0
```

The release ordering is significant: at the exact `0x0049a751` hit, the
probe sees KICK byte `+0=0` and release counter `+8=1`, while the dispatcher
sample from the same frame can still observe the prephysics action-mask value
`0x0040` because it runs later in the frame. The launch is therefore a
KICK-release/prephysics event, not a KICK press-edge event.

The launch bookkeeping writers recovered from the static path are:

| Field | Meaning kept in the model | Writer PCs |
| --- | --- | --- |
| `+0x2de8` | charge | `0x0049a5b5` increment, `0x0049a623` cap, `0x0049a8d2` wallie cap, `0x0049af1a` release reset |
| `+0x2de0` | launch latch | `0x0049a640`, `0x0049a6ac` set; `0x0049a6ef` stale clear; `0x0049a729` cancel clear; `0x0049a751` consume clear |
| `+0x2dd8` | pending prephysics launch | `0x0049a6bf` set; `0x0049a6d2` release cleanup |
| `+0x3144` | grounded movement target clear during eligible charge | `0x0049a669` |
| `+0x2dec` / `+0x2ddc` | launch charge snapshot / in-progress | `0x0049a74b` / `0x0049a758` |
| `+0x2c08` | launch auxiliary reset | `0x0049a763` |
| `+0x2f30` | launch speed metric | `0x0049a777` |
| `+0x2db8` / `+0x2df4` | mode latch / wallie flag | `0x0049a851` / `0x0049a86a` |
| `+0x303c` / `+0x3040` | launch / early-release counters | `0x0049ac14` / `0x0049ac4d` |
| `+0x3068` / `+0x306c` | launch angle accumulator / turn count reset | `0x0049ac59` / `0x0049ac62` |
| `+0x2f34` | launch frame | `0x0049af14` |

The outer frame maintains latch-age timestamp `+0x2de4`: it sets it to the
current frame at `0x0049f169` when raw state is `1..3`, and clears it at
`0x0049f173` outside that range. Release cleanup at `0x0049a6ef` clears a
latch only when the timestamp is older than twenty frames. The native model
keeps these raw bookkeeping fields visible rather than collapsing them into a
single semantic “ollie state.”

The request helper's dynamic return/caller value is `0x0049aca0`; the static
launch routine is `FUN_0049a280`, with alternate and ordinary request call
instructions at `0x0049ac7f` and `0x0049ac9b` respectively. The probe's caller
field at the exact latch writer is not used as a semantic caller because that
breakpoint runs in the probe context; the state request and state-writer
records provide the causal instruction evidence. The matching JUMP-only trace
at frame 620 has no `0x0049a751` event and no launch raw-state-1 transition.

The important launch-specific static path is in `FUN_0049a280`, called by the
main physics frame at `0x0049e680`: it applies a vertical impulse to the
`+0x50` velocity component, updates ollie bookkeeping around `+0x2db4`,
`+0x2db8`, and `+0x2da0`, and then requests raw state 1 or 3. The dynamic
KICK capture now verifies this as the code-level ground → air transition:
`0x0049a751` consumes the armed latch, the request uses reason `0x245c`, and
`0x004902bf` writes `0 → 1`. The landing side is in the contact branch inside
`Skater_DoPhysicsInAir` (`0x004991fe`, reason `0x1fd6`) for the bounded ollie.
The separate ground/collision (`0x00496550`) and ground-physics
(`0x0049df00`) routines contain additional contact/recovery requests.

The launch-state choice is resolved from the static branch, rather than from
the action name: after latching `+0x2db8 = +0x2db4`, `0x0049a280` requests
ordinary raw state `1` when `+0x2db8 == 0` **or** the current raw state is
nonzero. Only the zero-state/nonzero-latched-mode case requests raw state `3`,
with reason `0x2457` and the alternate-frame write at `+0x2da0`. The native
test covers both branches. The `+0x3144` field used by the hold path is the
movement target already exposed by the grounded movement probe; it is not a
separate ollie velocity field.

## Main-frame order for a faithful recreation

The decompiled `FUN_0049e680` gives the order that should be preserved. The
first grounded action/target stage is now recovered; later orientation,
animation, and collision work remains a separate seam:

```text
main physics frame 0x0049e680
  → after ground action, clear acceleration for raw states other than 1
      → stores `0x0049e996`, `0x0049e999`, `0x0049e99e` to `+0x60/+0x5c/+0x58`
  → prephysics / trick setup 0x0049b010, then 0x0049a280
      → read KICK action subrecord (+0x30), charge/latch
      → ollie vertical impulse (+0x50)
      → request state 1/3 through 0x004900b0
  → action-history update 0x00492190 → 0x00491c90
  → remaining ground/collision preparation and recovery 0x00492ea0, 0x0049df00
  → copy phase state (+0x30c0 = +0x30b8)
  → copy current/old position history (+0x2e00..+0x2e08, +0xbc..+0xc4)
  → dispatcher 0x0049db80
      → state 0 ground path
      → state 1/3/6 in-air path 0x00497f40
  → state-3 timeout check after 0x00497f40 returns
  → control-blocked reset 0x0049d8a0, when +0x2f64 is set
      → clear acceleration X/Z and launch latches
      → optional descending-Y clamp and component velocity decay
  → landing/grounding helper 0x004914d0, except raw state 8
      → collision preparation 0x004aaf70
      → consume/clear deterministic landing and recovery markers
  → outer velocity integration 0x0049f206
  → position implementation 0x00496060
```

The frame function has two distinct uses for `+0x30c0`. The state-request helper
temporarily stores the pre-transition value there immediately after writing
`+0x30b8`; later in the same `0x0049e680` frame, the frame function writes
`+0x30c0 = +0x30b8` before entering the dispatcher. It is therefore a phase
history word, not a persistent transition-from field. A C++ port should keep a
separate trace-only `transition_from` value while reproducing the phase-word
write at the observed points. Position history likewise needs separate
old/current vectors; the in-air and landing code consults that history.

The relevant outer-frame order is now represented by
`PhysicsStateMachine::step_frame` in the native reference slice. Its callback
boundaries are intentionally named after the retail callsites:

```text
action-mask update
  → gravity/movement callbacks (0x0049e680 / 0x00493370)
  → prephysics callback (0x0049b010 / 0x0049a280)
  → action-history callback (0x00492190 / 0x00491c90)
  → ground preparation callback (0x00492ea0 / 0x0049df00)
  → +0x30c0 = +0x30b8 and position-history rotation
  → collision-start callback (0x00490730)
  → dispatcher (0x0049db80)
  → in-air preparation callback (0x00497df0)
  → air motion callback / fixed-point integration (0x004983c0)
  → optional accepted air contact: commit (0x00496060), then request state 0
  → gravity add (0x004992f0), unless accepted contact jumped over it
  → landing collision-preparation callback (0x004aaf70)
  → landing cleanup callback (0x004914d0), except raw state 8
  → velocity integration (0x0049f206)
  → postphysics callback / velocity-damping boundary (0x0049d480)
```

This is a frame contract, not a claim that the native callbacks implement
collision or the orientation-dependent remainder of grounded motion. The
fixed-point target/clamp/decay/brake stage is native; the later helper and
collision interfaces remain falsifiable follow-up targets.

### Control-blocked reset (`0x0049d8a0`)

The static main-frame order contains one additional deterministic state
boundary after the selected dispatcher handler and before the outer
acceleration-to-velocity update at `0x0049f206`. `FUN_0049d8a0` is gated by
`player+0x2f64`; when that control-blocked flag is nonzero it performs these
stores and arithmetic:

| Retail operation | Field | Effect |
| --- | --- | --- |
| clear | `+0x58`, `+0x60` | clear X/Z acceleration; Y is left to the earlier frame boundary |
| clear | `+0x2de0`, `+0x2ddc` | clear the launch latch and in-progress bookkeeping |
| conditional clear | `+0x50` | clamp negative Y velocity when `+0x2f60 == 0` or raw state is `2` |
| decay | velocity X/Y/Z | subtract `((component / +0x2c10) * dt) >> 8` when the divisor is nonzero |

The divisor, frame delta, and Y-clamp predicate are runtime inputs owned by
the outer player/frame state. The native
`apply_blocked_physics_reset()` method exposes the exact deterministic portion,
and `PhysicsFrameCallbacks::blocked_physics_reset` places it after air
contact/gravity and before velocity integration. The same retail function
continues into a recent-trick-event scan that writes `+0x108`; that event
system is deliberately not promoted into the physics state machine.

The adjacent `0x004914d0` cleanup is a separate frame boundary. The native
`apply_landing_cleanup()` mirrors its marker countdown and exact early gates;
`PhysicsFrameCallbacks::landing_collision_preparation` leaves the preceding
`0x004aaf70` broadphase/cast selection caller-owned, and
`PhysicsFrameCallbacks::landing_cleanup` places the cleanup callback after it,
after the blocked reset, and before velocity integration. Accepted common-air
contact invokes the same helper at the retail landing point, before the
identity and state-request stores. The deeper animation/recovery branch
remains caller-owned.

The native boundary now also exposes the deterministic arithmetic behind those
two callbacks. `copy_orientation_basis()` mirrors `0x0049c7d0`'s exact short
permutation into `+0x30f4..+0x3114`; `fixed12_dot()` and
`fixed12_scalar_multiply()` mirror the x87 truncate-toward-zero helpers used
by `0x004f5f90` and `0x004f5fc0`. `apply_ground_surface_acceleration()` models
the post-collision projection and coefficient selection in `0x00496550` when
the collision-owned basis/flags are supplied. `compute_gravity_acceleration()`
models the `+0x2dac` initialization at the start of `0x0049e680`, including the
raw-2 transient and global modifiers. `apply_velocity_damping()` models
`0x0049d480` with explicit cap/drag draws and the indexed low-speed gate.
`initialize_gravity_acceleration()` publishes that recovered scalar into the
stateful native machine before the caller runs ollie or in-air work.
`compute_velocity_delta()` and `integrate_velocity()` mirror the outer-frame
acceleration*dt update at `0x0049f206`; `step_frame()` exposes that point as the
`velocity_integration` callback before postphysics.
These helpers close arithmetic contracts; they do not claim to discover the
surface normal, collision identity, or shared random-stream ownership.

The postphysics cap path also consumes independent random values. In
`FUN_0049d480`, the first type-3 draw gates the cap; if active, three fresh
draws independently produce the X, Y, and Z rescale targets before the speed
metric is recomputed. The native `VelocityDampingRandom` keeps `cap_x`,
`cap_y`, and `cap_z` separate from the initial `cap`, and the regression
exercises unequal component targets. Reusing the initial target for all three
components changes both the velocity result and the shared random-stream
position.

The deterministic basis portion of the in-air preparation is now also native.
`prepare_in_air_orientation()` mirrors `0x00497df0`: it advances or resets
`+0x310c/+0x3110/+0x3114`, normalizes that rolling axis through the halve,
integer-square-root, and Q12 divide loop at `0x00465f60`, forms the next axis
with the signed-short cross-product portion of `0x004e2ff0`, normalizes it,
forms the remaining axis with the second cross product, and publishes the
resulting low-16-bit orientation values as `0x0049c850` does. The native state
retains those published shorts in `orientation_shorts` using the same
`+0x2e58, +0x2e5a, ..., +0x2e68` order accepted by `copy_orientation_basis()`.
`0x004e2070`, called after each cross product, writes a separate signed-16-bit
clamped scratch copy; it does not replace the raw cross result consumed by
these basis updates. That ordinary side effect is exposed as
`clamp_cross_scratch_to_int16()`. Its exceptional/shared-global angle state is
still caller-owned and is not silently claimed by the pure `fixed12_cross()`
helper.

### Orientation recovery/rebuild (`0x0049d080`)

The same basis machinery has a deterministic recovery helper used by the
ground/collision paths and by special-contact branches in the in-air handler.
`FUN_0049d080` is gated by `player+0x3130 < 0x18001`. It reads a signed-short
target from `+0x80/+0x82/+0x84`; unless the progress value is exactly
`0x18000`, it approaches the prior full-width base at
`+0x3134/+0x3138/+0x313c` with `(target - base) >> 2 + base`, then normalizes
the result through `0x00465f60`.

It takes the current object `+0x2e5c/+0x2e62/+0x2e68` axis, forms and
normalizes `current × target`, then forms `target × first_axis`. The first
cross becomes `+0x3100`, the target becomes `+0x310c`, and the second cross
becomes `+0x30f4`; the corresponding object shorts are published in the
usual `+0x2e58, +0x2e5a, ..., +0x2e68` order. The normalized target is also
written back to the recovery base. The native
`apply_orientation_recovery()` method models these writes while leaving
collision/heading target selection and the shared `0x004e2070` exceptional
state outside the method.

### Upright correction (`0x0049c330`)

The common air handler calls `0x0049c330` after its motion/effect bookkeeping
and before the next collision query, but only when `player+0x30b8 != 2` and
`player+0x30c4 == 0`. The routine crosses the signed-short `+0x30f4` axis with
the shared global up vector `DAT_0056b7c0..DAT_0056b7c8`, then dots that result with `+0x310c`. A result
above `0x29` builds the small `Z=0x000b` turn; a result below `-0x29` builds the
wrapped `Z=0x0ff5` turn. Otherwise it only republishes the existing
orientation.

The angle helper uses the retail turn scale `angle * 2*pi / 0x1000` and x87
truncate-toward-zero conversion. For these two fixed angles the Q12 rotation
matrix is `cos=4095`, `sin=+69` or `-69`, so the native method can reproduce the
three `0x004e3130` matrix-vector products without introducing host floating
point into the state update. It then runs the same `0x0049c7d0` short-to-basis
permutation. The global up vector and callback timing remain caller-owned;
`apply_upright_correction()` exposes the deterministic state update.

### Packed collision-normal projection

Static disassembly resolves the call convention that the decompiler left
ambiguous around `0x00490610`. Its first argument is the velocity vector; the
next two arguments are packed collision words. The helper decodes them as:

```text
normal.x = sign_extend16(word0 & 0xffff)
normal.y = sign_extend16(word0 >> 16)
normal.z = sign_extend16(word1 & 0xffff)
```

It computes the Q12 dot product, subtracts the three Q12 scalar products from
the velocity in place, and does not normalize the packed normal first. The
caller `0x00490680` measures the original length, applies `0x00490610`, then
rescales the projected vector component-wise by the integer ratio of original
to projected lengths (both lengths are `floor(sqrt(dot(v,v))) * 0x40`,
right-shifted by 8 for the divide). A zero projected-length quotient leaves
the plane-projected zero/tangent result unchanged. This closes the vector
transform itself while leaving collision selection as the collision boundary.

The surrounding ground handoff is now explicit as well. When the collision
result branch is active, `0x00496550` selects either `player+0x2da8..+0x2db0`
for raw state `2` or the fallback vector `(0,0x1964,0)`, projects that vector
with `0x00490610`, and adds it to acceleration through `0x004ca9f0`. For
non-state-2 paths it publishes the projected vector at
`+0x3118/+0x311c/+0x3120`; it always stores the packed collision words at
`+0x3128/+0x312c` and passes them to `0x00490680` for velocity projection.
The native `apply_ground_collision_handoff()` models this post-selection
sequence and intentionally leaves collision-result selection/fallback timing
to its caller.

The air-contact landing path has the matching ordering: it commits the swept
contact position first, clears `+0x3144` and `+0x29dc`, writes the landing
marker, consumes it through `0x004914d0`, publishes the collision contact
identity at `+0x30b0`, and requests raw state `0` at `0x004991fe` with reason
`0x1fd6`. After that request, the
non-raw-1 branch projects velocity with the contact normal and writes the
packed-normal fields used by the next grounded pass. Raw state `3` contacts
inside the first `0x1e` frames remain in the grace path and do not request the
landing transition. The native extended `AirContactInput` boundary carries
the identity and packed normal while keeping collision detection external.

### Recovered ollie impulse arithmetic

`FUN_0049a280` computes the vertical impulse before calling the state-request
helper. The recoverable inputs are the charge at `+0x2de8`, slope metric at
`+0x3110`, horizontal speed metric at `+0x2f30`, and the signed height delta
`(+0x2f48 - +0x2f4c) >> 12`. It selects the low-slope branch for
`abs(+0x3110) < 0x9c4`, otherwise the high-slope branch, then adds the result to
`+0x50`. Raw state 5 also adds the wallie bias `-0xf000`.

The random draws are deliberately inputs to the native
`compute_ollie_vertical_impulse` helper rather than generated there. This
preserves the retail operation order while avoiding a false claim about the
shared game RNG stream. The streams are kept separate in the native release
input: five draws feed the impulse formula, a later pair feeds the
`+0x3040` early-release comparison, and raw state 5 has its own pair for the
wallie charge cap at `0x0049a8d2`. The held-charge path likewise uses one pair
for the first cap check at `0x0049a5bb` and a fresh pair for the refresh write
at `0x0049a623` when the cap is exceeded or forced. The decompiler shows a
speed/height adjustment gate with an inconsistent redundant comparison; the
native helper keeps the observed effective `height_delta > 500` gate explicit
and marks that condition as unresolved. The launch bookkeeping mirrored by
the native slice includes `+0x2dec` charge snapshot, `+0x2ddc` in-progress,
`+0x2db8` mode latch, `+0x303c` launch count, `+0x3040` early-release count,
and `+0x2f34` launch frame.

The controlled action comparison now resolves the direct input-to-launch
boundary. The JUMP-only run proves the configured record and mask (`+0x00`,
`0x0010`) but produces no launch-latch consume or causal raw-state-1 request.
The KICK-only run proves the positive path: KICK record `+0x30`, mask
`0x0040`, reaches the prephysics launch at frame 660. Static analysis recovers
the same stateful half: `0x0049a280` loads the action pointer at `0x0049a58d`,
tests its `+0x30` KICK byte, increments `+0x2de8`, caps it at
`0xf - (random_a + random_b) / 0x14`, sets `+0x2de0=1` at `0x0049a640` or
`0x0049a6ac` when its state/animation gate is eligible, and sets `+0x2dd8=1`
at `0x0049a6bf` while the launch is pending. The launch consumes the latch at
`0x0049a751`; the non-eligible cleanup clears `+0x2dd8` at `0x0049a6d2` and a
stale latch at `0x0049a6ef`.

The remaining seam is therefore not “JUMP bit directly arms the ollie latch.”
It is the upstream gameplay/animation mapping that makes the KICK-gated path
the user-facing ollie action. The native model keeps that eligibility explicit
and now models the KICK action record separately from the in-air JUMP record.

The obvious gameplay-update candidates were checked as a negative result:
`0x00489930` only updates the action record, `0x00469d30` routes through the
action-history reset path, and `0x00466be0` only updates a timing global. None
of those decompilations writes `player+0x2de0`; the search should continue at
the skater/object update dispatch rather than being folded into the input
record model.

The input-side negative can now be stated more precisely. `0x004e42c0`
produces JUMP and KICK as independent mask bits, and `0x00489a10` independently
updates the `+0x00` and `+0x30` action records. `0x00489930` contains only the
record held/edge/counter stores; its press callback `0x00466b60` is empty in
the recovered decompilation. Thus the direct `0x0010` → `0x0040` alias is not
present in this chain. A later gameplay/animation eligibility layer may still
be what makes the KICK-gated path user-facing, so the native boundary keeps
that decision explicit.

A direct binary sweep for stores to `player+0x2de0` also closes the remaining
obvious latch-writer gap. The normal gameplay latch lifecycle is limited to
`0x0049a640`/`0x0049a6ac` (set), `0x0049a6ef` (stale clear), `0x0049a729`
(cancel clear), and `0x0049a751` (launch consume). The other observed writers
are initialization or recovery/reset paths: `0x0046c9e3`, `0x00490585`,
`0x00490f5a`, `0x0049d8bf`, and `0x004c6af7`. No action-record updater or JUMP
record consumer writes this latch. This is a negative result about the
recovered executable path, not a claim about the unrecovered frontend that
may map a user-facing JUMP command onto the KICK-gated prephysics action.

### Ground re-entry bookkeeping

The release-side prephysics function also contains a deterministic cleanup
that occurs after a launch request has returned to a raw grounded value. When
`+0x2ddc` is nonzero, the phase/history word `+0x30c0` is nonzero, and raw
state is `0` or `7`, the stores are:

| Address | Field | Effect |
| ---: | --- | --- |
| `0x0049a4b3` | `+0x29c8` | clear when `+0x2dd4 == 0` |
| `0x0049a4b9` | `+0x2ddc` | clear in-progress launch flag |
| `0x0049a4bf` | `+0x2db8` | clear latched launch mode |
| `0x0049a587` | `+0x2c68` | clear recovery/action latch |

The same block calls animation, sound, combo, and skater helpers; those remain
outside this physics slice. The native `run_ollie_prephysics()` now preserves
the four state writes and exposes `recovery_cleared`, which lets a replay
model represent the first grounded recovery pass without assigning names to
the surrounding action fields.

### Recoverable air position integration

The apparent “air handler” is not only a dispatcher label. In
`Skater_DoPhysicsInAir` (`0x00497f40`), the position step at callsite
`0x004983c0` adds a fixed-point displacement directly to `player+0x08`:

```text
dt       = DAT_0056865c
dt2      = (dt * dt) >> 8              ; DAT_00568804
accel_d  = ((player+0x58 * dt2) >> 8) / 2
velocity =  (player+0x4c * dt) >> 8
position += velocity + accel_d
```

The vector helper sequence is `0x004cac30` (component multiply),
`0x004cacd0` (arithmetic `>> 8`), `0x004cac90` (integer divide by 2),
`0x004cabb0` (vector add), and `0x004ca9f0` (add to the live position). The
runtime timestep is supplied by the frame clock, so it is not hard-coded into
the native state machine. `src/physics_state_machine.cpp` exposes this as
`compute_in_air_position_delta` and `integrate_in_air_position`; the latter
updates the live position without pretending that it is the later collision
constrained `0x00496060` commit.

The jump-held vertical effect is applied immediately before this integration:
the native `apply_in_air_jump_hold_effect` method mirrors the
`0x00497fff`/`0x00498009`/`0x0049800c` check and stores. It is only active after
the recovered counter threshold (`+4 > 2`); the first two held updates leave
the existing vertical terms unchanged.

After the handler's motion and collision tests, an accepted contact calls the
position commit and then requests raw state `0`; the exact request instruction
is `0x004991fe`. This separates the three operations that a recreation must
keep distinct: integrate, collision-test/adjust, and commit/land.

The common air tail then adds the frame-start gravity vector to acceleration at
callsite `0x004992f0` (`FUN_004ca9f0(&player+0x58, &player+0x2da8)`). The outer
frame initializes that vector as `(0, player+0x2dac, 0)`, so the recovered
native `apply_in_air_gravity()` increments acceleration Y after the motion and
contact callback. It intentionally does not feed gravity into the displacement
that was just integrated; the updated acceleration is consumed by the next
air-frame position step.

### Recoverable in-air action acceleration

The beginning of `Skater_DoPhysicsInAir` also contains a deterministic action
control block before the jump-held vertical suppression and position step.
When the global air-control gate `DAT_0056b7f0` is enabled, the handler uses
the already-published orientation basis and adds these terms to acceleration
in this order:

| Action record | Basis/vector | Scale | Operation |
| --- | --- | ---: | --- |
| KICK `+0x30` | `+0x310c/+0x3110/+0x3114` | `+0xb4` (180%) | add |
| UP `+0xa0` | `+0x30f4/+0x30f8/+0x30fc` | `+0x96` (150%) | subtract |
| DOWN `+0xb0` | `+0x30f4/+0x30f8/+0x30fc` in normal X/Y/Z order | `+0x96` (150%) | add |
| SPINLEFT `+0x40` | `+0x3100/+0x3104/+0x3108` | `+0x96` (150%) | add |
| SPINRIGHT `+0x60` | `+0x3100/+0x3104/+0x3108` | `+0x96` (150%) | subtract |

For each row the scalar is `(player+0x2dac * scale) / 100`, each component
is multiplied by that scalar, and the result is arithmetic-shifted right by
12 through `0x004caa50`/`0x004caab0`; it is then added or subtracted through
`0x004ca9f0`/`0x004caa20`. This is distinct from the x87 truncate-toward-zero
dot/scalar helper used by collision math. The native
`apply_in_air_action_control()` exposes this block with caller-supplied basis,
gravity, and the existing action records. The deterministic basis work in
`0x00497df0` is available through `prepare_in_air_orientation()`; its initial
orientation/global inputs, the shared `0x004e2070` angle/clamp side effect,
collision selection, and animation/global gate resolution remain separate
inputs. The decompiler's reversed-looking DOWN local assignments do not
permute components; the native model and regression fixture preserve the
normal basis order.

After the action terms, the same gated block applies an in-air stabilization
term. It computes `dot(velocity, basis_3100)` with the Q12 x87 helper, scales
each `basis_3100` component by that dot, divides each component by `0x20`
using integer division, and subtracts the resulting vector from acceleration.
This is included in the native action-control helper; it runs whenever
`DAT_0056b7f0` is enabled, even when no listed action is held.

Minimal recreation contract:

```cpp
struct PhysicsState {
    int32_t raw_state;       // player + 0x30b8
    int32_t phase_state;     // player + 0x30c0; helper/frame phase history
    int32_t auxiliary;       // player + 0x30c4
    Vec3 position;
    Vec3 old_position;
    Vec3 velocity;
    uint32_t action_mask;
    ActionState jump;        // base +0x00, mask 0x0010; in-air effect
    ActionState kick;        // +0x30, mask 0x0040; ollie charge/latch gate
};

void RequestPhysicsState(int32_t next, uint32_t reason) {
    const int32_t old = state.raw_state;
    if (old != next) {
        trace_state_change(old, next, reason);
    }
    state.raw_state = next;
    state.phase_state = old; // helper copies the pre-request value
}

void BeginDispatcherPhase() {
    state.phase_state = state.raw_state; // 0x0049e680 before 0x0049db80
}
```

The first implementation should retain integer raw states and reason tags,
including the shared state-1/state-3 in-air handler. It should not encode raw
state 1 as “jump” globally: static callsites also request state 1 from ground
collision response, and state 3 is a distinct value with the same in-air
callee. Fixed-point/integer velocity and position behavior should be retained
at the physics boundary; no grounded Left/Right steering formula is inferred
by this session.

## Diagnostic strings and source clues

No runtime string names the raw `+0x30b8` values. The relevant static symbols
are:

- `Skater_DoPhysicsInAir` at `0x00497f40`
- `P_O_I in DoPhysicsInAir`
- `mOldPos wrong at start of DoInAirPhysics`
- source diagnostic `H:\TonyHawk_Pc2\physics.cpp`
- `restart_at_start_of_main_physics`
- source-symbol concepts `DoOnGroundPhysics`, `DoOnRailPhysics`, and
  `EPhysicsState`
- prephysics action strings `NOLLIE` and `FAKIE_OLLIE`
- `Tricks_Landed`

These support the physics/ollie interpretation but do not establish a global
enum mapping for every raw state.

## Provisional minimal machine

```text
KICK edge (mask 0x0040; action record +0x30 byte +1)
    ↓
prephysics charge/latch 0x0049a280
    ↓ request helper 0x004900b0 (callsite 0x0049aca0, reason 0x245c)
    ↓ actual store 0x004902bf
physics_state 0 → 1
    ↓
dispatcher 0x0049db80, case 1
    ↓
in-air handler 0x00497f40
    ↓
position updates / commit path
    ↓
landing/recovery boundary
    ↓ actual store 0x004902bf
physics_state 1 → 0
```

The KICK trace proves one reproducible `0 → 1 → 0` sequence with exact state
values, transition writer, dispatcher case, action record, launch-latch
consume, and position callsites. The JUMP-only control proves that the
configured JUMP edge is not itself sufficient to produce this launch path;
unrelated transient transitions were observed in other captures and remain
intentionally separate.

The optional runtime entry probes for `0x004900b0`, `0x004902bf`, and
`0x00497f40` are now available in the GDB helpers. The clean bounded trace is
the primary dynamic state sequence; the independent frontend trace adds direct
`0x00497f40` entry records and live state-request records, strengthening the
handler and landing assignments without globally renaming the raw enum.

The native frame now exposes the ordinary landing predicate directly through
`accepts_standard_air_contact()`. It recomputes the five raw expressions from
the packed `PositionCollisionHit` face word and face flags, then applies the
player-owned blocked/last-surface inputs before calling the existing
commit-before-state-request path. This closes the disk PSX face metadata to
in-air state transition boundary while leaving wall, rail, and special-contact
classification outside the generic predicate.
