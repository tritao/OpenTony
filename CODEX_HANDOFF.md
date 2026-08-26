# OpenTony handoff

This file is the restart guide for a fresh Codex/agent session or another developer machine.

## Read first

1. `README.md`
2. `docs/WORKFLOW.md`
3. `re/evidence/README.md`
4. `re/config/binaries.yml`
5. the subsystem note relevant to the current task under `re/notes/`

## Non-negotiable project rules

- `game/` is user-supplied proprietary input. Never commit or modify canonical copies.
- `build/` and `.tools/` are disposable/generated.
- `re/` is the source of truth for recovered knowledge.
- Record exact executable/media hashes before relying on addresses.
- An appealing interpretation is not a fact. Use the evidence levels in `re/evidence/README.md`.
- Prefer a controlled experiment over prolonged speculation.
- New tooling should normally be exposed through `tony`, not as an undocumented one-off command.
- Ghidra projects are rebuilt from the executable plus tracked knowledge; do not rely on local project state alone.
- Keep generated decompilation dumps out of Git unless a small excerpt is intentionally curated as evidence.

## Current stage

Milestone 1 is complete. `game/THPS2.img` is recorded as a 2352-byte-sector raw CD image with Mode 2/Form 1 sectors:

- size: `830514720` bytes / `353110` raw sectors
- SHA-256: `d7cb5caaa9751b9afced2ebbca68b74fdc0f7a0df70fa4d550230cd7ac33a66e`
- ISO-9660 volume: `316808` user-data sectors
- raw tail beyond the filesystem volume: `36302` sectors / `85382304` bytes
- raw-tail SHA-256: `0452b0321938bf7da2a52d0540ab9f2030aaac6c230c939ff621e7075eff60bf`

The raw tail is recorded but intentionally unclassified; the normalized ISO contains only the declared ISO-9660 volume.

The extracted retail executable is now recorded:

- path: `build/disc/files/SETUP/data/THawk2.exe`
- size: `1450035` bytes
- SHA-256: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
- format: PE32/i386
- image base: `0x00400000`
- entry point: `0x00502f74`

The generated `build/patched/THawk2.nocd.exe` bypasses the observed CD audio-TOC gate without changing the recorded retail executable. A runtime smoke run reached DirectDraw and loaded the game paths from Wine `D:`; the generated runtime log is under `build/runtime/`. Isolated `tony debug` launches now use a `1024x768x16` Xvfb screen with Mesa llvmpipe, which reaches the executable entry point and the CD-check helper headlessly.

The Warehouse gameplay loop and the first physics state machine are now
partially recovered. `re/evidence/functions/physics-states.md` records a
controlled KICK-edge → raw state `0 → 1 → 0` sequence with exact launch-latch
consume `0x0049a751`, state writer `0x004902bf`, dispatcher `0x0049db80`,
in-air handler `0x00497f40`, landing request `0x004991fe`, and position commit
`0x00496060`. A matching JUMP-only control proves the configured JUMP record
and bit do not directly produce that launch path. The framework-free native
boundary is in `src/physics_state_machine.hpp/.cpp`; it deliberately leaves
collision-result selection and orientation/animation input selection as
callbacks.
It also contains the
recoverable outer-frame ordering and fixed-point ollie impulse helper, the
KICK-gated prephysics charge/latch/release operation (including stale-latch
expiry and raw state 1/3 selection), and the fixed-point in-air
position step from `0x00497f40`, including the direct held-JUMP vertical
suppression at the in-air handler. The random stream and upstream user-facing
JUMP-to-KICK/gameplay eligibility mapping remain explicit seams. The native
helpers also reproduce the retail Q12 truncate-toward-zero math, the in-air
action-axis terms and final velocity/basis stabilization subtraction,
orientation-short basis permutation, frame-start gravity scalar, supplied-input
ground-surface acceleration tail, and supplied-draw postphysics velocity
damping. The packed-normal vector transform at `0x00490610`/`0x00490680` is
also decoded, including the ground collision-result handoff into acceleration,
`+0x3118`/`+0x3128` contact fields, and the post-request landing projection;
collision-result selection itself remains caller-owned.
The outer-frame model now clears acceleration at `0x0049e996`, `0x0049e999`,
and `0x0049e99e` for every raw state other than `1`, and raw state `6` is
treated as an in-air/contact path after its `0x004993f0` setup fallthrough.
The stateful native boundary now also publishes the recovered frame-start
gravity scalar into `+0x2dac` through `initialize_gravity_acceleration()`.
Dispatcher case 6 has a separate `run_state6_preair_setup()` boundary for
`0x004993f0`: it records the `0xf - (rand(2)+rand(0))/0x14` charge, request
to raw state 1 with reason `0x2058`, and `+0x2ddc=1`; its additional
orientation-relative velocity shaping is implemented with its three explicit
type-2 random draws and published `+0x30f4` basis.
Raw state `2` is retained as a distinct collision-transient dispatch case:
the ground-collision path requests `0x004972da` → state `2` with reason
`0x1ac9`, case 2 selects `0x00496550` and sets `+0x30c4`, and the return path
requests state `0` at `0x00497479` with reason `0x1b19`. The native
`enter_collision_transient()` and `exit_collision_transient()` methods retain
those exact request callsites while leaving collision selection caller-owned.

