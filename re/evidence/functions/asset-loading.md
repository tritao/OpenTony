# Runtime Warehouse PSX asset loading

Status: confirmed disk-to-runtime paths for Warehouse scene geometry and TRG gameplay asset consumers
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Branch: `re/asset-runtime`
Addresses: `0x00449030`, `0x004b39b0`, `0x004b2450`, `0x004647c0`, `0x004b2ac0`, `0x004a12d0`

This session follows the Warehouse environment PSX family. The trigger file is included where it identifies the level and causes the scene resource to be attached, but trigger-object pointers are not conflated with PSX scene-object pointers.

## Proven path

The controlled run used the normal single-session flow and the existing one-shot `tony-force-level warehouse` helper. The level-selection evidence is recorded in [level-load.md](level-load.md).

```text
Warehouse selection
    -> Front_LoadGame / 0x004524a0
    -> SkWare_T.trg through trig.cpp / 0x004c5130
    -> SkWare.psx through spool.cpp / 0x004b39b0
    -> raw PSX buffer allocation and load
    -> PSX environment parse / 0x004b2450
    -> model/object finalization / 0x004647c0
    -> environment-list attachment / 0x004b2ac0
    -> collision/ground consumer / 0x004a12d0
```

The first game-owned file-open routine after the runtime's file/archive layer is `0x00449030`. Its decompilation identifies `H:\TonyHawk\Pc2\fileio.cpp`; the trigger loader and environment spooler both call it. `0x00449030` returns the file size/resource handle used by the subsequent allocation/load stages.

For the trigger path, `0x004c5130` constructs `SkWare_T.trg`, calls `0x00449030`, allocates through `0x0046f490`, then completes the load with `0x00449230` and `0x00449660`. It relocates the `_TRG` node-offset table into `DAT_0056e210` and records the node count in `DAT_0056e214`.

For the geometry path, `0x004b39b0` is the spooler state machine. Its active file slot contains `SkWare.psx`; the state machine calls `0x00449030`, allocates the buffer with `0x0046f490`, and passes the loaded buffer into the PSX parser. The parser breakpoint observed `SkWare.psx` as slot 6, so this is not only a static filename match.

## Dynamic observation

Relevant output from the exact-build headless run:

```text
Loading Level: Warehouse
FILEIO_OPEN_TRG path="SkWare_T.trg"
TRG_PARSE count=313 table=0x005f46acc buffer=0x005f46ac0
FILEIO_OPEN_TRG path="SkWare.psx"
PSX_PARSE slot=6 raw=0x005d74ef0 object_count=252 model_count=288
PSX_FINALIZE slot=6 raw=0x005d74ef0 objects=0x005f34a5c \
  models=0x005d77270 object17=0x005f34f6c model17=0x005d78d2c
```

The `FILEIO_OPEN_TRG` label is the debugger label for a breakpoint at `0x00449030`; it does not mean that the PSX file is a trigger file. The same run also opened the expected auxiliary level resources `SkWare_L.psx` and `SkWare_O.psx`. Their parser counts were respectively `0/0` and `25/25`; the main scene resource is the `252/288` `SkWare.psx` parse above.

The trigger parser independently observed the Warehouse `_TRG` header as version 2 with 313 nodes. Its first object factory calls produced game-owned runtime pointers, for example `TRG_OBJECT idx=17 ... TRG_OBJECT_RETURN ptr=0x005f404c0`; that pointer belongs to the trigger/object-manager path, not to PSX scene object 17 below.

## Offline-to-runtime correspondence

OpenTony's offline parser reports the following for `build/assets/all-pkr/files/data/SKWARE.PSX`:

- size `131124`, SHA-256 `d77cc2d18c684410c201edcd6fe05c27f90901f7c2cb0706161b20540b3c95b5`;
- version 4, marker 2, tag offset `0x1c5f8`, object count `252`, model count `288`;
- one blockmap with 400 cells and 1,480 object references;
- 3,390 vertices, 1,869 normals, 1,869 faces, and 89 texture-name hashes.

Offline scene object 17 is:

