# Render model-to-polygon submission slice

Status: modeled native pre-backend boundary; exact retail ABI and bucket helper remain separate

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

Question: what records cross the ordinary model submission path, and in what
order, before culling/bucketing and Direct3D command consumption?

## Selected chain

```text
0x004d14d0  M3D_SubmitModelState
    -> 0x004d29e0  M3D_TransformVertices
       -> 0x00570878  seven-word transformed-vertex records
    -> 0x004d18b0  M3D_SubmitFacePolygons
       -> 0x004d1d40  M3D_BuildD3DPolygon
          -> 0x004d20f0  cull/depth/bucket boundary (next slice)
             -> 0x004d3160  linked command-list consumer
```

This slice stops after polygon construction. It does not assign the
Direct3D meanings of the packet flags, replace the unresolved bucket helper,
or move the present boundary from `0x004d0ca4`.

## Inputs and output records

The common submitter receives a model-packet pointer and a renderer state word.
The packet header and stream consumed by this path are:

```text
packet +0x00  flags (u16)
packet +0x02  vertex count (u16)
packet +0x04  normal count (u16)
packet +0x06  face count (u16)
packet +0x1c  packed geometry stream
```

For the ordinary model branch, each source vertex is eight bytes:

```text
source +0x00  signed x (s16)
source +0x02  signed y (s16)
source +0x04  signed z (s16)
source +0x06  source flags/auxiliary word (u16)
```

`0x004d29e0` writes one seven-word record per transformed source vertex at
`0x00570878`:

```text
working +0x00  projected X (f32)
working +0x04  projected Y (f32)
working +0x08  projected Z before reciprocal conversion (f32)
working +0x0c  reciprocal depth (f32)
working +0x10  source flags (integer bits in float storage)
working +0x14  clip flags (integer bits in float storage)
working +0x18  auxiliary/override value (f32)
```

The ordinary projection formula is the recovered consumer contract:

```text
pre[i]       = f32(dot(linear_row[i], s16(x, y, z)) + bias[i])
reciprocal   = f32(depth_scale / pre[2])
projected_x  = f32(reciprocal * pre[0] + center_x)
projected_y  = f32(reciprocal * pre[1] + center_y)
projected_z  = pre[2]
```

The ordinary clip bits are `0x01/0x02` for X outside the left/right edges,
`0x04/0x08` for Y outside the top/bottom edges, `0x10` for Z below the near
limit, and `0x20` for Z at or beyond the far limit unless state bit `0x10`
suppresses that far test. The source-flag `0x10` branch is outside this
ordinary contract.

`0x004d18b0` then walks the variable-size face stream using each face record's
length word. `0x004d1d40` consumes the selected transformed vertices and
material/color state and produces a 0x30-byte polygon record with this stable
prefix:

```text
polygon +0x00  next linked-record pointer
polygon +0x07  packet format byte, `0xb0` for this model path
polygon +0x08  packed face/render flags
polygon +0x10  runtime scene-material / PC-texture pointer for textured faces
polygon +0x14  vertex count, three or four
polygon +0x18  variable vertex/color/UV stream
```

Every output vertex starts with `(projected_x, projected_y, projected_z,
reciprocal_depth, color)`. Solid vertices use a 0x14-byte stride. Textured
vertices append normalized U/V and use a 0x1c-byte stride. The texture
normalization is:

```text
u = (float(source_u) + half_texel) / float(texture_width)
v = (float(source_v) + half_texel) / float(texture_height)
```

The material identity is the post-loader runtime material/checksum pair. The
renderer does not re-resolve the original disk texture index at this stage.

## Ordering

The frame-level order is established by the surrounding render stage:

```text
0x0045e8e0  bind view, reset polygon arena, initialize bucket heads
0x0045f530  traverse object list and select model/state path
0x004d14d0  submit one model packet and prepare transform state
0x004d29e0  write completed transformed-vertex records
0x004d18b0  walk faces in packet order
0x004d1d40  allocate/populate each polygon packet
0x004d20f0  cull, classify, and link accepted polygons
0x004d3160  consume linked polygon/command records
0x004d0c30  renderer present wrapper
0x004d0ca4  DirectDraw `Flip` callsite
```

The native slice preserves the observable source order at the portable
boundary: entity order, face order, corner order, then polygon output order.
The returned `working_vertex_offset` identifies the contiguous transformed
records associated with each output polygon. This is an explicit semantic
adapter; it does not claim that the flattened snapshot is the retail model's
shared vertex table.

## Native reconstruction and tests

`RenderPacketBuilder` in
[`render_packet_builder.hpp`](../../src/trg/render_packet_builder.hpp) and
[`render_packet_builder.cpp`](../../src/trg/render_packet_builder.cpp) models
the pre-backend record boundary. It:

- accepts raw PSX-space face vertices, Q16 object position, and camera state;
- supplies the recovered view-space input to an explicit projection callback;
- emits the `0xb0` polygon format and three-or-four vertex decision;
- retains object/model/face identity and runtime material/checksum fields;
- normalizes textured UVs only when texture dimensions are available; and
- returns transformed working records plus polygons in source traversal order.

The ordinary seven-word arithmetic is implemented by
`project_common_vertex` in `src/camera/camera_math.hpp`. The native tests
cover the live Warehouse constants and projected output, the raw clip bits,
the 0x30 polygon prefix contract, half-texel UVs, and multi-face output order
and working-record offsets.

## Confidence and exclusions

- Confirmed: packet header/stream offsets, eight-byte ordinary source records,
  seven-word transformed records, ordinary projection arithmetic, polygon
  prefix/format/count, textured UV dimensions, and stage ordering through
  polygon construction.
- Observed: the runtime polygon allocation and linked-list handoff at
  `0x004d20f0`.
- Open: exact projected winding tests, clipping versus trivial rejection,
  depth-bucket formula, bucket insertion direction, special source-flag path,
  per-face color table details, and final backend state/primitive dispatch.

The present boundary remains the separate `0x004d0ca4` DirectDraw `Flip`
callsite. No native packet-build event is treated as a displayed frame.
