# PSX post-model tag runtime ownership

Status: confirmed common PSX tag dispatch and runtime consumers for texture-WIB,
colour-pulse, blockmap, animation, BITS, hierarchy, and RGBS products; the
internal payload schemas remain partial and the animation tag distinction is
limited to a packaging/selection question not observed in the common builder
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004b20f0`, `0x004b2450`, `0x00461cf0`, `0x00462170`,
`0x00464a80`, `0x004b34f0`, `0x004b40a0`, `0x004b46a0`, `0x004610f0`,
`0x004604f0`

The common PSX parser does more than publish scene objects and model packets.
After the model/object tables, `0x004b20f0` walks a bounded sequence of
post-model records. `0x004b2450` dispatches each record by its 32-bit type and
publishes pointers into slot-indexed tables. Those products are then consumed
by the model, animation, palette, collision, and effect code below.

## Corpus and parser boundary

The extracted `ALL.PKR` inventory contains 282 PSX files. Every file parsed as
version 4 with marker 2 and completed the offline object/model/tag walk. The
observed tag distribution is:

| Tag type | Files | Offline/runtime role | Slot product or direct consumer |
| --- | ---: | --- | --- |
| `0x00000006` | 27 | texture-WIB item-info table | `DAT_0056d454[slot * 0x11]`; `0x00464a80` preflight and `0x00461cf0` model/face consumer |
| `0x00000007` | 28 | colour-pulse table | `DAT_0056d458[slot * 0x11]`; `0x00462170` pulse consumer |
| `0x0000000a` | 45 | blockmap | `0x004667e0` conversion into collision grid state |
| `0x0000002a` | 87 | `PK_3DANIM`; 3D animation table variant | `DAT_0056d444[slot * 0x11]`; `0x004b40a0` animation-table builder |
| `0x0000002c` | 44 | `PK_COMPRESSED3DANIM`; compressed 3D animation table variant | `DAT_0056d444[slot * 0x11]`; `0x004b40a0` animation-table builder |
| `0x00000045` | 1 | `PK_VERTEXCOLOURS`; BITS named-group/color table | `0x004b34f0` named-resource records and `DAT_0056db38` list |
| `0x52454948` (`HIER` bytes) | 128 | animation hierarchy table | `DAT_0056d448[slot * 0x11]`; hierarchy/pose consumers |
| `0x73424752` (`RGBs` bytes) | 123 | RGB/color table | `DAT_0056d450[slot * 0x11]`; render and colour-pulse consumers |

The executable's decompiler displays the little-endian four-byte hierarchy
type as `REIH`; its bytes spell `HIER` when read as a file tag. The offline
parser now gives these recognized values stable names in `PsxTag.type_name` and
reports only genuinely unrecognized post-model types as `unknown`.

The recovered `SKATE2.TAG` source inventory names the corresponding spool
products `PK_TEXTUREWIBBLE`, `PK_COLOURPULSE`, `PK_ZONE`, `PK_3DANIM`,
`PK_COMPRESSED3DANIM`, `PK_HIERARCHY`, and `PK_VERTEXCOLOURS`. The executable
switch and payload consumers tie those public source names to the numeric
products above; `PK_ANIMATIONINFO` is retained as a source-side animation
record name rather than assigned a second numeric tag without a matching
dispatch case.

## Texture-WIB item-info path

The type-6 parser slot is assigned at `0x004b2450` and is asserted by
`0x00464a80`, whose source strings identify the input as a texture WIB. The
preflight walks its variable records and marks which texture/material lists
have the required entries. This is not the ordinary PSX texture-name table.

`0x00461cf0` is the downstream model consumer. It retrieves the type-6 pointer
for a region slot, walks its item records, selects the referenced model/object,
and updates texture-coordinate data in model face records. It skips records
whose flags exclude the operation. This proves the type-6 disk payload reaches
the relocated model/face representation before rendering; it does not assign
names to every item flag or to the transformed coordinate fields.

## Colour-pulse and RGBS path

Type 7 is assigned to `DAT_0056d458[slot * 0x11]`. The assertion and diagnostic
strings in `0x00462170` identify it as a colour-pulse input. The consumer walks
bounded pulse records, applies the active pulse state, and writes the resulting
colour through the slot's RGBS table at `DAT_0056d450[slot * 0x11]`.

The RGBS tag itself is dispatched to `DAT_0056d450` by `0x004b2450` and is
consumed by the material/render helpers at `0x004610f0` and `0x004604f0`.
Thus the two tags have separate roles: type 7 supplies pulse rules, while RGBS
supplies the indexed colour table that receives or exposes the current colour.
The runtime record layout is now bounded:

```text
colour-pulse list +0x00  RGBS colour index
                    +0x01  list length
                    +0x02  current interval index (mutated by the consumer)
                    +0x03  elapsed interval units (mutated by the consumer)
                    +0x04  list_length records, each 4 bytes:
                           red, green, blue, interval duration
