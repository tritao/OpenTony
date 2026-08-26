# Frontend level selection and loading

Status: complete 13-entry level-resource table and cross-level PSX/TRG resource matrix; Warehouse runtime witness
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004524a0`, `0x004544a0`, `0x00458900`, `0x0046a8d0`,
`0x004b37a0`, `0x004b39b0`, `0x004c5130`, `0x004c5dc0`, `0x004c8050`

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

## Cross-level asset matrix

The complete extracted level corpus gives a useful sanity check on that table.
The counts below come from the OpenTony offline PSX/TRG readers; they are not
runtime counts guessed from filenames. `obj/model` is the PSX object/model
pair, `bm` is the number of blockmap tags, and `names` is the post-model
texture-name count.

| Level | TRG nodes | Main PSX | `_2` PSX | Normal-script auxiliary names | Two-player-script auxiliary names |
| --- | ---: | --- | --- | --- | --- |
| Hangar | 713 | 470/471, bm 1, names 105 | 356/357, bm 1, names 92 | `SkHan_L`, `SkHan_O` | `SkHan_L2`, `SkHan_O2` |
| School II | 920 | 1164/1164, bm 1, names 211 | 437/437, bm 1, names 131 | `SkSl2_L`, `SkSl2_O` | `SkSl2_L2`, `SkSl2_O2` |
| Marseille | 682 | 853/865, bm 1, names 163 | 592/604, bm 1, names 143 | `SkMar_L`, `SkMar_O` | `SkMar_L2` |
| New York | 1015 | 1218/1218, bm 1, names 193 | 704/704, bm 1, names 130 | `SkNY_l`, `SkNY_O` | `SkNY_l2`, `SkNY_O2` |
| Venice | 997 | 1008/1008, bm 1, names 160 | 598/598, bm 1, names 117 | `SkVen_L`, `SkVen_O` | `SkVen_L2` |
| Skate Street | 842 | 898/922, bm 1, names 132 | 527/551, bm 1, names 93 | `SkSS_l`, `SkSS_O` | `SkSS_l2`, `SkSS_O2` |
| Philadelphia | 1145 | 1001/1015, bm 1, names 187 | 580/583, bm 1, names 88 | `SkPH_l`, `SkPH_O` | `SkPH_l2`, `SkPH_O2` |
| Bullring | 1500 | 837/837, bm 1, names 64 | 389/389, bm 1, names 44 | `SkBul_L`, `SkBul_O` | `SkBul_L2`, `SkBul_O2` |
| School | 164 | 186/186, bm 1, names 51 | 113/113, bm 1, names 34 | `SkB1_O` | — |
| Hawaii | 1289 | 766/766, bm 1, names 98 | 374/374, bm 1, names 49 | `SkHvn_L`, `SkHvn_O` | `SkHvn_L2`, `SkHvn_O2` |
| Chicago/Jam | 606 | 642/883, bm 1, names 110 | 510/510, bm 1, names 52 | `Skjam_l`, `Skjam_o` | `Skjaml2`, `Skjamo2` |
| Vans | 248 | 267/441, bm 1, names 101 | 207/342, bm 1, names 90 | `SkVans_L`, `SkVans_O` | — |
| Warehouse | 313 | 252/288, bm 1, names 89 | 223/259, bm 1, names 87 | `SkWare_L`, `SkWare_O` | — |

The normal and two-player auxiliary columns are script-string evidence, not
an assumption that every similarly named file in the archive is live. For
example, `SKB1_L.PSX`, `SKMAR_O2.PSX`, and `SKVEN_O2.PSX` exist in the corpus
but have no matching normal level trigger-string reference. Conversely,
Chicago's two-player names are the legacy spellings `Skjaml2` and `Skjamo2`
without an underscore, and those exact spellings are present in the archive.

The runtime selection boundary is now explicit. `Front_LoadGame` passes the
level-root string to `0x004c5130`, which appends `.trg` and parses the `_TRG`
file. Trigger command streams are interpreted by `0x004c5dc0`:

```text
0x7e <name>  -> 0x004b37a0(<name>, mode=0, ...)
               raw PSX parse, auxiliary/non-attached region
0x80 <name>  -> 0x004b37a0(<name>, mode=1, ...)
               parse, 0x004b2ac0 finalization, attached environment region
0x81           -> drain the pending PSX spool queue
```

`0x004c8050` executes type-4 `AUTOEXEC` nodes for the ordinary path and type-
15 `AUTOEXEC2` nodes for the two-player path when present. The Warehouse
type-4 node contains `0x7e SkWare_L` and `0x7e SkWare_O`; its type-8 restart
command streams contain `0x80 SkWare` and `0x80 SkWare_2`. This explains the
observed `SkWare_L`/`SkWare_O` auxiliary parses and the finalized main-scene
slot without conflating `_L`/`_O` raw regions with the attached scene.

A headless validation selected the normal first level and used that one-shot override. At `0x004544a0`, the stack contained level `12` and mode `3`; `DAT_0056a898` became `12`. The normal loader then logged `Loading Level: Warehouse`, opened `ware.vab`, initialized Tony Hawk, and proceeded to `0x0046a3a0`.

## Interpretation

The PC binary has a recoverable level-selection chain independent of the PSX symbol dump. `SKATE2.TAG` helps name concepts and source files, but it is not being treated as a direct PC address map.

## Open questions / falsifiers

- Determine whether `DAT_0056a898` is only the selected level index or a broader current-level/session field.
