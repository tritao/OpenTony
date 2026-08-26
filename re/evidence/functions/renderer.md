# Warehouse PSX scene-object renderer path

Status: confirmed live/static renderer consumer for the loaded PSX environment,
including object/model selection, transformed-vertex, polygon-packet,
dispatch-table, and hardware primitive contracts
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x00467c90`, `0x0045e8e0`, `0x0045f530`, `0x004610f0`, `0x00461a50`, `0x00461a90`, `0x00461b10`, `0x004d0c30`, `0x004d11d0`, `0x004d14d0`, `0x004d18b0`, `0x004d1960`, `0x004d1d40`, `0x004d20f0`, `0x004d29e0`, `0x004d3160`, `0x004d3480`, `0x004d3510`, `0x004d3600`, `0x004d3780`, `0x004d3a40`, `0x004d41e0`, `0x004d42f0`, `0x004d45f0`, `0x004d49e0`, `0x004d4bf0`, `0x004d5040`, `0x004d5280`, `0x004d56c0`, `0x004d5960`, `0x004d5e40`, `0x004d6090`, `0x004d60c0`, `0x004d60f0`, `0x004d6120`, `0x004d6320`, `0x004d6560`, `0x004d66f0`, `0x004d68b0`

This document records the renderer boundary without assigning full Direct3D
semantics to the lower-level helpers. The scene loader's material rewrite is
recorded in [asset-loading.md](asset-loading.md); renderer face submission
consumes the resulting runtime face records rather than re-resolving the disk
texture index.

## Static call graph

The render-stage routine `0x00467c90` calls `0x0045f530` first with
`DAT_0056db28`, the head of the attached PSX environment list, and then with
`DAT_0056af40`, the separate game-object list. The environment call is the
scene-geometry branch established by the asset-loading and attach traces.

```text
DAT_0056db28 (attached PSX environment list)
    -> 0x0045f530  m3d object traversal, culling, model selection

ordinary environment object branch
    -> 0x00461a50 / 0x00461a90  state-geometry wrapper
    -> 0x004d14d0  model-packet state/geometry submission

flagged/special object branch
    -> 0x004610f0  m3d model/part preparation
    -> 0x004d11d0  indexed geometry packet submission
    -> Direct3D/state helpers (not yet fully named)
```

`0x0045f530` walks the runtime object's `+0x20` next pointer. For each object
it checks the object flags and the slot finalization state, reads the slot byte
at `+0x1f` and model index at `+0x1a`, and selects the model from
`DAT_0056d43c[slot * 0x11]`. It applies model-flag tests and view/culling
logic before selecting one of the two render branches below.

For ordinary environment objects, a selected model packet with flag `0x0004`
set enters the indexed path through `0x004d11d0`; otherwise the object goes
through `0x00461a50` or `0x00461a90`, both of which call `0x004d14d0`.
`0x004610f0` is a separate flagged/special-object path and must not be treated
as the only environment renderer entry.

`0x004610f0` is identified by the embedded source string as
`H:\\TonyHawk\\Pc2\\m3d.cpp`. It repeats the slot/model selection, walks the
model-part count in `DAT_0056d460[slot * 0x44]`, and consumes each variable-size
model packet. The model header counts at offsets `+0x02`, `+0x04`, and `+0x06`
are read as the vertex, normal, and face counts by the model finalizer and
control the packed-geometry walk. In the textured branch the function passes
the packet-derived stream pointer and count to `0x004d11d0` and uses the
adjacent m3d helpers for texture/material state.

For animation-capable objects, `0x004610f0` is also the concrete pose-to-model
consumer. It obtains an array of 0x18-byte pose records from
`0x00430920`, or from the object's cached pose array at `+0x138` when the
object flags select the built-pose path. It walks the loaded model-part count,
optionally remaps each part through the one-byte order table at `+0x150`,
reads the pose record's nine fixed-point basis words and three translation
words, composes them with the object transform, and submits the corresponding
model packet through `0x004d11d0`. This is rigid per-part model deformation
(one PSX model packet per animated part), not a claim of vertex-weighted
skinning. Together with the animation evidence, it closes the path:

```text
SK2ANIM.PSX compressed channels
    -> 0x00430920 per-part 0x18-byte pose records
    -> 0x004610f0 per-part basis/translation composition
    -> 0x004d11d0 model-packet submission
    -> Direct3D polygon list