```text
flags          = 0
position_fixed = (29675520, -5033984, 27557888)
position       = (7245, -1229, 6728)
model_index    = 17
model_offset   = 15932 (0x3e3c)
model_size     = 668
model_flags    = 8
model counts   = vertices 20, normals 12, faces 12
model bounds   = (0, 0, 1229, -1228, 342, -343)
model name     = 0x50556ba0
```

The correspondence is independently supported at each boundary:

| Offline field | Runtime evidence |
| --- | --- |
| PSX header object count at raw `+0x08` (`252`) | `0x004b2450` allocates `4 + count * 0x4c` bytes for the runtime environment-object array. The dynamic parse reported `252`. |
| PSX model count at `raw + 0x0c + object_count * 0x24` (`288`) | `0x004b2450` relocates the absolute model-offset table beginning at `raw + 0x10 + object_count * 0x24`; the dynamic parse reported `288`. |
| Model 17 file offset `0x3e3c` | Runtime model table at `0x005d77270` contains pointer `0x005d78d2c` at index 17; `0x005d74ef0 + 0x3e3c = 0x005d78d2c`. |
| Model 17 header counts | `0x004647c0` reads the runtime model header's 16-bit vertex, normal, and face counts and iterates the variable-length face packets. The offline values are 20/12/12. |
| Scene object 17 | Runtime object-array base `0x005f34a5c`, plus the parser's 4-byte count prefix and `17 * 0x4c`, gives `0x005f34f6c`, the pointer printed by the finalizer breakpoint. |
| Object position and model index | `0x004b2450` copies each disk object's position words and 16-bit model index into the 0x4c-stride runtime record. `0x004647c0` then uses the record's model index to select from `DAT_0056d43c[slot * 0x11]`. |

This establishes the useful bridge for the main scene asset:

```text
SKWARE.PSX object #17 / model #17
    ↕
runtime slot 6 object record 0x005f34f6c
runtime model pointer       0x005d78d2c
```

The runtime slot arrays have the following supported roles:

- `DAT_0056d440[slot * 0x11]`: loaded raw PSX buffer;
- `DAT_0056d438[slot * 0x11]`: count-prefixed, 0x4c-stride environment-object records;
- `DAT_0056d43c[slot * 0x11]`: relocated pointer array for the PSX model blocks;
- `DAT_0056db28`: head of the attached environment-array list. `0x004b2ac0` links the current environment through its `+0x20` next pointer and records region metadata in `DAT_0056db20`.

The list roots are materially distinct. `0x004647c0` builds the `+0x20`
chain inside the region's count-prefixed PSX object array, and
`0x004b2ac0` appends that array to `DAT_0056db28` (the environment-list head)
before recording the returned environment slot in `DAT_0056db20`. It does not
insert those records into `DAT_0056af40`. The type-192 constructor instead
inserts its heap object into `DAT_0056af40` directly, and the separate
`0x004a12d0` platform consumer walks that game-object root. This is direct
static evidence against deriving type-192 heap nodes from every PSX environment
object by position or model alone; the two products share model-table access
but have different list ownership and finalization inputs.

The 36-byte disk object is copied into the 0x4c-byte runtime record with a
vtable/dispatch prefix and runtime-owned links. In addition to the position,
model, and slot fields above, the parser copies the disk flags into runtime
`+0x04`, copies three 16-bit transform components into `+0x14/+0x16/+0x18`,
and preserves the trailing source word at `+0x24`. The constructor initializes
the runtime transform defaults at `+0x28/+0x2a/+0x2c` to `0x1000`. The
collision face test explicitly rejects a nonzero `+0x14/+0x16/+0x18` object as
a rotated object, so these are not arbitrary padding; the public rotation/
scale semantics remain unassigned. Finalization additionally derives a model
feature bit at `+0x19` from the selected model header.

The parser's copy loop makes the source-to-runtime offsets more precise:

```text
disk +0x00 flags       -> runtime +0x04 flags
disk +0x04/+0x08/+0x0c position -> runtime +0x08/+0x0c/+0x10
disk +0x10 low/high 16 bits -> runtime +0x14/+0x16 s16 transform components
disk +0x14 u16         -> runtime +0x18 s16 transform component
disk +0x16 u16 model   -> runtime +0x1a model index
disk +0x18/+0x1a       -> runtime +0x1c four-byte trailing transform/source area
disk +0x20 u32         -> runtime +0x24 source RGBX word
```