The bundled PSX symbol data also provides a useful cross-build semantic layer:
the `EPhysicsState` constants in `SKATE2.TAG` are ordered as ground, air,
invisible, air-stick-to, rail, wallride, footplant, stopped, and handplant.
The PC dispatcher’s eight cases match those handler roles in order. The native
model exposes these as `classify_physics_state()` / `DispatchResult::semantic_state`
without replacing the raw PC integer or claiming that the PC enum itself has
been directly symbolized; a runtime rail/wallride/plant capture remains the
next confirmation target.

The launch branch is anchored to the configured controls: JUMP is
`SPACE`/`RETURN`/`PAD2` (`0x0010`), KICK is `B`/`PAD6` (`0x0040`), and
`0x0049a280` reads the KICK record at action-bank offset `+0x30`. The native
slice deliberately does not invent a JUMP-to-KICK mapping. It also preserves
the raw-3 timeout request (`0x0049de9e`, reason `0x2bf2`) and the normal
landing bookkeeping writers (`0x004991a4`, `0x004991b3`, `0x004991d4`,
`0x004991f8`, `0x00499255`) around the `0x004991fe` air-to-ground request.
The prephysics model also keeps the retail random consumers separate: impulse
formula draws, held-charge cap refresh, wallie cap, and early-release count.
The eligible grounded charge path clears the movement target at writer
`0x0049a669`, and launch resets for `+0x2c08`, `+0x3068`, and `+0x306c` are
covered by the native replay test. The first grounded prephysics pass after
landing now also reproduces the deterministic recovery stores at
`0x0049a4b3`, `0x0049a4b9`, `0x0049a4bf`, and `0x0049a587` for the action
context/in-progress/mode/recovery-latch fields.

The grounded dispatcher has a separate static leave-ground transition after
its case-0 helper sequence: when the published slope/recovery-window
predicate is met, callsite `0x0049ddcf` requests raw state `1` with reason
`0x2ba1`, and the following store clamps negative Y velocity. The native
`try_ground_to_air()` method keeps those supplied inputs and transition
metadata explicit; it is not conflated with the KICK/ollie path.

The independent frontend trace also provides an exact collision transition
chain `0 → 2 → 4 → 1 → 0`: requests occur at `0x004972da` (`0x1ac9`),
`0x004913dd` (`0x0b1c`), `0x004905ab` (`0x0715`), and `0x004991fe`
(`0x1fd6`), all written by `0x004902bf`. Raw state 4 dispatches to
`0x00494210` and is provisionally the cross-build rail slot; the native
`enter_state4_from_collision()` / `leave_state4_to_air()` methods retain only
these request boundaries while rail geometry remains outside the model.

