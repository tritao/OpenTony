# Skater PSX model-region loading

Status: confirmed named skater-model PSX load, shared `SK2DEF.PSX` source,
complete static skater/costume selector table, player-spool PSH/PSX handoff,
and player-region handoff
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0042f950`, `0x00448dd0`, `0x004b5010`, `0x004521c0`,
`0x004a6bf0`, `0x004b0c60`, `0x004b1430`,
`0x004b0f80`, `0x004b1700`, `0x004b1980`, `0x004b43e0`, `0x004b47a0`,
`0x004b4940`,
`0x004b5370`, `0x004b5620`,
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

`0x004b1980` maps a logical custom-player index to the corresponding enabled
skater record by scanning 20 records at `DAT_0053a514`, whose stride is
`0x170`. It tests record flag bit `0x02`; in this executable logical custom
indices `0..3` therefore select rows `13..16`, while an index beyond the four
enabled records reaches the out-of-range diagnostic. This is the stable
boundary between front-end player selection and the skater manager's
model/costume slots.

The concrete name selector is `0x004b1700`. It validates the requested costume
against the count returned by `0x004b0f80`, then returns the selected base name
from the static table at `DAT_0053a3b8` using the exact index expression
`(costume_index + skater_index * 0x2e) * 2`. The table therefore has a
0x170-byte logical row per skater. `0x004b0f80` counts only the first four
logical positions and applies the record's optional flag filters before the
selector accepts a costume index; the `* 2` stride selects every other pointer
slot in the static table. The returned name is the value that the `%s.psx` and
`%s.psh` loaders consume. `0x004521c0` uses this selector on both its ordinary
and special/custom branches before publishing the player resource handles.

The complete static table and count behavior are independently recoverable:

| row | record flags | first four selector names | accepted count |
| ---: | ---: | --- | ---: |
| 0 | `0x208` | `hawk2`, `hawk2b`, `secret4`, empty | 2 (flag `0x08` stops before slot 2) |
| 1 | `0x000` | `burnq2`, `burnq2b`, empty, empty | 2 |
| 2 | `0x000` | `cab`, `cab2b`, empty, empty | 2 |
| 3 | `0x000` | `campb2`, `campb2b`, empty, empty | 2 |
| 4 | `0x000` | `glif2`, `glif2b`, empty, empty | 2 |
| 5 | `0x000` | `koston`, `koston2b`, empty, empty | 2 |
| 6 | `0x000` | `lasek2`, `lasek2b`, empty, empty | 2 |
| 7 | `0x000` | `mullen`, `mullen2b`, empty, empty | 2 |
| 8 | `0x000` | `muska2`, `muska2b`, empty, empty | 2 |
| 9 | `0x000` | `rynld2`, `rynld2b`, empty, empty | 2 |
| 10 | `0x000` | `rowley2`, `rowley2b`, empty, empty | 2 |
| 11 | `0x001` | `steam2`, `steam2b`, empty, empty | 2 |
| 12 | `0x000` | `thomas2`, `thomas2b`, empty, empty | 2 |
| 13 | `0x002` | `error`, `error`, empty, empty | 2 |
| 14 | `0x002` | `error`, `error`, empty, empty | 2 |
| 15 | `0x002` | `error`, `error`, empty, empty | 2 |
| 16 | `0x002` | `error`, `error`, empty, empty | 2 |
| 17 | `0x060` | `secret1`, `secret1b`, empty, empty | 2 |
| 18 | `0x0a1` | `secret2`, `secret2b`, empty, empty | 2 |
| 19 | `0x120` | `secret3`, `secret3b`, `secret3c`, `secret3d` | 4 |

Rows 13..16 are the four enabled custom records selected by
`0x004b1980`; their built-in `error` names are placeholders in the static
table, so the custom front-end/name replacement remains a separate producer
question. Rows 17..19 are real hidden/skater rows in the same selector table,
and row 19 is the only row with four accepted static model names. The live
`hawk2` load is one concrete row/slot witness; the table above closes the
remaining static name coverage without claiming which front-end screen
selects each hidden row.

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
The PSH files are C-preprocessor-style text headers, not separate `.spart`
files. The latter parses the text manifest by locating the `<base>part_` token
(for example `HAWK2PART_`), returns the part count through its output argument,
and writes pointers to the discovered part definitions and names into
caller-provided arrays. It also normalizes the search token before scanning.
The parser enforces a caller-supplied maximum and
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
`0x004b4940`. It constructs a `<base>part_` search token, scans the loaded
PSH buffer, and for every matching `#define <base>part_<model>_<part>` line
writes:

```text
part_count                         -> caller's u32 at param_5
part record pointer                 -> optional pointer array at param_4[index]
part name/text pointer              -> pointer array at param_3[index]
```

The parser bounds the count by the caller's `param_6`, rejects a zero-result
manifest, and terminates the extracted name at `_` and the following text at
the next newline. This is enough to recreate the manifest object as borrowed
pointers into the PSH buffer; it does not require copying the source lines.

The archive-wide manifest check closes the count boundary for the player
family. All 106 extracted PSH files have contiguous zero-based part indices.
104 have an exact same-base PSX companion, and every one of those 104 pairs
has equal PSH part count and PSX object/model counts. The two unmatched
manifests are `SK2ANIM1.PSH` and `SKED.PSH`: the former has no same-base PSX
file, while the latter is an editor/shared manifest whose related geometry is
split across `SKED1.PSX` through `SKED4.PSX`. They remain explicit alias/editor
cases rather than being fabricated into missing runtime regions.

The part-count distribution is also stable across the corpus: 91 manifests
have 19 parts, four have 151, two have 110, and the remaining nine have 1, 3,
5, 6, 8, or 29 parts. The 19-part `HAWK2.PSH`/`HAWK2.PSX` pair remains the
concrete name-remap witness used by the animated player path.

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
resource filename, a mode, a heap selector, and a final pad/capacity argument;
it rejects more than `0x40` queued items and stores each item on a `0x28`-byte
stride. The active player path constructs and passes the two filenames with
these exact arguments:

```text
<name>.psh -> 0x004b5370(name, mode=0, heap=0, pad=0xffffffff)
<name>.psx -> 0x004b5370(name, mode=1, heap=1, pad=0x10800)
```

The queue helper validates the filename through its first dot. With mode zero,
the `.psh`/`.psx` extension selects the corresponding internal branch and a
PSH request is stored as the base name for the later region queue. A nonzero
mode stores the direct-file branch. The only accepted heap selectors are zero
and one. Its final argument is written at the next entry stride's base word;
the direct-file consumer treats `0xffffffff` as no requested pad and otherwise
requires the resource to fit the requested capacity.

These call arguments and the inverse-facing stored mode are from the canonical
Ghidra decompilation of `0x004a6bf0` and `0x004b5370`; the native enum names the
stored consumer mode (`PshRegion=1`, `DirectPsx=0`) rather than the producer's
mode argument.

`0x004b5620` consumes one queue item. The PSX item calls the normal
`0x00449030` open, checks the staged pad/capacity, calls `0x0046f490` with the
entry heap selector, stores the resulting buffer at `+0x20`, and hands it to
`0x00449230`. Its assertions identify the failure boundaries: a non-positive
open result reports `file %s not found`, a resource larger than a requested
pad reports `Specified pad size is smaller than file size!`, and a null buffer
reports `not enough memory for %s`. The PSH-side branch calls `0x004b37a0`,
which registers/queues the associated PSX region and appends the `.psx` suffix
through `0x004b3750`. `0x004b37a0` searches the 20 named PSX region slots first,
otherwise writes a bounded region name and queue entry. It rejects a base name
that would exceed eight characters, then writes the base name plus `.psx` into
the queue's 13-byte name field. Its queue entries are 0x11 bytes and carry
active, mode, region-slot, and request-flag bytes after the name.

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

`0x004b5580` starts the first pending item, sets state to `1`, sets the shared
busy flag, and immediately calls `0x004b5620`; `0x004b5350` repeats that same
transition when state is `2`.
`0x004b5300` completes the current item, either draining the PSX region
spooler (`0x004b3df0`) for a PSH request or synchronizing the direct file
read (`0x00449660`), then advances the consume index through `0x004b57d0`.
When the index reaches `queued_count`, state returns to zero. A nonzero state
remaining after a normal finalize is promoted to state `2`, which is the
observable wait/synchronization state used by `0x004b5350`.