```

The model finalizer at `0x004647c0` provides the stronger packet-layout
boundary. For each entry in the count-prefixed model-pointer table it reads:

```text
packet +0x00  flags (u16)
packet +0x02  vertex count (u16)
packet +0x04  normal count (u16)
packet +0x06  face count (u16)
packet +0x08  radius (u32)
packet +0x0c  six signed 16-bit bound words
packet +0x18  additional 32-bit header word
packet +0x1c  packed 8-byte geometry records followed by variable-size face data
```

For version-4 PSX resources, the first geometry records are independently
cross-checked by the offline reader and the renderer's pointer arithmetic:

```text
packet +0x1c + vertex_index * 0x08
    s16 position_x, position_y, position_z
    u16 source_padding_or_auxiliary_word

packet +0x1c + vertex_count * 0x08 + normal_index * 0x08
    s16 normal_x, normal_y, normal_z
    u16 source_padding_or_auxiliary_word

face stream
    u16 packed_face_flags
    u16 record_length_in_bytes
    ... variable face payload, ending at face_start + record_length_in_bytes
```

The runtime keeps these records in the relocated model packet; it does not
expand them into a second per-model vertex class before collision/render
consumers read them. Version 3 uses the same two eight-byte geometry strides
but stores 32-bit face vertex indices, while version 6 widens the UV values;
those source-format variants remain distinct from the common version-4
Warehouse path.

It normalizes the packet in place, records the realized model/part count in
`DAT_0056d460[slot * 0x44]`, and leaves the same model pointers for collision
and rendering. The offline parser's Warehouse model 17 header is therefore a
direct check of this runtime header: flags `8`, counts `20/12/12`, and the six
bound words `(0, 0, 1229, -1228, 342, -343)`.

`0x004d11d0` consumes the stream/count pair from the textured branch. Its
packet loop reads the per-record flag/length area, resolves indexed texture
state through the active palette/texture table when the record requests it,
and emits transformed color/geometry data to the lower Direct3D path. The
stable boundary for a recreation is the call contract; the exact GPU packet
bitfield names remain intentionally open.

`0x00461a50` and `0x00461a90` forward their model-packet argument to
`0x004d14d0` with `DAT_005620a4` plus an optional `0x200` state bit.
`0x00461b10` is the more general state wrapper: it adds the low state bit and
conditionally adds `0x200` and `0x08` before calling the same submitter. The
submitter at `0x004d14d0` records the state word, derives the vertex and normal
streams from `packet + 0x1c` using the packet counts, and invokes lower
transformation/state helpers. It then walks the variable-size face records
through `0x004d18b0`: visible faces enter `0x004d1960` for color-table/vertex
state resolution and `0x004d1d40` for the 0x30-byte D3D polygon record. In the
textured face form, `0x004d1d40` reads the runtime face texture/material field
at `face + 0x10` and the four UV bytes at `face + 0x14`; those fields are the
post-loader values, not the original Warehouse checksum-table index. The
exact Direct3D texture-resource field behind a scene material record remains
open for hashes that are not loaded in the controlled frame, but the disk index
-> hash record -> runtime face pointer -> polygon submission chain is
established. When the material has been populated, its `+0x14` points to the
`RuntimePcTextureRecord`; that record's `+0x00` is the Direct3D texture resource
consumed by the lower upload/render state. Four Warehouse hashes are now
observed at that upload boundary; the remaining residency question concerns
the other material keys and frame-to-frame lifetime, not the ownership layout.

The inverse ownership path is also fixed. Region teardown decrements the shared
material's `+0x10` reference count. The zero-reference sweep releases the PC
texture at material `+0x14`; `0x004d8b50` releases its Direct3D object, calls
`0x004da3e0(material, 0)` to detach the reverse link, unlinks the texture's
`+0x24/+0x28` list links, and frees its owned source/cache buffers before
freeing the 0x2c-byte record. Renderer material state is therefore shared and
reference-counted, not owned independently by each face.

The renderer therefore consumes the same model pointer table and the same
runtime object fields already established by the PSX loader and collision
path. It is not a second, unrelated scene representation.

At the frame boundary, `0x00467c90` selects the per-view polygon arena at
`DAT_00560fd4 + 0x88 + view * 0x8000` and calls `0x0045e8e0` with the active
viewport and that view's bucket-head array. `0x0045e8e0` binds the viewport,
resets the polygon allocation cursor, stores the bucket-head pointer in
`DAT_0056433c`, and seeds the first head record. Accepted polygons from
`0x004d20f0` are linked into this array using the depth/class bucket returned
by its lower classifier. This establishes the per-frame arena and bucket-list
ownership that feeds the `0x004d3160` Direct3D command consumer.

## Polygon-list consumer and hardware state

The later draw loop is now identified as `0x004d3160`. It receives the head
of the current linked polygon/command list, begins the render-state bracket
with `0x20000/0x30000`, and follows each record through its `+0x00` next link.
The byte at `+0x07` is the command opcode/format byte and `+0x04` is the
record-enable/payload word checked before dispatch.

For ordinary geometry, `0x004d3160` uses `opcode & 0xfc` to select a handler
from the executable's geometry dispatch table. The low bits control state:

```text
opcode bit 0  records the geometry mode in DAT_006a0064
opcode bit 1  selects opaque versus alpha/texture state
opcode bit 2  selects texture/blend setup, except for fixed command classes
opcode bit 4  selects the alternate per-record texture word (+0x1a vs +0x16)
```

The texture/blend state path is concrete. It changes the Direct3D render-state
slots through the device vtable at `+0x50` and `+0x94`, sets the current
texture-blend mode through `0x004d3480`, and selects the blend/color mode
through `0x004d3510`. Four encoded texture modes map to the observed color
constants `0x80000000`, `0xff000000`, `0`, and `0x40000000`, with corresponding
blend factors `(1,1)`, `(2,2)`, `(4,4)`, and `(2,2)` plus packet flags
`0x08/0x18/0x28/0x38`. These are operational state mappings; the original
Direct3D enum names are not needed for a faithful recreation.

The same consumer handles non-geometry command records. An `0xe1` packet
updates the active texture mode from its word at `+0x04` and obtains the PC
texture resource through the material pointer at `+0x0c` and material `+0x14`.
An `0xe3` packet is sent to `0x004d3600`, which sign-extends and clamps its
viewport rectangle before `0x004d36c0` submits it to the device. The reverse
heap path (`0x004d3a40` -> `0x004d3a70` -> `0x004d3780`) reverses a bounded
command list and applies the same opcode/state dispatch, preserving the
ordering required by the alternate render path.

The handler table is initialized by `0x004d41e0`: all 256 entries at
`DAT_0065f82c` start at the no-op routine `0x004d7420`, after which the
following concrete opcode bases are installed. The handlers emit the legacy
Direct3D `DrawPrimitive`-style calls with vertex type `0x44`; the vertex count
below is the count passed to that device call.

| Opcode base | Handler | Proven primitive/behavior |
| --- | --- | --- |
| `0x20` | `0x004d42f0` | solid triangle, 3 vertices |
| `0x24` | `0x004d45f0` | textured triangle, 3 vertices |
| `0x28` | `0x004d49e0` | solid quad, 4 vertices |
| `0x2c` | `0x004d4bf0` | textured quad, 4 vertices |
| `0x30` | `0x004d5040` | Gouraud/color triangle, 3 vertices |
| `0x34` | `0x004d5280` | textured Gouraud triangle, 3 vertices |
| `0x38` | `0x004d56c0` | Gouraud/color quad, 4 vertices |
| `0x3c` | `0x004d5960` | textured Gouraud quad, 4 vertices |
| `0x40` | `0x004d6560` | two-vertex line, one color |
| `0x48` | `0x004d6120` | line strip, four vertices |
| `0x4c` | `0x004d6320` | closed line strip, five vertices |
| `0x50` | `0x004d66f0` | two-vertex colored line |
| `0x60` | `0x004d5e40` | solid rectangle/quad fan, four vertices |
| `0x68` | `0x004d6090` | unit rectangle wrapper around `0x60` |
| `0x70` | `0x004d60c0` | 8-unit rectangle wrapper around `0x60` |
| `0x78` | `0x004d60f0` | 16-unit rectangle wrapper around `0x60` |
| `0xb0` | `0x004d68b0` | general polygon/vertex-list path; stride and primitive mode depend on flags |

The table explains the previously opaque final boundary: model faces reach
the polygon list with a format byte, and that byte selects one of the exact
solid, textured, Gouraud, line, rectangle, or general-polygon emitters. The
same handlers have a separate modern-device path, but both paths consume the
same packet fields and vertex counts. `0x004d68b0` additionally walks a
variable vertex list at `packet + 0x28` with either five- or seven-word records
and forwards it to the textured/untextured lower submitters.

After the list is consumed, `0x004d0c30` performs draw synchronization and
buffer presentation through the Direct3D device objects. The packet format,
list ownership, state selection, control packets, viewport handoff,
individual geometry-handler table, and final present boundary are proven. The
original high-level names of the legacy Direct3D enums remain unrecovered,
but the primitive family and exact vertex counts are sufficient for a
faithful recreation.

## Transformed vertex and polygon packet contracts

The common submitter at `0x004d14d0` establishes the internal renderer handoff
more precisely than the outer object traversal. It stores the model packet in
`DAT_0056e870`, derives the packed vertex stream at `packet + 0x1c`, and
derives the normal stream from the vertex count. It then calls `0x004d29e0`
to transform the packed vertices into seven-float working records at
`DAT_00570878`. The working record layout is:

```text
working +0x00  projected X (f32)
working +0x04  projected Y (f32)
working +0x08  projected Z (f32)
working +0x0c  reciprocal depth (f32)
working +0x10  source packed-vertex flags (f32 storage)
working +0x14  clip flags (f32 storage; bit values are written as integers)
working +0x18  auxiliary/override value (f32 storage)
```

Indexed/controlled source vertices can instead point into the same working
table through the renderer's override table. The transformer also copies all
seven words when an override entry is materialized, so the stride is a runtime
contract rather than merely a temporary decompiler artifact.

`0x004d18b0` walks the variable-size face stream using the face record's
length word. Visible records go through `0x004d1960`, which resolves the
packed face color against the active color/palette tables, and then through
`0x004d1d40`, which allocates a fixed 0x30-byte polygon record from the
per-frame polygon arena. The observed stable fields are:

```text
polygon +0x00  linked-list next pointer
polygon +0x07  packet format byte (0xb0)
polygon +0x08  packed face/render flags
polygon +0x10  runtime scene-material / PC-texture record pointer, textured faces
polygon +0x14  vertex count (3 or 4)
polygon +0x18  variable transformed vertex/color/UV stream
```

`0x004d1d40` writes that tail in two exact forms. Every vertex starts with the
four transformed words `(x, y, z, reciprocal_depth)` and one packed color
word. Solid records therefore use a `0x14`-byte stride; textured records append
two normalized `f32` UV values and use a `0x1c`-byte stride. The builder emits
four vertices unless the face flags select three, then records the resulting
count at polygon `+0x14`. This is the boundary a renderer recreation should
preserve before translating the packet to a modern graphics API.

For textured faces the builder copies the four UV byte pairs from the runtime
face record at `face + 0x14`. It resolves the scene material at polygon
`+0x10`, follows material `+0x14` to the `RuntimePcTextureRecord`, and reads
the post-upload source/declared texture dimensions at texture `+0x14/+0x16`.
(`0x004d8cd0` temporarily puts the BMP dimensions there before the upload;
`0x004d8f10` swaps the dimension pairs back after creation.) Each UV component
is normalized by the exact integer-sized texture dimension plus the engine's
half-texel constant:

```text
polygon_uv_u = (float(face_uv_u) + DAT_00519948) / (float)texture->+0x14
polygon_uv_v = (float(face_uv_v) + DAT_00519948) / (float)texture->+0x16
```

This independently confirms that the scene-material pointer is consumed after
PSX parsing and that the renderer does not use the original disk texture-table
index at this stage. A face with source flags selecting three vertices emits a
three-vertex polygon; otherwise the builder emits four vertices, and quads may
later be split by `0x004d20f0`.
Quads may be split into two triangle submissions by `0x004d20f0`; that helper
performs the projected-space winding/depth tests and links accepted polygons
into the renderer's bucketed list. The final list consumer is `0x004d3160`;
whose concrete opcode handlers are described above.

The indexed/special path at `0x004d11d0` has a separate color-packet contract:
it walks eight-byte records, checks their flags, and expands indexed color
values into the renderer color table before ordinary polygon submission. It
should remain a separate path in a recreation rather than being folded into
the environment model packet format.

## Warehouse object-17 bridge

The controlled load established:

```text
SKWARE.PSX object #17 / model #17
    -> slot 6 runtime object record 0x005f34f6c
    -> slot 6 model-table entry 0x005d78d2c
