# PSX texture and palette runtime path

Status: confirmed PSX scene-material hash relocation, material-to-PC-texture ownership, static PSX image/palette handoff, inline source-palette lookup, one disk-to-runtime texture witness, live Warehouse bitmap-hash lookup, PC bitmap lookup templates, normalized texture allocation, indexed/RGB conversion, and upload handoff into the PC texture manager
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0048a360`, `0x0048a400`, `0x004b1f70`, `0x004b2030`, `0x004b20f0`, `0x004b2450`, `0x004b2b70`, `0x004b4e70`, `0x0048a7e0`, `0x004da260`, `0x004da3e0`, `0x004da460`, `0x004da680`, `0x004d8c60`, `0x004d8cd0`, `0x004d8df0`, `0x004d8f10`, `0x004d98d0`, `0x004d9880`, `0x004d9950`

The geometry path and texture path share the same PSX parser, but not every PSX
asset carries an inline image/palette table. This document records the handoff
for the PSX variants that do, without pretending that the PSX texture payload
is itself a Direct3D object. `SKWARE.PSX` still supplies scene material/name
hashes and reaches the shared slot/parser boundary, but the offline parser
reports no inline texture records for that file. The dynamic Warehouse run is
used below as a bitmap-lookup witness, not as a per-texture image checksum
witness.

The format split is visible in the offline corpus:

```text
SKWARE.PSX   objects=252 models=288 texture_names=89 textures=0
SKWARE_O.PSX objects=25  models=25  texture_names=10 textures=0
DEFAULT.PSX  objects=1   models=1   texture_names=2  textures=2
HAWK4_FE.PSX objects=19  models=19  texture_names=12 textures=12
SK2ANIM.PSX  objects=19  models=19  texture_names=0  textures=0
```

The first two are scene/material-hash inputs; the latter two provide direct
inline texture-table witnesses for the image/palette path below.

The dynamic constructor witness below has an exact offline counterpart in
`MEM_CARD.PSX`, another inline-table variant:

```text
MEM_CARD.PSX texture #0
  palette/name reference = 0x7477d00a / 0x8a49d4b5
  color count            = 16
  dimensions             = 64 x 64