For direct PSX entries, `0x004b5a00` acts only on a processed entry, asserts
that the allocated `+0x20` buffer exists, releases it via `0x0046f4d0`, clears
the pointer, and clears the processed byte. For PSH entries it requires the
associated region load to have completed, publishes the region through
`0x004b2450`, removes/clears the named region through `0x004b3270`, drains the
shared PSX spooler, and resets the entry's `+0x1c` handle to `-1`; its processed
byte remains set. `0x004b3270` performs a case-insensitive name lookup through
`0x004b3230`; if the matching region is active, it delegates the actual slot
clear to `0x004b32f0`. The manager reset `0x004b52b0` first processes a state-1
request, then releases all 0x40 entry indices and resets the counters and
global spool-busy flag. Since the PSH release branch does not clear its
processed byte, that byte can remain set after reset until the slot is reused;
the direct-file branch clears it while freeing its buffer.

One implementation detail should remain explicit in a recreation: the
pad/capacity argument supplied to `0x004b5370` is written at the *next
stride's base word* (`manager + (entry_index + 1) * 0x28`), and the consumer
reads it from that same location before allocating the direct-file buffer.
This is an observed staging-word layout, not evidence for an ordinary
`entry.request_size` field inside the 0x28-byte entry. The shared
`DAT_005615dc` flag is set while a spool operation is pending and cleared at
synchronization/completion boundaries.

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
  `DAT_00568648`/`DAT_0056a848` and the front-end conditions selecting each
  remaining skater/costume row beyond the statically recovered name table.

The native PSH boundary is implemented in `src/assets/psh_asset.*`. It parses
the C-header-style `#define <base>PART_<model>_<part> <index>` stream into an
owned, source-ordered part list, retains both the model prefix and the trailing
runtime part label, and requires the contiguous zero-based indices observed by
the runtime parser. `match_psh_parts` reproduces the name-based merge: it joins
`SK2ANIM.PSH` and `HAWK2.PSH` by labels and returns the independently observed
animation-to-model map `0->0, 1->3, 2->1, 3->2`, with the remaining ordinary
parts unchanged. The native test walks the real 106-manifest corpus in
addition to the HAWK2 and C_TAXI witnesses.

The player queue lifecycle is implemented in
[`player_resource_spool.hpp`](../../src/runtime/player_resource_spool.hpp) and
`player_resource_spool.cpp`. It preserves the `0xa10` manager image, 0x40
entries at 0x28-byte stride, processed/name/mode/handle/heap offsets, the
separate requested-size staging word, and the loading/wait/idle state
transitions. `load_current()` dispatches the proven PSH versus direct-PSX
file path into the native manifest/archive readers and retains the parsed
resource until an explicit release. Each queue index has one stable owned
runtime-result slot, so appending another request cannot invalidate a loaded
object. The same queue now routes direct, package, and PRE-backed requests
through `ResourceLoader`: the resource bytes are copied and synchronized
before PSH/PSX parsing, and the resulting manifest/archive owns its own bytes.
The source kind and the direct-file allocation/pad size are retained on that
result for the direct/PKR/PRE boundary. The native request fixture uses the
confirmed PSH `(heap=0, pad=0xffffffff)` and PSX `(heap=1, pad=0x10800)`
arguments; unsupported heap selectors and undersized direct pads are rejected
before publication.

Malformed PKR or PRE payloads therefore fail before the queue's processed bit
or loaded-result slot is published. The native adapter leaves the current
request in its pending state so a caller can retry after replacing the source;
`reset()` is the explicit whole-manager failure cleanup, while `release()`
models the observed per-entry `0x004b5a00` cleanup after completion. These
exception-safety and retry semantics are native ownership guarantees, not a
claim that the retail executable recovers from every fatal file error.

The test uses a deterministic one-object PSX and one-part PSH fixture to cover
both PKR and PRE paths, destroys the package/PRE owner after loading, verifies
the parsed runtime objects remain usable, and checks malformed payloads do not
publish partial state. The real HAWK2 pair remains an additional corpus
witness when the extracted files are available.