```

The static renderer selection contract is:

```text
object[+0x1f] = 6
object[+0x1a] = 17
DAT_0056d43c[6 * 0x11][17] = 0x005d78d2c
model flags 8 -> 0x00461a50/0x00461a90 -> 0x004d14d0
```

The exact pointer values are run-specific heap addresses. The slot, index,
stride, and model-table relationship are the stable evidence.

## Live Warehouse render witness

A controlled gameplay frame reached the actual render stage with
`CurrentLevel = 12`:

```text
0x00467c90:
    attached environment head = 0x05f34a5c
    game-object head          = 0x05f30f48

environment record #140:
    pointer                   = 0x05f373ec
    record stride              = 0x4c
    position                  = (0x023e6000, 0xffffc000, 0x02b6e000)
                              = (37642240, -16384, 45539328)
    slot (+0x1f)              = 6
    model index (+0x1a)       = 140

slot 6 model table            = 0x05d77270
model table[140]              = 0x05d83078
model packet header            = flags 0x48, counts 8 / 4 / 4
0x004d14d0 argument pair       = (0x05d83078, 0x00000800)
```

The offline parser reports the corresponding `SKWARE.PSX` object/model pair:

```text
object #140:
    position_fixed = (37642240, -16384, 45539328)
    position       = (9190, -4, 11118)
    model_index    = 140

