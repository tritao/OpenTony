# Skater physics dispatch and in-air update

Status: inferred
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0049db80`, `0x00497f40`

## Observation

- `0x0049db80` dispatches on the object field at `param_1 + 0x30b8`, with cases that call `0x00496550`, `0x00497f40`, `0x00494210`, `0x00499710`, and `0x004993f0`.
- The same dispatcher reads/writes state fields around `param_1 + 0x30c4`, clears or updates velocity-related fields, and resets position from the object offsets `+0xbc`, `+0xc0`, and `+0xc4` in one state.
- `0x00497f40` references `physics.cpp` diagnostics `P_O_I in DoPhysicsInAir` and `mOldPos wrong at start of DoInAirPhysics`. It reads the skater object position at `+8`, `+0xc`, and `+0x10`, performs collision/ground calculations, and writes updated movement state.
- The source symbol dump contains the matching concepts `DoInAirPhysics`, `DoOnGroundPhysics`, `DoOnRailPhysics`, and `EPhysicsState`, but those names are treated as cross-build hypotheses until runtime mapping confirms them.

## Interpretation

`0x0049db80` is conservatively named `Skater_PhysicsDispatcher`; `0x00497f40` is a strong candidate for the in-air physics routine. The object layout and exact state enum remain unconfirmed.

## Open questions / falsifiers

- Break on `0x0049db80` in Warehouse and record `param_1`, `param_1 + 0x30b8`, and the position/velocity candidates.
- Confirm which dispatcher case corresponds to stationary ground, rolling, airborne, and rail states.
- Do not name `+8/+0xc/+0x10` as position or `+0xbc/+0xc0/+0xc4` as velocity until repeated runtime observations support it.