```

## PSX parser handoff

During `0x004b2450`, after model face references have been resolved, the parser
reads palette and texture metadata when the selected PSX variant contains
those tables. It creates or finds palette entries through the palette helpers
and stores their converted pixel data in the runtime palette table. It then
sets the per-slot texture-ready flag `DAT_0056d463[slot]` and calls
`0x004b2b70` from `spool.cpp`. Warehouse reaches this common boundary, but its
89 offline texture-name hashes are not evidence that 89 inline image records
were present.

The two directly observed palette construction forms are:

```text
4-bit palette  -> 0x0048a7e0(..., colors, 1)
8-bit palette  -> 0x0048a7e0(..., colors, 2)
pixel convert  -> 0x004da680(..., colors, 0x10 or 0x100)
```

`0x0048a7e0` allocates a palette record, validates the format flags, handles
the PSX transparent color (`0x7c1f`), marks the palette entries, and writes the
hardware color/index metadata. `0x004da680` allocates a converted color buffer
and transforms each PSX 16-bit color using the active Direct3D channel masks.
The separate cache node allocated by `0x004da460` is `0x18` bytes and is linked
through global head/tail pointers. `0x004da680` fills that node with the source
palette key at `+0x00`, declared colour count, a converted `u16` colour buffer,
the source `u16` colour buffer, and next/previous links. `0x004d8f10` searches
this list by the palette key passed into the upload call and stores the matching
cache node at the PC texture record's `+0x20`. It also marks the texture when
the first source colour is zero, which is later used by the indexed upload
branch. `0x004da680` transforms each PSX 16-bit colour using the active
Direct3D channel masks.

There is a second, distinct `0x18`-byte source-palette lookup list. `0x0048a360`
allocates and links its nodes through `DAT_0056b2f0`, stores the source/cache
key at node `+0x08`, and uses `+0x10/+0x14` as next/previous links;
`0x0048a400` walks that list and returns the node whose `+0x08` key matches.
The two lists must remain separate: the `0x004da460` records own converted
colour buffers, while the `0x0048a360` records provide source-palette lookup.

The PSX palette handle produced by `0x0048a7e0` is a different record. Its
constructor writes packed palette metadata at `+0x00`, a palette index at
`+0x02`, the format selector at `+0x03`, a ready byte at `+0x06`, and keeps a
source-palette pointer at `+0x0c` for the palette-copy helpers. The full record
size is not required for the texture handoff and remains intentionally open.

## Scene-material and texture metadata walks

The scene-material half of the parser is recorded in
[asset-loading.md](asset-loading.md): `0x004b2450` resolves each Warehouse
face's checksum-table index to a shared hash-keyed 0x2c-byte runtime material
record before the renderer sees the model. That record is intentionally not
conflated with the later PC D3D texture record, which also happens to be 0x2c
bytes. The material-to-PC-texture ownership link is now also confirmed:
`0x004da3e0(material, pc_texture)` stores the PC texture record at material
`+0x14` and writes the reverse material pointer at PC texture `+0x04`.

`0x004b2b70`, identified by the source string as `spool.cpp`, asserts that the
slot's textures are resident and walks texture-reference records for variants
with an inline texture table. For each referenced record it validates the
declared colour count (`0x10` or `0x100`), resolves the runtime texture entry,
and uses the record's dimensions and name/checksum fields to prepare a PC
texture. The Warehouse scene's hash-only material table is therefore a proven
scene-material association, while its later mapping from the scene material
record to a loaded PC bitmap remains open.

In the inline path, the same routine calls `0x0048a400` with
`DAT_0056db3c + reference[+0x08]` before preparing the PC texture. This proves
that the reference's `+0x08` word is used as an offset/key into the loaded
source-palette table and is resolved through the source-palette cache; it is
not evidence that the Warehouse hash-only table is itself an inline texture
record.

When the texture entry has not been initialized, it calls `0x004d8cd0`. The
function also prepares the texture's pixel dimensions for the 4-bit and 8-bit
paths and calls `0x004e8770` with the palette/pixel data. The latter is a small
or optimized helper in this build and is not assigned a stronger semantic name
from the decompiler alone.

The texture-reference facts supported by the parser are:

```text
record +0x04  declared colour count (16 or 256)
record +0x08  palette/checksum reference
record +0x0c  runtime texture/name reference
record +0x10  width (u16)
record +0x12  height (u16)
```

The `+0x10/+0x12` interpretation is cross-checked against the offline PSX
texture header reader and against the dimension arguments used by
`0x004d8cd0`. It does not assign a meaning to every remaining texture-record
word.

## PC texture object

`0x004d9880` allocates a zeroed `0x2c`-byte PC texture record and links it into
the global texture list:

```text
DAT_006a051c  first texture record
DAT_006a0520  last texture record
DAT_006a04d4  texture-record count
record +0x24  next record
record +0x28  previous record
```

`0x004d8cd0`, identified by its D3D texture source strings as `Pc_d3dtex.cpp`,
fills this record in a fixed order: it stores the PSX payload association,
declared dimensions, and the resolved texture-entry checksum/name, then opens
the PC-side image through `0x004d8df0`. The image header is read with
`0x004e7ad0`; this branch requires a 24-bit, uncompressed bitmap, replaces the
realized dimensions with the BMP dimensions, and finally calls `0x004d8f10`
for texture creation/upload. That routine uses the Direct3D texture vtable and
has separate indexed (4/8-bit) and converted-color branches.

The ownership sequence is explicit in the decompilation:

```text
RuntimePsxMaterialRecord +0x18 (checksum hash)
    -> 0x004d8df0 bitmap lookup
    -> 0x004d9880 RuntimePcTextureRecord
    -> record +0x0c = material checksum
    -> 0x004d8f10 Direct3D creation/upload
    -> 0x004da3e0(material, pc_texture)
    -> material +0x14 = RuntimePcTextureRecord
       pc_texture +0x04 = RuntimePsxMaterialRecord