model #140:
    offset         = 0xe188
    size           = 236
    flags          = 8
    counts         = 8 / 4 / 4
    bounds         = (231, -230, 4, -5, 290, -291)
```

The runtime pointer values are heap addresses from one run. The independent
position, object index, model index, packet flags, and packet counts establish
the disk-to-runtime-to-render correspondence. The `0x004d11d0` calls seen in
the same run were from the player/special-object path; the Warehouse scene
geometry witness is the `0x004d14d0` call above.

## Native pre-backend packet boundary

The native recreation now carries this proven portion of the path in
[`render_packet_builder.hpp`](../../src/trg/render_packet_builder.hpp) and
`render_packet_builder.cpp`. `GameplaySession::render_packets()` obtains the
current [`LevelRenderSnapshot`](../../src/trg/level_render_snapshot.hpp) from
the loaded TRG/PSX runtime and turns each face into a renderer packet. The
builder applies the recovered Q12 object-position/camera transform, preserves
the `0xb0` polygon format and the three-or-four vertex decision, carries the
runtime material index/checksum, and performs the observed half-texel UV
normalization when a material-dimension resolver is supplied. The projector
callback is intentionally explicit: the remaining viewport/FOV/projection
calibration and raster submission are not hidden behind an invented backend.
The native integration test exercises the complete Warehouse
file -> runtime-object -> snapshot -> polygon-packet path.

The packet list also accepts the confirmed item-object consumer. A pickup
entity is already present in the TRG scene registry; the native snapshot joins
it by source node to the 0x100-byte powerup record, selects the region-local
`ITEMS.PSX` or `SKMEDALS.PSX` model, and uses that region's material table and
texture dimensions while retaining `object_index = npos` (there is no claim
that a pickup is a static environment object). Warehouse node 17 therefore
produces both the observed scene object-17 packets and a separate subtype-6,
model-5 pickup packet stream. Unmapped type-5 subtypes remain present without
invented geometry.

## Dynamic evidence and limits

A controlled run also stopped at `0x004610f0` with the live front-end object
and model/part arguments:

```text
object argument = 0x05f3c280
model/part argument = 0x05f3cc80
object +0x1f = 2
object +0x1a = 0
```

The object carries the expected runtime header/vtable and the routine reads
the slot byte before consulting the per-slot model table. This is a dynamic
confirmation of the call contract, but it is a front-end skater object, not a
Warehouse environment object and is not used as evidence for object 17.

A broader renderer probe also reached `0x004d11d0` repeatedly while the
front-end skater-selection assets were being drawn. Those calls remain useful
for the indexed/special path, but are not substituted for the Warehouse
environment witness above.

- `confirmed`: render-stage call to the attached environment list, runtime
  object traversal, slot/model-table selection, model packet header/count walk,
  the live Warehouse object-140/model-140 bridge, and the
  `0x004d14d0` state-geometry consumer; per-view polygon arena and bucket-head
  setup; transformed vertex records;
  variable-size face walking; color resolution; polygon allocation, texture
  pointer/UV population, quad splitting, bucket linking, polygon-list
  consumption, opcode/state dispatch, E1/E3 control packets, viewport
  submission, dispatch-table initialization, solid/textured/Gouraud
  triangle/quad emitters, line and rectangle emitters, general polygon path,
  draw synchronization/presentation, and the animation pose to per-part model
  packet handoff through `0x004610f0`.
- `observed`: live `0x004d11d0` calls for front-end assets and the separate
  flagged/special-object branch; exact original Direct3D enum names and the
  full meanings of several packet flag bits.
- `inferred`: detailed meanings of the original engine's C++ class names.
