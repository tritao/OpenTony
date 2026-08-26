# Skater PSX model-region loading

Status: confirmed named skater-model PSX load, shared `SK2DEF.PSX` source,
player-spool PSH/PSX handoff, and player-region handoff
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0042f950`, `0x00448dd0`, `0x004b5010`, `0x004521c0`,
`0x004a6bf0`, `0x004b0c60`, `0x004b1430`,
`0x004b43e0`, `0x004b47a0`, `0x004b4940`, `0x004b5370`, `0x004b5620`,
`0x004b3750`, `0x004b37a0`, `0x004b2450`, `0x00452390`, `0x00457420`

The level loader does not only load Warehouse geometry. It also resolves the
active skater/costume through `skatermgr.cpp`, loads the corresponding PSX
model region, and stores the resulting runtime region handles used by the
player object and animation code.

## Manager slots

`0x004b18a0` selects a skater-manager slot through `0x004b0c60`. The manager
supports eight slots; `0x004b0c60(manager, slot)` returns the slot's resource
pointer at `manager + 0x04 + slot * 0x0c`. A second slot record is accessed by
`0x004b1430(manager, slot)` at `manager + 0xb8 + slot * 0xbc`.

`0x004b1980` maps a logical player index to the corresponding enabled/custom
skater index by scanning the 13 skater records at `DAT_0053a514`, whose stride
is `0x170`. This is the stable boundary between front-end player selection and
the skater manager's model/costume slots.

## Named PSX model load

`0x004b43e0(model_name, region_selector)` is the direct skater model-loader
boundary. It validates the name, checks whether the model region is already
loaded, and otherwise:

```text
model_name
  -> format "%s.psx"
  -> 0x00448dd0 raw file read
  -> 0x004b4640 free PSX slot selection/initialization
  -> 0x004b2450 common PSX parse
  -> store model name in DAT_0056d428[slot * 0x44]
```

The loader verifies that the resulting PSX region has the expected optional
hooks, texture-WIB, and colour-pulse tables before accepting it. It then calls
the skater-specific model-table setup (`0x004b5010`) and returns the PSX region
slot. This is a separate consumer of the same object/model/model-pointer
structures used by Warehouse, with the scene blockmap absent for ordinary
skater assets.

## Shared SK2DEF source

The default/custom skater definition is a separate, directly named PSX asset.
`0x0042f950` (source path `H:\TonyHawk\Pc2\custmgr.cpp`) opens the literal
`sk2def.psx` through the raw resource reader `0x00448dd0`, relocates its
internal pointer/offset tables with `0x004b5010`, selects a free PSX region
slot through `0x004b4640`, and hands the rebased buffer to the common parser
`0x004b2450`. The resulting slot is stored in the caller's region-handle
table and receives the generated name `sk2def%d`.

```text
SK2DEF.PSX
  -> 0x0042f950 custom/default skater definition loader
  -> 0x00448dd0 raw resource read
  -> 0x004b5010 in-buffer pointer/offset relocation
  -> 0x004b4640 free PSX region slot
  -> 0x004b2450 common PSX parse
  -> DAT_0056d440 / DAT_0056d43c region tables
  -> "sk2def%d" named region
  -> player/custom-skater setup
```

`0x004b5010` is not a second format parser: it walks the definition's
model-pointer table and the fixed-size model/object sections and adds the
loaded-base delta to embedded pointers before the common PSX parser sees the
buffer. This is the runtime reason `SK2DEF.PSX` can share the ordinary PSX
object/model representation while still using a definition-specific load
entry point.

The shipped `SK2DEF.PSX` cross-check is exact: size `294824`, SHA-256
`656ccc5d0f2e695713aae7acba808624c6db752220d665273b5057c46d355e4c`, version 4,
marker 2, 110 objects, 110 models, 2,690 vertices, 5,461 normals, 2,771
faces, and 37 texture-name hashes. The 110/110 object/model counts and the
37-entry texture table are offline facts; the loader call chain and generated
region name are static runtime facts. No individual definition object is
claimed to correspond to a particular gameplay skater until a live slot and
consumer witness is captured.

## Player resource handoff

`0x004521c0`, called from `Front_LoadGame` for each active player, selects the
skater/costume resources and creates the player-side model object. Its two
branches cover ordinary and special/custom skater records, but both store the
resulting runtime model handle in `DAT_00568648[player]` and the player-side
resource pointer in `DAT_0056a848[player]`.

After the level resources are loaded, `Front_LoadGame` loads `sk2anim.PSX` and
calls `0x00452390` for each active player. That helper shares the animation and
hierarchy tables from the animation region into the selected player region.

The combined player path is:

```text
skater/costume selection
  -> 0x004521c0 player resource setup
  -> 0x004b43e0 "%s.psx" skater model load
  -> 0x004b2450 common PSX parser
  -> DAT_0056d43c / DAT_0056d444 / DAT_0056d448 region tables
  -> 0x00452390 animation/hierarchy sharing
  -> 0x00480730 runtime animation selection