The disk word at `+0x1c` is not copied by this loop; runtime `+0x20` is
reserved for the intrusive next pointer. This distinction prevents the
source-format tail from being mistaken for a contiguous runtime struct.

The native recreation now preserves this boundary in
`src/assets/psx_runtime.*`: `PsxRuntimeEnvironment` owns the count-prefixed
`0x4c` object images and a non-owning relocated model-pointer table. It also
builds the shared `0x2c` material records from the PSX texture-name/checksum
table, retaining checksum `+0x18`, face-reference count `+0x10`, and the
source-texture-index to shared-record mapping while leaving retail heap links
unmaterialized. `LevelRenderSnapshot` exposes that resolved material index and
checksum to a renderer backend. The Warehouse regression joins object/model
17 and material indices 50/51 to their independently observed disk hashes.

The native end-to-end geometry path is now executable through the same owned
objects. `LevelRuntime` loads the `TRG`/`PSX` pair (or the corresponding
`ALL.PKR` entries), `PsxRuntimeEnvironment` owns the relocated object/model
and material view, and `LevelRenderSnapshot` copies the scene registry's
active entity bindings into renderer-independent faces. `GameplaySession`
then passes that snapshot and the live raw camera state to
`RenderPacketBuilder`, which preserves the retail `0xb0` polygon boundary,
three/four-vertex choice, object-plus-camera fixed-point transform, material
checksum, and the explicit projection callback. Textured UVs are normalized
with the recovered half-texel rule when realized dimensions are available;
`LevelRuntime` now materializes the proven PC texture record for external
hash-named BMPs and inline PSX images, so the four Warehouse bitmap witnesses
reach this same packet path. Genuinely unresolved materials remain source-UV
based. This is the first native disk -> runtime object -> material/image record
-> scene snapshot -> render packet chain, covered by the Warehouse
`opentony_gameplay_session_test` and `opentony_render_packet_builder_test`.

The same native packet boundary now includes the confirmed TRG pickup path.
`LevelRuntime` retains finalized `ITEMS.PSX`/`SKMEDALS.PSX` region runtimes;
`LevelRenderSnapshot` joins a pickup's source TRG node to its 0x100-byte
powerup record, copies the resolved checksum-selected model and position, and
emits its faces with the region-local material table. Thus Warehouse node 17
(subtype 6) reaches `ITEMS.PSX` model 5 and the same `0xb0` packet builder as
the environment objects. The six Warehouse type-5 rows with no recovered
checksum selection remain geometry-less runtime entities, preserving the
unknown branch explicitly. Package-only `ALL.PKR` construction follows the
same region path.

The parser also resolves face references against a checksum/material table and processes the post-model image/palette tables. The material ownership is now statically proven. After `0x004b20f0` locates the post-tag tables, `0x004b2450` relocates the scene checksum entries by `DAT_0056db3c`, looks each checksum up through `0x004b2030`, and allocates a missing 0x2c-byte record through `0x004b1f70`. The lookup uses `checksum & 0x1ff` as a bucket index at `DAT_0056cc18`; the record stores the checksum at `+0x18`, a reference count at `+0x10`, and doubly-linked list pointers at `+0x24/+0x28`.

For every model face whose packed flags include bit `0x01`, the loader replaces the disk texture/checksum-table index at `face + 0x10` with the corresponding `RuntimePsxMaterialRecord*`. This is the disk-to-runtime material bridge; it is separate from the later inline-image palette/cache and PC D3D texture records described in [texture-runtime.md](texture-runtime.md).

The independent Warehouse data provides a concrete table witness. `SKWARE.PSX` has an 89-entry checksum table beginning at file offset `0x1fec4`; model 140's face indices are `50, 51, 51, 51`, which resolve to checksums `0xd783cf21` and `0x288ca4c4`. The runtime loader therefore rewrites those four face references to two shared hash-keyed material records, with the three faces using `0x288ca4c4` sharing one record. The allocation addresses are run-specific, but the hash bucket, key field, sharing, and face-field rewrite are independently supported by the loader disassembly.

