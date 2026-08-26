# Frontend level selection and loading

Status: Warehouse launch observed; complete 13-entry level-resource table recovered
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

The per-skater career initializer at `0x00416790` creates exactly 13 level records. Their static ordering starts with Hangar at index `0` and places the legacy Warehouse entry at index `12`. The GDB helper `tony-force-level warehouse` replaces the next `0x004544a0` level argument with `12`, allowing the normal load/session path to be tested independently of save-game unlock state.

The same initializer walks a static table at `DAT_0053c240` with a `0x1ac`
byte record stride. The resource-root name pointer at record `+0x04` gives the
complete PC level mapping:

```text
0   SkHan_T   (The Hangar)
1   SkSl2_T   (School II)
2   SkMar_T   (Marseille)
3   SkNY_T    (New York)
4   SkVen_T   (Venice)
5   SkSS_T    (Skate Street)
6   SkPh_T    (Philadelphia)
7   SkBul_T   (The Bullring)
8   SkB1_T    (School)
9   SkHvn_T   (Hawaii)
10  SkJam_T   (Chicago/Jam)
11  SkVans_T  (Vans)
12  SkWare_T  (Warehouse)
```

The table entry is the filename stem used by the level loader for the paired
`_T.TRG`, `.PSX`, and auxiliary level resources. This is static data-table
evidence; the live run independently confirmed the index-12 `SkWare` open
and parser path.

A headless validation selected the normal first level and used that one-shot override. At `0x004544a0`, the stack contained level `12` and mode `3`; `DAT_0056a898` became `12`. The normal loader then logged `Loading Level: Warehouse`, opened `ware.vab`, initialized Tony Hawk, and proceeded to `0x0046a3a0`.

## Interpretation

The PC binary has a recoverable level-selection chain independent of the PSX symbol dump. `SKATE2.TAG` helps name concepts and source files, but it is not being treated as a direct PC address map.

## Open questions / falsifiers

- Determine whether `DAT_0056a898` is only the selected level index or a broader current-level/session field.
