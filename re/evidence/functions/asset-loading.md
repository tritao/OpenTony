# Runtime Warehouse PSX asset loading

Status: confirmed disk-to-runtime path for Warehouse scene geometry
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

The parser also resolves face references against a checksum/material table and processes the post-model image/palette tables. The material ownership is now statically proven. After `0x004b20f0` locates the post-tag tables, `0x004b2450` relocates the scene checksum entries by `DAT_0056db3c`, looks each checksum up through `0x004b2030`, and allocates a missing 0x2c-byte record through `0x004b1f70`. The lookup uses `checksum & 0x1ff` as a bucket index at `DAT_0056cc18`; the record stores the checksum at `+0x18`, a reference count at `+0x10`, and doubly-linked list pointers at `+0x24/+0x28`.

For every model face whose packed flags include bit `0x01`, the loader replaces the disk texture/checksum-table index at `face + 0x10` with the corresponding `RuntimePsxMaterialRecord*`. This is the disk-to-runtime material bridge; it is separate from the later inline-image palette/cache and PC D3D texture records described in [texture-runtime.md](texture-runtime.md).

The independent Warehouse data provides a concrete table witness. `SKWARE.PSX` has an 89-entry checksum table beginning at file offset `0x1fec4`; model 140's face indices are `50, 51, 51, 51`, which resolve to checksums `0xd783cf21` and `0x288ca4c4`. The runtime loader therefore rewrites those four face references to two shared hash-keyed material records, with the three faces using `0x288ca4c4` sharing one record. The allocation addresses are run-specific, but the hash bucket, key field, sharing, and face-field rewrite are independently supported by the loader disassembly.

The adjacent [texture-runtime.md](texture-runtime.md) evidence covers the independently supported PSX palette records, converted-palette cache, and PC texture records used by the image-upload side.

## Downstream consumer

The ground/collision path at `0x004a12d0` is a confirmed consumer of the runtime environment representation. It walks a runtime object list, reads the object's slot byte and model index, indexes `DAT_0056d43c[slot * 0x11]`, reads the selected model's bounds, scales those bounds by `0x1000`, and compares them with the player's `+0x08/+0x0c/+0x10` position/AABB. A qualifying model/object is passed to `0x0049f4c0` with the player's `+0x4c` response vector.

This proves the disk-derived model pointer table is consumed by collision/ground processing. The more detailed [physics.md](physics.md) path now follows the selected PSX face through the collision query outputs and surface-flag extraction into skater response state. It does not claim that every node in the separate `DAT_0056af40` trigger-object list is a `SKWARE.PSX` object, nor does it reverse the full renderer.

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
- [renderer.md](renderer.md) traces the attached environment list into model
  packet submission; and
- [texture-runtime.md](texture-runtime.md) traces palette/texture records into
  the PC texture manager.
- [skater-asset-runtime.md](skater-asset-runtime.md) traces named player/skater
  PSX model regions into the player resource and animation handoff.
- [psx-lifecycle.md](psx-lifecycle.md) traces the inverse region clear path,
  including shared-material usage decrement, blockmap reset, and environment
  detach.

## Cross-family runtime inventory

The same common resource path has now been followed far enough to establish
these independent asset/runtime boundaries:

| Asset family | Runtime boundary | Consumer or handoff |
| --- | --- | --- |
| `SKWARE.PSX` / `SkWare_T.trg` | 0x4c-stride PSX environment records, relocated model pointers, and TRG object families | renderer, blockmap collision, trigger/object manager |
| `SK2DEF.PSX` | rebased shared/default skater PSX region named `sk2def%d` | player/custom-skater model setup |
| `SK2ANIM.PSX` | compacted animation table at `DAT_0056d444[slot * 0x11]` | gameplay animation start/update |
| skater `PSH`/`PSX` resources | player spool records, part manifests, and model regions | player object/model and animation handoff |
| player/skater runtime object | `0x3538` skater object plus contained `0x674` camera | animation, physics, collision, and renderer consumers |
| BMP / hashed PC textures | face-image loads, PRE-embedded images, and hash-keyed PC texture records | UI, player appearance, and renderer texture state |
| PSX image/palette tables | PSX palette handles, 0x18 converted-palette cache nodes, 0x2c PC texture records | bitmap open, conversion, Direct3D upload |
| PRE/PKR2/direct files | shared abstract resource handles and synchronized buffers | PSX, FNT, WAV, replay, and legacy module loaders |
| `FNT` | bounded font slots, glyph records, and image resources | text renderer |
| WAV/VAB sound resources | sound-bank records and runtime PCM buffers | sound playback |
| `PRK` custom parks | level-generation grid/items and finalized runtime region | custom-park level generation |
| replay/card buffers | fixed header plus five records and paired card buffers | replay load/save |
| `TRICKS.BIN` / `CRETEX.BIN` | relative section pointers and texture-set records | trick manager / texture-set manager |

The remaining gaps are semantic rather than missing top-level ownership paths:
Warehouse-specific bitmap residency after the proven hash/filename lookup,
the original names of several renderer state bits and Direct3D enums, the
final world-facing interpretation of the now-recovered steering-target
integrator, and the exact class names behind a few legacy module and
audio/texture handles.

## Confidence and limits

- `confirmed`: exact Warehouse file names, file-open path, PSX parser entry, object/model counts, model-pointer relocation, object-array allocation size/stride, object-17/model-17 correspondence, checksum-bucket material allocation, and the textured-face index-to-runtime-material rewrite.
- `observed`: trigger node count and trigger-object allocations; auxiliary `SkWare_L.psx`/`SkWare_O.psx` slot contents.
- `inferred`: semantic names for some runtime record fields beyond position, model index, next pointer, and model-derived flags.

The primary geometry trace intentionally stops short of renderer redesign,
external Warehouse bitmap provisioning, and full physics behavior. The
adjacent renderer, texture, PRE, TRG, and animation boundaries are recorded in
their own evidence files so those paths can be recreated without weakening the
geometry proof.