The adjacent [texture-runtime.md](texture-runtime.md) evidence covers the independently supported PSX palette records, converted-palette cache, and PC texture records used by the image-upload side.

## Downstream consumer

The ground/collision path at `0x004a12d0` is a confirmed consumer of the runtime environment representation. It walks a runtime object list, reads the object's slot byte and model index, indexes `DAT_0056d43c[slot * 0x11]`, reads the selected model's bounds, scales those bounds by `0x1000`, and compares them with the player's `+0x08/+0x0c/+0x10` position/AABB. A qualifying model/object is passed to `0x0049f4c0` with the player's `+0x4c` response vector.

This proves the disk-derived model pointer table is consumed by collision/ground processing. The more detailed [physics.md](physics.md) path now follows the selected PSX face through the collision query outputs and surface-flag extraction into skater response state. It does not claim that every node in the separate `DAT_0056af40` trigger-object list is a `SKWARE.PSX` object; renderer packet and hardware semantics remain separately documented in [renderer.md](renderer.md).

The render consumer is now dynamically bridged as well. In a Warehouse
gameplay frame (`CurrentLevel = 12`), the attached environment list entered
`0x0045f530`, and runtime record #140 at `0x005f373ec` selected slot 6/model
140. Its fixed-point position was `(37642240, -16384, 45539328)`, exactly the
offline parser's `SKWARE.PSX` object #140 position; slot 6's model table
selected packet `0x005d83078`, whose flags and counts were `0x48` and `8/4/4`,
matching offline model #140. The packet was then observed as the first
argument to the state-geometry consumer `0x004d14d0` with state `0x800`.
The complete render-side branch split and witness are recorded in
[renderer.md](renderer.md).

The other proven consumers are recorded separately:

- [fileio-runtime.md](fileio-runtime.md) expands the backend-handle and PRE
  fast-path below the first game-owned open;
- [blockmap-collision.md](blockmap-collision.md) traces the type-10 blockmap
  into cell pointers and face collision;
- [collision.md](collision.md) records the shared query/result ABI, face cache,
  nearest-hit arithmetic, and raw face-flag handoff;
- [renderer.md](renderer.md) traces the attached environment list into model
  packet submission; and
- [texture-runtime.md](texture-runtime.md) traces palette/texture records into
  the PC texture manager.
- [ground-movement.md](ground-movement.md) traces the selected collision result
  through grounded response, orientation, and position commit.
- [skater-asset-runtime.md](skater-asset-runtime.md) traces named player/skater
  PSX model regions into the player resource and animation handoff.
- [skater-asset-runtime.md](skater-asset-runtime.md) also records the native
  `SK2ANIM.PSH`/`HAWK2.PSH`/`HAWK2.PSX` name join and direct player model-slot
  binding; `player_resource_spool.*` preserves the 0x40-entry PSH/direct-PSX
  queue, file dispatch, parsed ownership, and completion state, including the
  package-backed `ALL.PKR` variant.
- [items-runtime.md](items-runtime.md) traces `ITEMS.PSX` and optional
  `SKMEDALS.PSX` regions through TRG powerup objects into the shared render
  list, and traces `BITS.PSX` type-0x45 named resources into the effect path.
- [psx-tags-runtime.md](psx-tags-runtime.md) inventories the common PSX
  post-model tag dispatch and its texture-WIB, colour-pulse, animation,
  hierarchy, RGBS, vertex-colour, and blockmap runtime products.
- [trick-object-runtime.md](trick-object-runtime.md) traces Warehouse TRG
  type-12/14 nodes through checksum-bearing `RuntimeTrickObject` records into
  player/rail activation and the per-frame trick-object consumer.
- [trg-runtime.md](trg-runtime.md) traces type-1 traffic nodes through the
  proven `0x1e8` runtime allocation, subtype-selected `C_*.PSX` region, model
  selector, and positional-sound metadata.

