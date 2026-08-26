# Warehouse ollie capture

This is the bounded Session B capture used to correlate the jump action record,
physics state, dispatcher, and position updates.

The canonical image is SHA256
`f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`; runtime
debugging uses its six-byte no-CD derivative, as documented in
`re/evidence/cd-check.md`.

Trace: `build/debug/sessions/physics-states7/ollie.trace.ndjson`
Supplemental raw-contact probe: `build/debug/sessions/physics-contact2/air-collision.trace.ndjson`
Zero-mask state-0 baseline: `build/debug/sessions/physics-states6/ollie.trace.ndjson`,
frames 653–685 (not treated as stationary because its position was still
changing)
Independent frontend probe: `build/debug/sessions/physics-states17/ollie.trace.ndjson`
Level: Warehouse, index `12`
Player: `0x05f39530`

## Capture method

The configured JUMP binding is `SPACE`/`RETURN`/`PAD2` (`0x0010`), while the
separate KICK binding is `B`/`PAD6` (`0x0040`). Headless Wine DirectInput did
not produce a dependable keyboard scan, so the captures injected the resolved
action mask at post-poll address `0x00489a15` and held it for bounded action
updates. This preserves the game’s normal action-state and physics processing
after the mask boundary while making each input edge repeatable.

## Bounded sequence

| Frame | Action / state | Dispatcher and position evidence |
| ---: | --- | --- |
| 623 | Jump edge injection at `0x00489a15`; action record `0x0056aff8` byte `+1` and then byte `+0` become `1` | The companion zero-mask baseline has state `0` for frames 653–685; the bounded trace observes state `0` before the launch transition |
| 626–658 | Mask `0x0010`; jump held byte `+0=1`; held counter advances; state remains `0` | `0x0049db80` case 0; common commit `0x0049f0e5` |
| 659 | `player+0x30b8: 0→1`, watch trap `0x004902c5`, actual writer `0x004902bf` | Dispatcher record is at the launch boundary; position commit `0x0049f0e5` records state `1` |
| 660–662 | State `1`; mask still `0x0010`; held byte remains `1` | `0x0049db80` case 1 → handler candidate `0x00497f40`; position advances each frame |
| 663 | Mask released; jump byte `+0: 1→0` at trap `0x0048996a`, actual store `0x00489968`; inactive counter starts | Dispatcher remains case 1 for this update |
| 664–665 | State `1`, mask `0`, jump record released | Case 1 → `0x00497f40`; position continues updating |
| 666 | `player+0x30b8: 1→0`, watch trap `0x004902c5`, actual writer `0x004902bf` | In-air contact branch commits through `0x00496060`, then requests state 0 at exact call `0x004991fe` with reason `0x1fd6`; `0x0049917b` is the nearby recovery-boundary callsite |
| 667+ | State `0`, mask `0`, jump record inactive | Dispatcher returns to case 0 / grounded helper path |

The position commit implementation is `0x00496060`; the recurring callsite in
this trace is `0x0049f0e5`. The static case-1 handler is the `0x00497f40`
candidate, also used by dispatcher case 3 in the decompilation.

Static launch/landing context: the main physics frame `0x0049e680` calls
prephysics `0x0049a280`, where the ollie path applies the vertical velocity
impulse and requests state `1` with reason `0x245c` (or state `3` with reason
`0x2457` on the alternate branch). The dispatcher sends both raw states 1 and
3 to `0x00497f40`; state 3 has a separate timeout recovery to state 1. In the
bounded raw-state-1 capture, `Skater_DoPhysicsInAir` accepts the contact result,
commits the contact position, and calls `0x004900b0(0, 0x1fd6)` at
`0x004991fe`; this is the exact air → ground request. Ground collision response
at `0x00496550` and ground physics at `0x0049df00` contain additional
contact/recovery requests. These reason tags are static callsite labels, not
finalized enum names.

