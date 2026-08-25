# Frontend level selection and loading

Status: inferred
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004524a0`, `0x004544a0`, `0x00458900`, `0x0046a8d0`

## Observation

- The string `loading Skmedals from front_loadgame` at `0x0052ca14` is referenced by `0x004524a0`.
- The string `Loading Level: %s` at `0x0052caac` is also referenced by `0x004524a0`.
- The decompilation of `0x004524a0` selects a level name using `DAT_0056a898`, loads the level's player resources, loads `items` and `SkMedals`, loads `sk2anim`, and initializes panel/gameplay resources. This supports the provisional name `Front_LoadGame`.
- `0x004544a0` formats `Launch The Game Level: %d, Mode: %d`, sets the game mode global, and calls `0x0046a8d0`.
- `0x0046a8d0` stores its level argument in `DAT_0056a898`, initializes display/session state, calls `0x004524a0`, and continues into the level session.
- `0x00458900` references `H:\TonyHawk\Pc2\LevelSel.cpp` and runs a nested update loop that returns a selected level or `-1`. It is reached through the small wrapper at `0x004588f0`.

The frontend function `0x00452ff0` selects `levelsel.pre`, calls the level-select wrapper, and routes a successful selection into the launch path. `SKWARE`/`Warehouse` assets therefore provide a concrete target for the first dynamic breakpoint experiment.

The headless Free Skate Hangar run dynamically confirmed this chain. `0x004544a0` received level index `0` and mode `2`, then `0x004524a0` logged `Loading Level: The Hangar` before the session reached `0x0046a3a0`.

## Interpretation

The PC binary has a recoverable level-selection chain independent of the PSX symbol dump. `SKATE2.TAG` helps name concepts and source files, but it is not being treated as a direct PC address map.

## Open questions / falsifiers

- Identify the exact numeric level index for Warehouse at the PC table `DAT_0053c1f8` and confirm it dynamically.
- Determine whether `DAT_0056a898` is only the selected level index or a broader current-level/session field.
- Capture the arguments at `0x004544a0` and `0x004524a0` during an authentic Warehouse launch.
