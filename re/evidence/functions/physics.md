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

## Interpretation

`0x0049db80` is conservatively named `Skater_PhysicsDispatcher`; `0x00497f40` is a strong candidate for the in-air physics routine. The object layout and exact state enum remain unconfirmed.
The dynamic position watches now identify `0x00496060` as the next high-value breakpoint target: inspect its caller and arguments while changing only one input direction, then compare the values passed into its three position stores. `0x004ca9f0` is best treated as a reusable `Vector3_Add` helper rather than the owner of the player state.

## Open questions / falsifiers

- Break on `0x00496060` and follow its caller to identify which physics/collision path supplies each committed position vector.
- Confirm which dispatcher case corresponds to stationary ground, rolling, airborne, and rail states.
- Do not assign final names to the object vectors until repeated runtime observations support them.