`LevelRuntime::resources()` now applies the same case-insensitive basename
lookup to the package directory when no loose-file catalog is present. The
package-only Warehouse fixture therefore reports the two TRG resource
requests as available archive entries, matching the loose-file path without
claiming that the binding itself has uploaded or played the resource.
- [psx-lifecycle.md](psx-lifecycle.md) traces the inverse region clear path,
  including shared-material usage decrement, blockmap reset, and environment
  detach.

The repository-wide extension coverage and negative-result audit is recorded
in [asset-runtime-audit.md](asset-runtime-audit.md). It checks all 3,771
extracted `ALL.PKR` files against these runtime families and keeps the
console/tool-only formats outside the proven PC path.

## Cross-family runtime inventory

The same common resource path has now been followed far enough to establish
these independent asset/runtime boundaries:

| Asset family | Runtime boundary | Consumer or handoff |
| --- | --- | --- |
| `SKWARE.PSX` / `SkWare_T.trg` | 0x4c-stride PSX environment records, relocated model pointers, and TRG object families | renderer, blockmap collision, baddy and traffic object managers |
| `SK2DEF.PSX` | rebased shared/default skater PSX region named `sk2def%d` | player/custom-skater model setup |
| `SK2ANIM.PSX` | compacted animation table at `DAT_0056d444[slot * 0x11]` | gameplay animation start/update |
| skater `PSH`/`PSX` resources | player spool records, part manifests, and model regions | player object/model and animation handoff |
| player/skater runtime object | `0x3538` skater object plus contained `0x674` camera | animation, physics, collision, and renderer consumers |
| BMP / hashed PC textures | face-image loads, PRE-embedded images, and hash-keyed PC texture records | UI, player appearance, and renderer texture state |
| PSX image/palette tables | PSX palette handles, 0x18 converted-palette cache nodes, 0x2c PC texture records | bitmap open, conversion, Direct3D upload |
| PRE/PKR2/direct files | shared abstract resource handles and synchronized buffers | PSX, FNT, WAV, replay, and legacy module loaders |
| `FNT` | bounded font slots, glyph records, and image resources | text renderer |
| VAB-selected `audio/*.wav` sound resources | sound-bank records and runtime PCM buffers; packaged `.SFX` tables are not opened by the PC executable | sound playback |
| `CDPARKS.TXT` / `MUSIC.TXT` / `CREDITS.TXT` | borrowed label pointers and 0x44/0x10/0x18-byte presentation records | park menu and credits/music update/render paths |
| startup `*.STR` / `GrayMat.dat` media | blocking PC movie/media reader state | logo, intro, and startup media playback; no gameplay object handoff |
| `PRK` custom parks | level-generation grid/items and finalized runtime region | custom-park level generation |
| `ITEMS.PSX` / `SKMEDALS.PSX` | named item/medal PSX slots and 0x100-byte TRG powerup objects | powerup update, glow state, and renderer object list |
| `BITS.PSX` | type-0x45 named-group records and runtime resource list | shadow/effect named-resource lookup |
| `SKWARE_T.TRG` type 12/14 nodes | 0x18-byte checksum-linked `RuntimeTrickObject` records | player/rail trick activation and per-frame tint/update |
| `THPS2_*.SAV` replay/card buffers | fixed `SC` header, manager records, and caller-registered aligned buffers | career/replay/custom-park save, load, delete, and frontend state; see [save-runtime.md](save-runtime.md) |
| career `.SAV` payload | `0x1dc4` copied career image, 20 `0x104`-byte skater records, selectors, trick bits, and custom appearance blob | career load/save preparation, skater selection, trick configuration; see [career-save-runtime.md](career-save-runtime.md) |
| `TRICKS.BIN` / `CRETEX.BIN` | relative section pointers, bytecode cursor/state, input-history/queued-start path, animation/physics writes, and trick point-stack handoff; texture-set records | trick manager/skater script executor, panel score-stack refresh, and texture-set manager |
| remaining `.BIN`, `.SEQ`, `.SBL`, `.TST`, `.TDF`, `.TAG`, `.TS`, `.NT`, `CD.*` tables | console/tool/build metadata or unconnected archive tables; no PC consumer proven in this build | intentionally outside the runtime recreation boundary |