```

`0x00462170` advances the current index and elapsed interval from the frame
clock, computes the interpolation between the current and next RGB entries,
and writes the resulting packed colour to the selected RGBS entry. The RGBS
entry fields themselves remain unnamed, but the pulse-list storage and mutable
state are no longer an unresolved boundary.

## Animation tag products

Types `0x2a` and `0x2c` both enter the animation-table slot at
`DAT_0056d444[slot * 0x11]`; the parser also sets the corresponding animation
availability state. `0x004b40a0`, called directly by the `sk2anim.PSX` loader
`0x004b46a0`, compacts the selected metadata and publishes the hierarchy/source
table at `DAT_0056d448[slot * 0x11]`. The hierarchy tag is consumed alongside
that table by the pose/decompression path.

The corpus provides a stronger common-format invariant. For every observed
type-0x2a and type-0x2c payload, the first word is the animation-record count
`N`, the second word is the source-stream offset `4 + N * 8`, and the next `N`
records are eight bytes each. The compressed/source stream begins at that
offset. Type 0x2a is used almost entirely by small one-record resources
(86 files have `N=1`; `RASTA.PSX` is the one `N=63` exception), while
type 0x2c includes the large player animation tables (`N=78` or `N=218`) as
well as small front-end/model resources. `0x004b40a0` consumes the shared
layout and does not branch on the tag value.

The runtime ownership is therefore proven for both on-disk animation variants.
The remaining question is why the asset builder emits two tag values when the
PC loader uses the same table/stream contract; no higher-level behavioral
distinction is assigned. The existing [animation-runtime.md](animation-runtime.md)
records the concrete `SK2ANIM.PSX` type-0x2c count, record stride, compressed
stream, hierarchy payload, and pose handoff.

## BITS and blockmap products

Type `0x45` is the executable's `PK_VERTEXCOLOURS` tag. The extracted
`BITS.PSX` uses this product as a named-group/color table. `0x004b34f0`
rewrites its named entries into the runtime list headed by `DAT_0056db38`; `0x004b3680` then finds
names such as `Shadow` and `Smoke`. Its item/effect consumer is recorded in
[items-runtime.md](items-runtime.md).

Type `0x0a` is the blockmap already decoded by the offline parser. The common
parser passes it to `0x004667e0`, after which the runtime zone/collision grid
walk uses the cell object references. Its object/model correspondence is
recorded in [blockmap-collision.md](blockmap-collision.md).

The native recreation mirrors the two bounded post-model products in
`src/assets/psx_bits_runtime.*` and `src/assets/psx_animation_runtime.*`.
`PsxBitsRuntime` decodes the fixed eight-byte group-name/count/entry framing
and provides the proven case-insensitive named lookup. `PsxAnimationRuntime`
retains each animation tag's eight-byte records and the bytes after the
records as its source stream, plus the raw HIER payload; no unproven color,
compression, or pose field names are introduced.

## Runtime slot map

The pointer products are slot-indexed with a 0x44-byte region stride (0x11
words). The currently supported map is:

| Address | Runtime product |
| --- | --- |
| `0x0056d438` | PSX environment-object array |
| `0x0056d43c` | relocated model-pointer array |
| `0x0056d444` | animation metadata/table |
| `0x0056d448` | compacted animation hierarchy/source table |
| `0x0056d450` | RGBS colour table |
| `0x0056d454` | texture-WIB item-info table |
| `0x0056d458` | colour-pulse table |
| `0x0056d440` | raw/finalized PSX buffer |

This map closes the post-model tag-to-runtime ownership gap for the extracted
PSX family. It does not claim that all payload records have been reconstructed
or that every slot pointer remains live after region teardown.

## Concrete witnesses

- `SK2ANIM.PSX` has a type-`0x2c` animation table followed by a hierarchy tag;
  it reaches `0x004b40a0`, compacted animation state, and pose decompression.
- `BITS.PSX` has a type-`0x45` table whose named groups include `FONT`,
  `SHADOW`, `SMOKE`, `ribbon`, and `Buttons`; those names reach the runtime
  named-resource lookup.
- Warehouse scene variants carry blockmap, RGBS, texture-WIB, or colour-pulse
  tags in combinations that explain why the common PSX parser publishes more
  than one optional slot product for a level resource.

## Open questions / falsifiers

- Recover the remaining type-6 record flags and coordinate transform names if
  faithful texture-coordinate mutation is needed; type-6 record width,
  object-selection word, count, and eight source coordinate bytes are bounded
  by `0x00464a80`/`0x00461cf0`.
- Determine the authoring/selection reason for animation tags 0x2a and 0x2c;
  their common count/record/source-offset invariant and shared PC builder are
  already proven, so this is not an unconnected runtime path.
- Capture teardown or reload of a slot carrying each optional tag to verify
  which product pointers are cleared by the region lifecycle.
- A PSX tag is not promoted to a runtime product merely because its numeric
  value appears in an extracted file; the parser dispatch and consumer edge
  above are the required evidence.