The same frame decompilation fixes the outer ordering: after prephysics and
ground preparation, the frame copies `+0x30c0 = +0x30b8`, rotates
`+0xbc/+0xc0/+0xc4` into the older position history, calls the collision-start
check at `0x00490730`, enters the dispatcher, and performs the common position
commit before postphysics velocity damping at `0x0049d480`. The native
`PhysicsStateMachine::step_frame` exposes those points as callbacks. It does
not provide collision results or grounded steering.

The launch arithmetic is also recoverable: `0x0049a280` computes the integer
vertical delta from charge `+0x2de8`, slope `+0x3110`, speed `+0x2f30`, and the
height delta `(+0x2f48 - +0x2f4c) >> 12`, using shared random draws. The native
`compute_ollie_vertical_impulse` helper takes those draws explicitly so replay
tests remain deterministic without assuming the global RNG sequence.

The in-air position step is also recoverable at static callsite `0x004983c0`.
With `dt = DAT_0056865c` and `dt2 = (dt * dt) >> 8`, the handler adds
`(velocity * dt) >> 8` plus `((acceleration * dt2) >> 8) / 2` to the live
position. The source fields are `+0x4c/+0x50/+0x54` and
`+0x58/+0x5c/+0x60`; the helper chain is `0x004cac30`, `0x004cacd0`,
`0x004cac90`, `0x004cabb0`, and `0x004ca9f0`. This direct integration occurs
before collision testing and is separate from the later constrained commit at
`0x00496060`. The native reference model exposes the operation through
`compute_in_air_position_delta()` and `integrate_in_air_position()` while
leaving the runtime timestep caller-supplied.

## Action record details

The jump record transitions were captured as:

```text
frame 623: byte +1 0→1 at trap 0x00489951 (store 0x0048994d)
frame 623: byte +0 0→1 at trap 0x00489966 (store 0x00489963)
frames 626–662: byte +0=1, +4 increments 4→40, +8=0
frame 663: byte +0 1→0 at trap 0x0048996a (store 0x00489968)
frames 663+: +4=0, +8 increments, +0xc continues incrementing
```

The `byte +1` release-looking write at `0x0044de62` in frame 626 belongs to a
second-bank/main-loop copy path; it is not used as the action-record semantic
assignment. The authoritative action update stores are the `0x004899xx`
instructions above.

## Decisive KICK-gated launch confirmation

The primary trace above is retained as the first clean state sequence, but its
delayed JUMP edge is not sufficient to attribute the launch. The controlled
comparison uses the same Warehouse player (`0x05f39530`) and a physics-frame
delay of 620 in both runs.

| Frame | Event | Exact evidence |
| ---: | --- | --- |
| 620 | KICK edge | mask `0x0040` is injected at `0x00489a15`; action record `+0x30` becomes held with its press-edge byte set |
| 621–659 | KICK held | KICK `u32 +4` advances through the held updates; the base JUMP record is not the launch gate |
| 660 | Launch latch consumed | `0x0049a751` sees `+0x2de0=1` before the store and `+0x2de8=12`; request path returns at `0x0049aca0` with raw state `1`, reason `0x245c` |
| 660 | Ground → air writer | `0x004902bf` writes `player+0x30b8: 0→1` |
| 661–680 | Airborne updates | dispatcher case `1` selects `0x00497f40`; position updates continue in the air handler |
| 681 | Landing transition | exact request instruction `0x004991fe` / caller `0x00499203` requests raw state `0`, reason `0x1fd6`; `0x004902bf` writes `1→0` |

The corresponding JUMP-only trace is
`build/debug/sessions/physics-states32/jump.trace.ndjson`. Its edge at frame
620 sets mask `0x0010` and the base record `+0x00` held/edge fields at frame
621, but it produces no `0x0049a751` event and no causal raw-state-1 request.
Its later `0→2→0` transition is the ordinary collision path: request call
`0x004972da` (runtime return `0x004972df`) with reason `0x1ac9`, followed by
the ground-collision return request at `0x00497479` with reason `0x1b19`; it is
not the ollie launch. This negative result is kept explicit so the configured
JUMP binding is not incorrectly hard-wired to the prephysics KICK gate in the
recreation.