```

The exact model filename for each custom-skater record is still a data-table
question; the disk-to-runtime loader contract is independent of those names.

## Live player-model witness

A controlled level-launch run stopped at the player setup boundary
`0x004521c0`, then at the named model loader `0x004b43e0`. The loader's first
argument was the live base name `"hawk2"`, and the game immediately reported
`Loading Player: Tony Hawk` before proceeding through the common PSX region
path. This is a runtime witness for the ordinary player branch, independent of
the static `%s.psx` filename construction.

The same run reached `0x004d8df0` while the player assets were being prepared
with material/checksum `0xd783cf21`. The run used the one-shot level override,
so `DAT_006a0548` still contained `"Hangar"` even though the load log reached
`Loading Level: Warehouse`; this hash observation is retained as a player-load
witness, not as proof of Warehouse bitmap residency.

## PSH part manifest and player spool

The player spooler provides a second, more concrete resource path around the
model loader. `0x004a6bf0` resets the spool state, selects up to six active
player records, and queues two resources for each selected record:

```text
player/costume record
  -> name resolver 0x004b1700
  -> format "%s.psh"
  -> 0x004b5370(name, mode=0, ...)
  -> format "%s.psx"
  -> 0x004b5370(name, mode=1, heap=1, ...)
  -> spool finalization 0x004b5580 / 0x004b5620
```

The same function loads the face image for each selected player through
`0x00457420`, using the fixed `s2face*.bmp` table and 64x64 dimensions. This
is an independent bitmap consumer from the level texture path and establishes
that the player-facing image is a runtime resource, not just front-end text.

`0x004b47a0` is the synchronous PSH entry point. It constructs `%s.psh`,
reads the file through `0x00448dd0`, and passes the buffer to `0x004b4940`.
The latter parses the text manifest by locating `.spart` records, returns the
part count through its output argument, and writes pointers to the discovered
part records and names into caller-provided arrays. It also normalizes a
`%s.spart` companion name. The parser enforces a caller-supplied maximum and
raises the exact `No %s parts found in %s.PSH file!` and `Too many pieces in
%s.psh` diagnostics, so the count and pointer-array contract is confirmed.

The part-line naming convention is not yet assigned to a complete C++ model
class: the parser's operational output is a set of pointers into the loaded
PSH text, with underscore/newline termination. The appearance merge at
`0x00480d90` compares the animation and model part-name pointers byte-for-byte
and calls `0x00480cd0` only for matching names, storing the matching
animation/model index correspondence in the runtime PSH record. The safe
runtime correspondence is therefore name-based rather than positional:

```text
PSH file buffer
  -> part manifest pointers/count
  -> queued PSX geometry resource
  -> player-region model tables
  -> player object and animation consumer