The in-air handler's deterministic action-control block is also exposed in
the native slice: when its supplied global gate is enabled, KICK/UP/DOWN and
spin records add or subtract the recovered orientation-axis acceleration
terms before jump-held suppression and position integration. DOWN uses the
published basis in normal X/Y/Z order; the apparent reversal in the
decompiler is only local assignment order. Basis preparation is exposed as an
`air_preparation` callback at `0x00497df0`, and the native
`prepare_in_air_orientation()` implements its deterministic rolling-axis
normalization, signed-short cross products, and orientation-short publication
at `0x00465f60`, `0x004e2ff0`, and `0x0049c850`. The shared
`0x004e2070` ordinary signed-16-bit scratch clamp is modeled separately from
the live cross result; its exceptional/shared-global state remains an explicit
seam while collision
selection remains caller-owned. `step_frame()` now also exposes the case-6
pre-air setup callback at the correct boundary: it runs after the case-6
dispatch observation and before common in-air callbacks, so a supplied replay
can request raw state 1 with the recovered charge/velocity shaping.
The same frame contract exposes `gravity_initialization` before the grounded
action stage and `apply_in_air_gravity()` at the recovered `0x004992f0` air-tail
callsite, preserving the fact that gravity is accumulated for the next air
integration rather than the displacement just integrated. When accepted air
contact takes the retail landing branch, the frame skips that gravity-add
fallthrough; the native test covers this ordering. The outer frame's final
acceleration-to-velocity update at `0x0049f206` is exposed as the
`velocity_integration` callback before postphysics damping.
The selected-result contact recovery helper at `0x00497aa0` is also modeled,
including its raw-2/raw-1 request callsites, optional horizontal velocity
correction, and launch-recovery bookkeeping. `step_frame()` now maintains the
same `+0x2d8c` recovery timestamp gate before collision callbacks.
The common-air upright correction at `0x0049c330` is also modeled: its global
up vector is explicit, its ±`0x29` cross-dot branches use the retail `0x000b`
and wrapped `0x0ff5` turn matrix, and its orientation-short/basis publication
is covered by native regression tests.
The post-dispatch control-blocked boundary at `0x0049d8a0` is now explicit as
well. When `+0x2f64` is set it clears X/Z acceleration and the launch latch /
in-progress fields, conditionally clamps descending Y velocity, and applies
the captured component decay from `+0x2c10`; `step_frame()` exposes it before
the outer `0x0049f206` velocity integration. The recent trick-event scan in
the same retail function remains outside this physics-state slice.

The normal gameplay request writer is not globally the only raw-state writer.
Static inventory in `physics-states.md` separates initialization/restart,
special-action raw `8`, script reset, replay decode, and network reset/decode
stores from the `0x004900b0 → 0x004902bf` gameplay path. Those external ingress
paths are documented but are not folded into the controlled Warehouse state
machine.

The input/action-update sweep closes the direct mapping question at that
boundary: `0x004e42c0` emits JUMP and KICK independently, `0x00489a10`
updates independent action records, and `0x00489930` has no player-object
side effect. The native model therefore does not alias JUMP `0x0010` to KICK
`0x0040`; a later gameplay/animation eligibility layer remains the only
unresolved user-facing handoff.

The next movement boundary is also captured. `0x00493370` consumes the full
12-record action bank before prephysics/dispatch; a bounded Warehouse Left
injection (`0x8000`) reaches the Left record and updates `player+0x3144` and
`+0x3148` while raw physics state remains `0`. The native action bank mirrors
those masks, held/edge bytes, and counters. Grounded steering, heading, and
collision formulas remain partly black-box callbacks until a same-state
comparison requires them. The native slice now reproduces the deterministic
`0x00493370` target/clamp/decay/brake stage through
`apply_ground_action_step()`; collision-provided basis/flags feed the separate
`apply_ground_surface_acceleration()` helper, while the `0x00492f20`
animation/heading side effects and shared random stream remain caller-owned.

## Baseline checks

```bash
source .tools/venv/bin/activate
tony doctor
tony verify
pytest -q
```

## Intended first milestones

1. Extract/install the PC game without modifying the canonical image. **Complete.**
2. Identify `Setup.exe` and the installed game layout. **Complete.**
3. Record the exact retail `THawk2.exe` identity and PE32/i386 metadata. **Complete.**
4. Confirm the executable runs under the canonical Wine prefix. **Smoke run complete; repeatable runtime trace pending.**
5. Produce a deterministic Ghidra import from the recorded retail executable. **Complete.** Fresh rebuild currently exports `4739` functions.
6. Locate the startup path and main loop / frame boundary, then capture the first runtime trace. **Startup anchor and physics-frame boundary observed; broader gameplay loop remains partial.**
7. Recover enough player state to define `warehouse-idle`, `warehouse-run`, and `warehouse-ollie` experiments. **Ollie state-machine slice complete; run/steering remain partial.**
8. Extend the native slice with collision/position behavior only when a new
   original-game trace can falsify the implementation.

Update this file when a milestone materially changes the project's starting assumptions.
