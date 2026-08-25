# Skater physics dispatch and in-air update

Status: inferred
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0049db80`, `0x00497f40`

## Observation

- `0x0049db80` dispatches on the object field at `param_1 + 0x30b8`, with cases that call `0x00496550`, `0x00497f40`, `0x00494210`, `0x00499710`, and `0x004993f0`.
- The same dispatcher reads/writes state fields around `param_1 + 0x30c4`, clears or updates velocity-related fields, and resets position from the object offsets `+0xbc`, `+0xc0`, and `+0xc4` in one state.
- `0x00497f40` references `physics.cpp` diagnostics `P_O_I in DoPhysicsInAir` and `mOldPos wrong at start of DoInAirPhysics`. It reads the skater object position at `+8`, `+0xc`, and `+0x10`, performs collision/ground calculations, and writes updated movement state.
- The source symbol dump contains the matching concepts `DoInAirPhysics`, `DoOnGroundPhysics`, `DoOnRailPhysics`, and `EPhysicsState`, but those names are treated as cross-build hypotheses until runtime mapping confirms them.
- A one-shot Warehouse write watch on the first `+0xbc` word fired at debugger PC `0x0049ea81` (`Skater_PhysicsDispatcher+0xf01`) for player `0x05f39530`. The preceding store at `0x0049ea7f` writes the first word of the `+0xbc` vector.
- Static disassembly around that store copies the three words at `player + 0x08`, `+0x0c`, and `+0x10` into `player + 0xbc`, `+0xc0`, and `+0xc4`. This is the first concrete movement-state handoff: the `+0xbc` vector is a previous-position/state-history candidate, not yet confirmed as velocity.
- A three-watch Warehouse run watched `player + 0x08`, `+0x0c`, and `+0x10` simultaneously, using three of the four x86 hardware slots. It captured 621 writes before the WineDbg proxy failed while a bounded watch was being removed; the trace is `build/debug/sessions/pos-writes-auto2/xyz.trace.ndjson`.
- The same writes repeatedly trap after the contiguous stores in `0x00496060`: `0x00496257` writes Y, `0x00496260` writes X, and `0x00496263` writes Z. This is the strongest current candidate for a position commit path because all three components are written to the skater object in one routine.
- Other confirmed writers include the vector-add helper at `0x004ca9f0` (post-store PCs `0x004ca9ff`, `0x004caa0d`, and `0x004caa16`) and a physics-dispatch path at `0x0049f0cc`, `0x0049f0d4`, and `0x0049f0e2`. The post-store PC is one instruction beyond the actual write, as expected for x86 debug traps.
- Static callers of `0x00496060` include the in-air candidate `0x00497f40` at `0x00498cf5`, `0x0049905c`, and `0x0049917b`, the dispatcher path at `0x0049f0e5`, and additional state routines at `0x00496550` and `0x00499710`. The common call shape loads three scalar arguments and invokes the routine with the player object in `ecx`.
- The repeatable `tony-position-commit 16` probe captured 16 stationary dispatcher calls with `action_mask=0` and `physics_state=0`, followed by calls through `0x0049917b` (`Skater_DoPhysicsInAir+0x123b`) after Left input. A correlated event recorded `action_mask=0x8000`, held key `203` (Left), `physics_state=1`, and changed three commit arguments: `0x00081fc8`, `0x02752b10`, `0x00568838`. The complete trace is `build/debug/sessions/position-probe3/position.trace.ndjson`.
- A controlled Warehouse Left/Right comparison reproduced the input-to-commit handoff in two fresh isolated sessions. The Left trace (`build/debug/sessions/lr-compare4/lr.trace.ndjson`) recorded the action edge at `0x0056b078`, then an `in-air-position-3` call at `0x0049917b` with `action_mask=0x8000`, held key `203`, `physics_state=1`, and arguments `0x00043108`, `0x02795ec6`, `0x00568838`. The Right trace (`build/debug/sessions/lr-compare6/lr.trace.ndjson`) recorded the action edge at `0x0056b088`, then repeated `physics-dispatch-position` calls at `0x0049f0e5` with `action_mask=0x2000`, held key `205`, `physics_state=0`, and the first directional sample `0xfffe2127`, `0x03376e1a`, `0x00568835`.
- The side-by-side result confirms the action-state mapping `Left -> 0x8000 / DirectInput 203` and `Right -> 0x2000 / DirectInput 205`. Both feed the shared `0x00496060` position commit routine, but these captures entered different state-specific callers: Left while airborne and Right through the dispatcher while grounded. The caller difference is therefore a state difference, not evidence of separate Left/Right position writers.
- A same-state ground comparison (`build/debug/sessions/ground-compare2/ground.trace.ndjson`) captured 31 Left and 30 Right position commits through the same `physics-dispatch-position` callsite `0x0049f0e5`, all with `physics_state=0` and the constant third argument `0x00568835`. During the Left phase, the first two arguments ranged from `0xfffe2580`, `0x030654f3` to `0xfffd5480`, `0x02daee1f`; during the Right phase, they ranged from `0xfff63da1`, `0x037217d7` to `0xfffe2bb4`, `0x032cd737`. This confirms that both direction bits reach the same grounded handoff while changing its scalar inputs.

## Interpretation

`0x0049db80` is conservatively named `Skater_PhysicsDispatcher`; `0x00497f40` is a strong candidate for the in-air physics routine. The object layout and exact state enum remain unconfirmed.
The dynamic position watches and static cross-references identify `0x00496060` as a shared position commit path. The strongest state-specific caller is the in-air routine at `0x00497f40`; `0x0049f0e5` is the dispatcher handoff that supplies the prior-position/current-position vectors. `0x004ca9f0` is best treated as a reusable `Vector3_Add` helper rather than the owner of the player state.
The live probe now ties both directional action edges to the shared handoff. When the state is controlled, Left and Right use the same grounded caller and differ in the first two scalar arguments, making the code immediately before `0x0049f0e5` the next useful instruction-level target for identifying the directional components.

## Open questions / falsifiers

- Use the stable callsites at `0x00498cf5`, `0x0049905c`, and `0x0049917b` as software-breakpoint targets when the WineDbg proxy refuses to stop at the shared callee entry.
- Trace the values feeding `0x0049917b` across a longer Left hold and compare them with the action-state record at `0x0056b078`.
- Inspect the input-state writer around `0x00489966`/`0x0048996a` and the argument setup immediately before `0x0049f0e5` while replaying the two ground phases.
- Determine which of the first two commit arguments represents each horizontal/forward component by correlating their deltas with the skater's facing direction.
- Confirm which dispatcher case corresponds to stationary ground, rolling, airborne, and rail states.
- Do not assign final names to the object vectors until repeated runtime observations support them.