```

The PSH parser's output contract is independently recoverable from
`0x004b4940`. It constructs a `<base>.spart` search token, scans the loaded
PSH buffer, and for every match writes:

```text
part_count                         -> caller's u32 at param_5
part record pointer                 -> optional pointer array at param_4[index]
part name/text pointer              -> pointer array at param_3[index]
```

The parser bounds the count by the caller's `param_6`, rejects a zero-result
manifest, and terminates the extracted name at `_` and the following text at
the next newline. This is enough to recreate the manifest object as borrowed
pointers into the PSH buffer; it does not require copying the source lines.

Two asset pairs cross-check the manifest count against the common PSX
container: `C_TAXI.PSH` has six numbered parts and `C_TAXI.PSX` has six
objects/models, while `C_BULL.PSH` has eight numbered parts and `C_BULL.PSX`
has eight objects/models. The equal counts do not by themselves prove every
model part's public class name, but the merge helper independently proves the
per-part name-match and index-assignment operation used by the runtime model
set.

For the ordinary Tony Hawk pair, the checked-in PSH manifests make the
name-based remap concrete. `SK2ANIM.PSH` uses animation indices 0..18, while
`HAWK2.PSH` uses the same named parts but stores the right and left leg pieces
in a different order. The executable's `0x00480d90`/`0x00480cd0` merge path
therefore produces this independently supported correspondence:

```text
animation index       0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18
selected HAWK2 index  0  3  1  2  6  4  5  7  8  9 10 11 12 13 14 15 16 17 18
part                  pelvis, right thigh/shoe/shin, left thigh/shoe/shin,
                      torso/arms/head, board/front wheel/back wheel
```

The merge writes the selected-model index to the animation object's
`+0x150[index]` byte and sets `+0x14c` when at least one match is present. The
per-part renderer consumes exactly that table before reading the 0x18-byte
pose records, so this is a disk-manifest -> runtime remap -> renderer bridge,
not merely a count comparison.

### Player hook packet

The player setup also binds a small static hook packet after selecting the
named PSX region. `0x00468e90` calls `0x0046d940`; the latter calls
`0x0047fe30` (`Object_SelectNamedPsxRegion`) and then selects
`DAT_00534620 + object[+0x2cc0] * 0x14` before calling
`0x00464b90` (`M3D_ReadHooksPacket`). `0x00464b90` has a deliberately small
contract: it stores `packet + 4` in the global hook table slot indexed by the
object's PSX region byte at `+0x1f`.

The initialized table has 11 entries. Each entry is 0x14 bytes: a 4-byte
header followed by two 8-byte records. The header words are not assigned a
semantic name here; the records' final signed-16 field is the part index
consumed by `0x00464d10`/`0x00464e90` through the bound hook table. The exact
static values are:

```text
table index   header   record 0 (4 x s16)   record 1 (4 x s16)
0             0,1      0,0,0,18             0,0,0,3
1..10         0,1      0,0,0,12             0,0,0,0
```

This proves the packet is player-model hook setup and gives the runtime
selection key (`object + 0x2cc0`), while leaving the two record prefixes and
the meaning of the unused header open. The first three record words are not
dead padding: `0x00464e90` passes them to `0x004f5160`, which copies them into
the lower transform state before applying the selected animation part. Their
individual axis/vector meanings remain open. `M3D_GetPartPosition` and
`M3D_GetHookPosition` then select the animation pose/cache, apply the hook's
part mapping, and return a world-space position; the known consumers include
rail/camera attachment and dust placement.

## Spool queue and PSH-to-PSX handoff

`0x004b5370` is the bounded player spool queue insertion point. It accepts a
resource name, a PSH/PSX mode, a heap/size argument, and a final request flag;
it rejects more than `0x40` queued items and stores each item on a `0x28`-byte
stride. The active player path calls it with the same base name twice:

```text
<name>.psh -> mode 0, final argument -1
<name>.psx -> mode 1, heap selector 1, size/flags 0x10800
```

`0x004b5620` consumes one queue item. The PSX item uses the normal
`0x00449030` open, `0x0046f490` allocation, and `0x00449230` read handoff,
then stores the loaded buffer in the spool record. The PSH-side branch calls
`0x004b37a0`, which registers/queues the associated PSX region and appends the
`.psx` suffix through `0x004b3750`. `0x004b37a0` searches the 20 named PSX
region slots first, otherwise writes a bounded region name, a request flag,
and a queue entry. It rejects a base name that would exceed eight characters,
then writes the base name plus `.psx` into the queue's 13-byte name field.
Its queue entries are 0x11 bytes and carry active, mode, region-slot, and
request-flag bytes after the name.

### Player spool lifecycle

The ownership boundary is now also supported by the manager lifecycle around
the queue insertion point. `0x004b5200` constructs a `0xa10`-byte manager
with a vtable, zeroes its counters, and initializes 0x40 entries beginning at
`manager + 0x04` at a 0x28-byte stride. The entry array ends exactly at
`manager + 0xa04`; the manager counters are:

```text
manager + 0xa04  queued_count
manager + 0xa08  consume_index
manager + 0xa0c  state
```

Each entry initializes its processed byte at `+0x04` to zero, its region
handle at `+0x1c` to `-1`, and its direct-file buffer pointer at `+0x20` to
zero. `0x004b5370` copies the base name at `+0x05`; the next field starts at
`+0x18`, so the entry has a confirmed 19-byte name span (`+0x05..+0x17`,
including its terminator). The mode discriminator at `+0x18` is `1` for a
PSH/region request and `0` for a direct PSX file. The heap selector passed to
the allocator is stored at `+0x24`.

`0x004b5580` starts the first pending item and sets state to `1`;
`0x004b5300` completes the current item, either draining the PSX region
spooler (`0x004b3df0`) for a PSH request or synchronizing the direct file
read (`0x00449660`), then advances the consume index through `0x004b57d0`.
When the index reaches `queued_count`, state returns to zero. A nonzero state
remaining after a normal finalize is promoted to state `2`, which is the
observable wait/synchronization state used by `0x004b5350`.

For direct PSX entries, `0x004b5a00` releases the allocated `+0x20` buffer via
`0x0046f4d0`, clears the pointer, and marks the entry processed. For PSH
entries it requires the associated region load to have completed, publishes
the region through `0x004b2450`, removes/clears the named region through
`0x004b3270`, drains the shared PSX spooler, and resets the entry's `+0x1c`
handle to `-1`. `0x004b3270` performs a case-insensitive name lookup through
`0x004b3230`; if the matching region is active, it delegates the actual slot
clear to `0x004b32f0`. The manager reset `0x004b52b0` releases all 0x40 entry
indices and resets the counters and global spool-busy flag.

One implementation detail should remain explicit in a recreation: the
requested-size/limit argument supplied to `0x004b5370` is written at the
*next stride's base word* (`manager + (entry_index + 1) * 0x28`), and the
consumer reads it from that same location. This is an observed staging-word
layout, not evidence for an ordinary `entry.request_size` field inside the
0x28-byte entry. The shared `DAT_005615dc` flag is set while a spool operation
is pending and cleared at synchronization/completion boundaries.

This connects the text manifest and binary region at the runtime boundary:

```text
<name>.PSH
  -> 0x004b4940 part pointers/count
  -> 0x004b5370 / 0x004b5620 spool records
  -> 0x004b37a0 named PSX-region queue