```

The same `0x004da3e0` attachment is used by the generic indexed-bitmap path
(`0x004d8c60`) and by the inline PSX path (`0x004d8cd0`). For inline records,
`0x004b2b70` additionally stores the source-palette lookup node at material
`+0x1c`; this is palette ownership, not a replacement for the PC texture link.

The allocation-to-upload handoff is more concrete than the public class name:

```text
record +0x08  = PSX texture payload pointer + 0x14
record +0x0c  = checksum/name word from the resolved texture entry + 0x18
record +0x10  = initial flags (0x12, or 0x92 for the alternate path)
record +0x14  = PSX-declared width
record +0x16  = PSX-declared height
record +0x1c/+0x1e = image-header dimensions after the PC image is opened
record +0x20  = image/header-ready marker set before upload
```

The checked PC image path accepts a 24-bit bitmap header for this branch; an
unsupported bit depth or nonzero compression field tears down the pending
texture and returns through the error path. `0x004d8f10` then chooses the
indexed/palette or converted-color upload based on the record flags and
dimensions. This supplies a faithful recreation boundary even though the
Direct3D vtable object itself is still opaque.

The exact PC-side bitmap lookup is recoverable from the format strings in
`Pc_d3dtex.cpp`. `0x004d8cd0` supplies the PSX material/checksum word at
`param_3+0x18` to `0x004d8df0`, which reads a 14-byte BMP header and a 0x28-byte
DIB header through the common backend. The lookup templates are:

```text
configured NEWTEX branch:        "%s\\%s\\%08X.BMP"
fallback NEWTEX branch:          "%s\\%08X.BMP"
alternate NEWBMP branch:         "%s\\%s"
```

The configured branch uses the texture-root string at `DAT_006a0548` as the
subdirectory when its texture-mode comparison permits it; otherwise the
literal `NEWTEX` path is formatted. When the first argument is null, the
alternate branch strips the directory prefix from the second argument and
opens the basename under `NEWBMP`. The filename policy is therefore a
checksum/material lookup rather than a runtime scan of all images. Static data
at `DAT_00549f24` is `1` in this executable and has no discovered writer, so
the configured branch is enabled by default; `DAT_006a6964 == 1` also selects
it when the graphics mode is active. `DAT_006a0548` is populated from the
frontend's selected `D3DLevelName` before the level launch call. The
render-time disk-name template, 24-bit/uncompressed validation, and
record-to-upload boundary are fixed.

For the Warehouse model-140 witness, the two material keys therefore produce
the candidate bitmap names `D783CF21.BMP` and `288CA4C4.BMP` under either the
configured `NEWTEX\\<root>` subdirectory or the fallback `NEWTEX` directory.
This is a deterministic loader prediction, not evidence that both files were
resident or opened in the captured Warehouse frame.

## Live Warehouse bitmap lookup

A controlled level-12 launch reached `0x004d8df0` after the executable printed
`Loading Level: Warehouse`. The breakpoint filter used the two material keys
from the renderer's model-140 witness and observed both exact lookup arguments:

```text
WAREHOUSE_BMP_LOOKUP hash=0xd783cf21
WAREHOUSE_BMP_LOOKUP hash=0x288ca4c4
```

These are the offline `SKWARE.PSX` material/name hashes, not guessed filenames;
the loader's `%08X.BMP` formatting therefore connects the Warehouse PSX
material table to the live PC bitmap lookup function. However, the same forced
level log records `D3DLevelName: Hangar` before the level argument is
overridden, so this run proves the hash lookup but not a
`NEWTEX\\Warehouse` directory selection. The controlled extracted asset tree
contains neither `D783CF21.BMP` nor `288CA4C4.BMP`, and this run did not capture
a successful `0x004d8f10` upload for either key. Actual bitmap residency remains
conditional on the external configured-root files being present; a normal
Warehouse selection is required to prove the final subdirectory value.

The stable record fields set before the upload are:

```text
+0x00  Direct3D texture object/realized resource
+0x08  source image/data association
+0x0c  texture name/checksum value
+0x10  texture state/creation flags
+0x14  declared width
+0x16  declared height
+0x18  normalized realized-resource width
+0x1a  normalized realized-resource height
+0x1c  realized/source width
+0x1e  realized/source height
+0x20  converted-palette cache pointer or ready sentinel
+0x24  next texture record
+0x28  previous texture record
```

The field names above are deliberately operational rather than claims about a
public C++ class. The generated `NEWTEX`/`NEWBMP` path policy and the level-root
input are now recorded above; only the successful Warehouse upload witness
remains to be collected.

## Upload and realized-resource contract

The decompiled upload routine is a dispatcher over two concrete data paths.
Before dispatch, `0x004d8f10` resolves the optional converted-palette cache node
from the global `0x18`-byte list. A zero first source colour sets the texture's
transparent/palette state bit; the cache pointer is retained at record `+0x20`.
The sentinel value `1` used by the 24-bit bitmap path at the same offset means
that this field is operationally a cache pointer-or-ready marker, not a plain
boolean image flag.

When record flags do not contain `0x08`, `0x004d9630` locks the realized
resource, obtains the palette pointer from the cached record, and expands each
source row. A 4-bit texture consumes the low and high nibbles of each source
byte; an 8-bit texture consumes one palette index per byte. Each index becomes
a 16-bit palette colour in the destination row. A zero palette entry clears
the texture's `0x10` state bit while the row is expanded.

When record flags contain `0x08`, `0x004d9310` reads the 24-bit BMP BGR stream,
converts each pixel through the active Direct3D channel masks, and writes the
engine's 16-bit colour words. The conversion preserves the engine's special
transparent/bright-colour test: the qualifying source colour becomes zero and
clears the same `0x10` state bit; normal pixels receive the high format bit and
the packed channel fields. This is sufficient to reproduce the loader's pixel
contract without depending on the opaque Direct3D texture vtable.

If no realized resource exists, `0x004d8f10` normalizes the requested width and
height before the D3D create call. Depending on the device configuration it
rounds each dimension to a power of two, optionally makes the texture square,
clamps to configured maxima, and applies configured minima. The normalized
values are stored at record `+0x18/+0x1a`; the declared PSX dimensions remain at
`+0x14/+0x16`, while the source-image dimensions are carried at `+0x1c/+0x1e`.
After a successful upload, the routine swaps those two dimension pairs when
the bitmap flags contain `0x04` or `0x08`, restoring the stable contract:

```text
+0x14/+0x16  declared/source PSX dimensions
+0x18/+0x1a  normalized realized-resource dimensions
+0x1c/+0x1e  realized PC-image dimensions
+0x20        converted-palette cache pointer, or ready sentinel
```

`0x004d8990` is the companion source-pixel allocator used when the device path
needs a writable copy. It sizes the buffer from the normalized height and
width, copies rows with their source pitch, and releases the temporary source
object after the copy succeeds. These helpers close the disk-image-to-texture
memory boundary even though the Direct3D resource object and its vtable remain
unidentified.

## Dynamic PC-texture witness

A controlled headless run reached the same constructor while the front-end
skater assets were being initialized. This is not a Warehouse texture, but it
independently confirms the static argument-to-record handoff:

```text
break 0x004d8cd0:
  param_1 = 0x05f38300
  param_2 = 0x7477d00a
  param_3 = 0x05f3c648
  param_4 = 0