The exact causal pipeline for the observed launch is therefore:

```text
KICK edge / mask 0x0040 at frame 620
  → KICK-held charge/latch path in 0x0049a280
  → launch latch consume 0x0049a751 at frame 660
  → request raw state 1, reason 0x245c
  → writer 0x004902bf: 0 → 1
  → dispatcher case 1 → 0x00497f40
  → accepted contact / position commit 0x00496060
  → request raw state 0 at 0x004991fe, reason 0x1fd6
  → writer 0x004902bf: 1 → 0
```

The mapping from the user-facing configured JUMP concept to this KICK-gated
ollie path remains an upstream gameplay/animation seam. The direct gameplay
physics path and its normal request writer are resolved; other lifecycle,
special-action, replay, and network writers found by static scan are separate
ingress paths, not additional conclusions from this Warehouse trace.

## Independent raw state-4 chain

The frontend-driven companion trace also records a separate collision path,
with duplicate request/writer records from the armed probes, whose ordered
raw transitions are:

```text
0 → 2  at 0x004972da, reason 0x1ac9, return 0x004972df
2 → 4  at 0x004913dd, reason 0x0b1c, return 0x004913e2
4 → 1  at 0x004905ab, reason 0x0715, return 0x004905b0
1 → 0  at 0x004991fe, reason 0x1fd6, return 0x00499203
```

The common state writer is `0x004902bf`; the dispatcher routes the middle
states through `0x00496550`, `0x00494210`, and `0x00497f40` respectively.
State 4 is provisionally the cross-build rail slot, but the trace does not
replace the clean ollie sequence or establish the full geometry semantics of
the dedicated state-4 handler.

## Historical clean-sequence result

The original primary trace plus the companion state-0 baseline supply the
reproducible raw ground → air → ground shape:

```text
edge at 623
  → state 0 → 1, writer 0x004902bf at 659
  → dispatcher case 1 / 0x00497f40 candidate, frames 660–666
  → accepted contact, 0x004991fe requests state 0 (reason 0x1fd6)
  → state 1 → 0, writer 0x004902bf at 666
  → grounded case 0 recovery
```

The independent frontend probe confirms the callee entry and landing request
without being used to rename the intermediate states. It records the live
sequence `0 → 2 → 4 → 1 → 0` around a configured jump edge, with direct entries
at `0x00497f40` during state `1` and the landing request at `0x004991fe`.
Because that run began with the player already contacting Warehouse geometry,
state `2` and state `4` must nevertheless be retained as real raw values.

Raw state `0` is retained as a grounded/normal-path candidate and raw state
`1` as an airborne-path candidate. Other values, especially state `2` and its
`+0x30c4` activity, remain provisional because repeated captures showed
transitions unrelated to this bounded action-edge window.

For a C++ recreation, keep `raw_state`, `phase_state`, and `auxiliary`
separate, preserve the state-request reason, and retain old/current position
history. The minimum faithful order is: action record update → ollie
prephysics/vertical impulse → state request → dispatcher → position update and
commit → contact/landing recovery. Do not globally rename raw state 1 to
“jump”: it is also requested by collision paths, while raw state 3 is distinct
despite sharing the in-air handler.

The action record pointer is stored in `player+0x2ccc` by the player
constructor (`0x0056aff8` for the first skater). In-air processing checks its
held byte and held counter at `0x00497fff`; once the counter exceeds `2`, the
retail writes at `0x00498009` and `0x0049800c` zero vertical acceleration and
velocity before applying the fixed-point position step. The native replay
model exposes this as `apply_in_air_jump_hold_effect`. The launch routine at
`0x0049a280` does load the action-record pointer, but its charge gate reads the
`+0x30` KICK subrecord rather than the configured JUMP record at base `+0x00`.
The native replay keeps the KICK record and the unresolved user-facing action
mapping as separate boundaries.