<name>.PSX
  -> 0x00449030 -> 0x0046f490 -> 0x00449230
  -> 0x004b2450 common PSX parse
  -> DAT_0056d428 / DAT_0056d438 / DAT_0056d43c
```

The loaded player region then shares animation/hierarchy tables through
`0x00452390` and is consumed by the player animation path. The exact C++
ownership class around the spool records remains open, but the queue layout,
file handoff, region-slot publication, and common consumer are supported.

This also explains why the direct `%s.psx` loader at `0x004b43e0` and the
player spool path both exist: the former parses a named model region directly;
the latter builds the asynchronous resource set, including the PSH manifest,
for the active player roster.

## Confidence and limits

- `confirmed`: eight-slot skater manager access, player-index mapping, `%s.psx`
  model-file construction, live ordinary `hawk2` model load, literal
  `SK2DEF.PSX` load and generated
  `sk2def%d` region naming, its pre-parse pointer relocation, PSH
  load/part-count and pointer-array contract, six/eight-part PSH↔PSX count
  cross-checks, common PSX parser entry, region-table storage,
  animation/hierarchy sharing call, the 19-part `SK2ANIM.PSH`↔`HAWK2.PSH`
  remap, and the `0x0046d940` static hook-packet bind through
  `PlayerHookPacketTemplates`.
- `observed`: the two player-resource setup branches and the optional hook,
  texture-WIB, and colour-pulse checks; the 0x40-entry player spool manager,
  its counters/state machine, 0x28-byte entries, direct-file release path,
  PSH region publication path, and shared spool drain; the PSX scheduling
  queue uses 20 0x11-byte entries.
- `inferred`: the precise C++ class types returned through
  `DAT_00568648`/`DAT_0056a848` and the model names selected by every skater
  record.