break 0x004d8f10:
  PC record = 0x03b48520
  record +0x08 = 0x05f38314  (= param_1 + 0x14)
  record +0x0c = 0x8a49d4b5  (= *(param_3 + 0x18))
  record +0x10 = 0x1a
  record +0x14/+0x16 = 0x40/0x40
  record +0x1c/+0x1e = 0x40/0x40
  record +0x20 = 1
```

The record was allocated by `0x004d9880` and was passed directly into the
upload routine. At the same stop the texture-manager list was live at
`DAT_006a051c = 0x03b42b40`, `DAT_006a0520 = 0x03b48c70`, with count
`DAT_006a04d4 = 0x15a`. These heap addresses are run-specific; the argument
offsets and record fields are the reusable evidence.

The two material words in that same stop identify the source texture without
relying on a runtime filename:

```text
constructor param_2              = 0x7477d00a
*(constructor param_3 + 0x18)    = 0x8a49d4b5
runtime record +0x0c              = 0x8a49d4b5
offline MEM_CARD.PSX texture #0  = palette 0x7477d00a / name 0x8a49d4b5
```

Its declared and realized dimensions are both `64 x 64`, matching the offline
texture header. This is the disk-to-runtime material bridge: the runtime
record's checksum/name and palette reference are independently identified by
an offline PSX texture entry. It does not yet prove that the Warehouse
scene's 89 material hashes use this same resource family.

## End-to-end texture flow

```text
PSX variant with inline palette/texture records
  -> 0x004b2450 palette/cache preparation
  -> 0x0048a7e0 palette record + 0x004da680 converted colors
  -> 0x004b2b70 texture-reference walk
  -> 0x004d9880 0x2c-byte PC texture record
  -> 0x004d8cd0 image/header and D3D texture setup
  -> 0x004d8f10 indexed/color upload branch
  -> renderer model packets consume the resulting texture state
```

The Warehouse run did reach `0x004b2b70` through slot 6 and reported the raw
scene buffer at `0x005d74ef0`, whose header agrees with the parsed scene
(`252` objects and `288` models). That is only a slot/parser boundary witness:
because `SKWARE.PSX` has no inline texture records in the offline parse, it is
not promoted to a Warehouse checksum-to-PC-texture correspondence. The
scene-material record itself is nevertheless proven by the parser's checksum
bucket and face-field rewrite; only the subsequent PC-bitmap ownership needs
another consumer-side witness.

## Confidence and limits

- `confirmed`: parser-to-texture call boundary, scene-material to PC-texture
  attachment/reverse ownership, accepted PSX palette formats,
  source-palette lookup-list allocation/search, palette conversion allocation,
  one offline MEM_CARD texture to live PC
  texture-record correspondence, PC texture-record size/list linkage, bitmap
  path templates, image header read, 24-bit/uncompressed validation, dimension
  propagation, indexed nibble/byte expansion, 24-bit BGR conversion,
  normalized-resource sizing, and Direct3D upload branch.
- `observed`: the operational field offsets, the cache-pointer-or-ready
  marker at `+0x20`, the separate indexed/color branches, and a live
  constructor-to-upload record with 64x64 dimensions.
- `inferred`: the exact PC-bitmap ownership reached from a Warehouse scene
  material record, `0x004e8770`'s complete role, and all Direct3D texture flags.