The remaining gaps are semantic rather than missing top-level ownership paths:
the original names of several renderer state bits and Direct3D enums, the
face-surface flag names and a few state-specific response predicates, the
final world-facing interpretation of the now-recovered steering-target
integrator, and the exact class names behind a few legacy module and
audio/texture handles.

## Remaining reverse targets

The next useful work is bounded to these consumer-side questions; none of them
requires reopening the already-proven Warehouse file-to-object path:

| Target | Existing boundary | Evidence needed to close it |
| --- | --- | --- |
| TRG gameplay command names and state effects | `0x004c5dc0`, `0x004c7a00`, `0x004c7c50` | correlate remaining decoded command operands with controlled activation; script-object/rail/camera-point allocations, the `0xc9` gap table, the `0x0098` linked-restart path, and the `0x009e` level-event initialization are now bounded, while larger list ownership remains open |
| TRICKS remaining physics/table semantics | `0x004be450`, `0x004904d0`, `0x0048cbc0`, `0x0048cd40` | retain raw script fields and score-table constants until their public gameplay names are independently recovered; landed-total/best-score commit is proven in `bin-runtime.md` |
| public surface flags and remaining physics predicates | `0x0048ea80`, `0x0049db80`, `0x00497f40`, `0x00494210`, `0x00499710`, `0x004995d0` | raw state names are cross-build corroborated in `physics.md`; retain face-bit names and state-specific response predicates until a PC-side consumer proves them |
| final renderer/material meanings | `0x0045f530`, `0x004d14d0`, and [texture-runtime.md](texture-runtime.md) | provide the missing Warehouse hash-named bitmaps or capture equivalent texture resources, then correlate packet fields with submitted material state |
| individual named PSX/PSH selection coverage | `0x004b0f80`, `0x004b1700`, `0x004b1980`, `0x004521c0`, plus [skater-asset-runtime.md](skater-asset-runtime.md) | all 20 static rows, four-position count/filter, four enabled custom rows, hidden rows, and name-return contract are proven; only the producer that replaces custom `error` names remains open |
| legacy module and packaged sound consumers | `0x004ac0c0` generic `.bin`/`.rel` helper and the negative `.SFX` search in [legacy-assets.md](legacy-assets.md) | locate a direct PC caller or producer; absent that, these remain outside the faithful PC runtime path |

Startup `*.STR`/`GrayMat.dat` playback is intentionally not in this queue:
[`startup-runtime.md`](../startup-runtime.md) proves a separate blocking media
consumer with no gameplay-object handoff.

## Confidence and limits

- `confirmed`: exact Warehouse file names, file-open path, PSX parser entry, object/model counts, model-pointer relocation, object-array allocation size/stride, object-17/model-17 correspondence, checksum-bucket material allocation, the textured-face index-to-runtime-material rewrite, the four-hash Warehouse BMP -> PC texture upload witness recorded in `texture-runtime.md`, the `ITEMS.PSX`/`SKMEDALS.PSX`/`BITS.PSX` runtime bridges recorded in `items-runtime.md`, the TRG type-12/14 checksum/object bridge recorded in `trick-object-runtime.md`, the linked restart/gap/event/camera-point TRG bridges recorded in `restart-runtime.md`, `gap-runtime.md`, `level-event-runtime.md`, and `trg-runtime.md`, the career `.SAV` image/record bridge recorded in `career-save-runtime.md`, and the `TRICKS.BIN`/`CRETEX.BIN` runtime boundaries recorded in `bin-runtime.md`.
- `observed`: trigger node count and trigger-object allocations; auxiliary `SkWare_L.psx`/`SkWare_O.psx` slot contents.
- `inferred`: semantic names for some runtime record fields beyond position, model index, next pointer, and model-derived flags.

The startup movie/media row is an observed media path from
[`startup-runtime.md`](../startup-runtime.md), included to make the inventory
complete; it does not converge on the gameplay resource/object structures.

The primary geometry trace remains focused on the disk-to-environment-object
bridge. Renderer, texture, PRE, TRG, animation, item, replay, audio, text,
custom-park, lifecycle, and physics boundaries are recorded in their own
evidence files so those paths can be recreated without weakening the geometry
proof. Remaining questions are semantic or rare-branch coverage, not missing
top-level ownership of an active archive family.
