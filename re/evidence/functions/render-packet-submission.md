# Render model-to-polygon submission slice

Status: tested native pre-backend boundary through arena allocation/rollback, near clipping, visibility/depth classification, and caller-owned state/depth resolution; final bucket traversal remains separate

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
          -> 0x004d20f0  cull/depth/bucket boundary
             -> 0x004d2310  near-plane polygon rewrite
                -> 0x004d25c0  edge interpolation helper
             -> 0x004d3160  linked command-list consumer
```

This slice stops before Direct3D state/primitive execution and the actual
present boundary at `0x004d0ca4`. Camera remains the producer of the projection
and viewport values consumed by the clipper; asset-runtime remains the producer
of material/textured state. Renderer-state/material mode resolution and final
bucket traversal remain explicit seams.

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

## Arena allocation, rollback, and lifetime

Static disassembly fixes the cursor contract rather than merely establishing
the packet's logical prefix. Buffer preparation at `0x0042fed0` resets
`DAT_00560fd0` from the active render buffer's arena start. The stage at
`0x00467c90` selects the per-view `0x8000`-byte region and its bucket-head
array. `0x0045e8e0` captures the current cursor for its view record, links that
record into the initial head, and advances the cursor by `0x14`; therefore the
first polygon slot begins at `cursor_before_view + 0x14`.

At `0x004d1d40`, the allocator compares `DAT_00560fd0` with the exclusive end
at `0x00568628`. When the cursor is below the end it uses the current cursor
as the packet address and stores `cursor + 0xc0`. The decompiler's apparent
`+0x30` increment is thirty dwords (`rep movs` copies `0x30` words), hence the
byte reservation is exactly `0xc0`, not the stable `0x30`-byte prefix. Every
construction attempt—including each half of the eligible textured-quad
split—gets one slot; the second split allocation occurs only after the first
call returns.

The raw instruction sequence corroborates the generated model: the allocator
compares the cursor/end at `0x004d1d60`, forms `cursor + 0xc0` at
`0x004d1d81`, and stores it back to `0x00560fd0` at `0x004d1d87`. The view
setup similarly forms `cursor + 0x14` at `0x0045e9f2` and publishes it at
`0x0045e9f8`. The rejection branches use the inverse `cursor - 0xc0`, while
the consumer's loop at `0x004d3449` loads the current record's `+0x00` link
for the next iteration.

The early all-near/all-side rejection in `0x004d1d40` subtracts one `0xc0`
slot. The partial-near path enters `0x004d20f0`, which calls `0x004d2310`
only when the near bit is present. `0x004d2310` rewrites the packet in place,
does not allocate, and returns either the same packet address or null. The
caller subtracts exactly one `0xc0` on null—including an all-lateral or
fewer-than-three result—and also on a later winding rejection. A rejected
attempt therefore never reaches the bucket-head store and its slot is
immediately reusable by the next attempt.

On success, `0x004d20f0` stores the prior bucket head at packet `+0x00` and
publishes the packet as the new head. The slot and its link remain arena-owned
until the next buffer reset; `0x004d3160` follows the link and reads the packet
through dispatch without rewinding the cursor, unlinking the record, or
freeing its storage. This establishes the lifetime as
`buffer arena -> bucket head/link chain -> packet dispatch`; the failed path
has no published node.

For a textured four-vertex packet, `0x004d1d40` splits the quad before calling
`0x004d20f0` when the active view state permits it. The calls are made in this
exact order and with these three-corner packets:

```text
first:  (v0, v1, v3)
second: (v3, v1, v2)
```

Each split packet is classified independently, and the second allocation is
made only after the first call returns.

## Visibility, winding, depth, and bucket link

`0x004d1d40` accumulates the transformed vertices' clip words as two values:

```text
all_flags = clip[0] & clip[1] & ...
any_flags = clip[0] | clip[1] | ...
```

It discards a packet when every vertex is near-clipped (`all_flags & 0x10`),
or when all vertices share any non-near clip bit and no vertex is near-clipped
(`(all_flags & 0x3f) != 0 && (any_flags & 0x10) == 0`). A partial near clip is
forwarded to `0x004d20f0` with `any_flags`; a non-near partial side/far clip is
left for the later raster path. The target calls `0x004d2310` only for the
near bit. If that clipper returns null, the target rewinds the arena cursor by
`0xc0` and stops; the full allocation and ownership sequence is detailed
above.

The recovered `0x004d2310` near path is a circular Sutherland-Hodgman walk over
the packet's transformed stream. The material pointer at packet `+0x10`
selects a `0x14` solid or `0x1c` textured vertex stride. For each source vertex
it consumes the ordinary transform's reciprocal depth at stream `+0x0c`,
computes `factor = 1.0 / reciprocal_depth`, and temporarily stores:

```text
normalized_x = f32((x - center_x) * factor)
normalized_y = f32((y - center_y) * factor)
view_depth   = f32(depth_scale * factor)
```

The edge walk copies an inside vertex, emits no record for an outside-to-outside
edge, emits only an intersection for an outside-to-inside edge, and emits the
inside vertex followed by an intersection for an inside-to-outside edge. The
intersection helper at `0x004d25c0` sets the generated depth to the near value,
interpolates normalized X/Y, and for textured records interpolates U/V. Its
three color bytes at stream offsets `+0x10`, `+0x11`, and `+0x12` use unsigned
byte differences, truncation toward zero (`__ftol`), and byte wrap on addition.
The byte at `+0x13` is not written by the target helper.

When at least three records survive, the clipper reprojects each one. It copies
the temporary depth to projected Z, computes `reciprocal = depth_scale /
depth`, uses the still-extended reciprocal for X, and reloads its stored f32
value for Y. It then evaluates lateral flags against the camera-owned raw
viewport record at `0x00563a38`, whose observed edge order is
`[right, bottom, left, top]`:

```text
bit 0x01 if x < left       bit 0x02 if right <= x
bit 0x04 if y < top        bit 0x08 if bottom <= y
```

If every generated vertex shares any lateral flag, the target changes the
packet count to zero. Otherwise counts three through six are returned as the
rewritten packet; fewer than three and more than six return null after writing
the output count. The native adapter exposes the aggregate lateral flags and
uses a deterministic current-vertex value for the helper's unwritten high
color byte.

For the ordinary three/four-corner packets, the target's projected winding
operands are:

```text
w0 = (last.y - v0.y) * (v1.x - v0.x)
   - (last.x - v0.x) * (v1.y - v0.y)

w1 = (v3.x - v2.x) * (v1.y - v2.y)
   - (v3.y - v2.y) * (v1.x - v2.x)   # quads only
```

The retail threshold at `0x00518484` is zero. A triangle is rejected when
`w0 <= threshold`, unless the reverse-winding global at `0x00563b08` is
nonzero, in which case the strict opposite comparison is used. A quad only
applies that facing rejection when `w1` has the same threshold side as `w0`;
opposite triangle sides keep the quad. This preserves the unusual operand
order and the equality behavior rather than substituting a host cross-product
convention.

Immediately before the lower classifier, `0x004d20f0` resolves the depth
offset from the renderer state word at `0x0058f30c`. It masks with `0x6000`
and selects only these exact values:

```text
0x2000 -> 0x00563a64
0x4000 -> 0x00563a7c
0x6000 -> 0x00563a88
anything else -> offset 0
```

The selected value is passed as the third argument to `0x004d26b0`, after the
polygon pointer and caller state word. The lower classifier then receives a
resolved depth offset and returns a bucket index. Its ordinary path first
applies the selected mode:

```text
mode 0: z' = f32((depth_offset + z) * 1.00)
mode 1: z' = f32((depth_offset + z) * 0.95)
mode 2: z' = f32((depth_offset + z) * 0.70 or 0.95)
mode 3: z' = f32((depth_offset + 0x005620a8 + z) * 1.00)
mode 4: z' = f32((16380.0 + z) * 1.00), and set packet flags bit 31
```

The mode-1 path may select the minimum adjusted depth; the other ordinary
paths select the maximum. Each stored vertex depth is then clamped to the
near value `10` at `0x0058089c`, replaced by `10 / z'`, and capped at `0.99`
(`0x3f7d70a4`). In the alternate display-rectangle branch, the classifier
selects the maximum existing depth and does not perform this normalization.
The mode/material/texture-state tests that resolve these inputs are still
owned by the retail caller; the native adapter takes the resolved values
explicitly.

The returned bucket is formed by `__ftol` at `0x005004f4`, whose control-word
setup truncates toward zero:

```text
bucket = clamp_0_fff(trunc(selected_depth + 0.5) >> 2)
```

On acceptance, the target prepends the packet to the selected 8-byte bucket
slot at `0x0056433c`:

```text
packet->next = bucket_heads[bucket]
bucket_heads[bucket] = packet
```

After `0x004d26b0` returns, the same caller applies a textured-material state
rewrite. It is taken only when the current level is neither 6 nor 10, the
packet state satisfies `(flags & 0x1c0) == 0x40`, and the runtime material
record reached through packet `+0x10`, `+0x14`, and then `+0x10` has no bits
`0x110`. The resulting packet state is `(flags & ~0x40) | 0x80`; solid
packets, excluded levels, nonmatching state, or material flags leave it
unchanged. This write precedes the packet's prepend into the selected bucket
head.

The native `RenderDepthStateInputs`/`RenderDepthStateResolution` seam records
these exact inputs and outputs without taking ownership of camera production,
material parsing, or the lower classifier's already-resolved mode. Tests cover
all three exact mask values, the zero-offset default, and each flag-rewrite
guard.

Therefore face/split calls remain in source order at the target boundary, but
records sharing a bucket are traversed through a LIFO intrusive chain. The
dispatcher preserves that linked order; the final caller's choice and
iteration order across bucket heads are not promoted here.

## Ordering

The frame-level order is established by the surrounding render stage:

```text
0x0042fed0  reset cursor to the active render buffer's arena start
0x00467c90  select the per-view arena/bucket region
0x0045e8e0  bind view, consume the 0x14-byte view record, seed a head
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

Its explicit `bucketize` seam additionally:

- computes the retail all/any clip summary and distinguishes trivial rejection
  from a partial near-clip request;
- performs the recovered near-plane edge walk, edge interpolation, final
  reprojection, six-vertex bound, and aggregate lateral rejection through
  `clip_near_plane`;
- applies the verified projected winding tests, including the quad's two
  triangle operands and reverse-winding equality behavior;
- models the resolved depth modes, f32 stores, near normalization, forced
  state-bit update, and bucket quantization; and
- exposes `bucket_heads` and `next_polygon` indices so the native tests verify
  the exact per-bucket head-prepend ordering without pretending that host
  vector addresses are retail arena pointers.

`RenderPolygonArena` models the observed `0x14` view-record advance, fixed
`0xc0` polygon reservations, exclusive-end test, and one-slot rollback. The
success and failed-near-clip fixtures in `render_polygon_bucket_test.cpp`
couple those cursor transitions to bucket-link construction and
`RenderCommandDispatcher`, proving that accepted slots survive dispatch while
the rejected tail slot is reused, the earlier committed slot remains live, and
the failed attempt never appears in the list.

`split_textured_quad` models the preceding retail split as a separate
caller-resolved operation and tests that its output records retain the exact
`(v0,v1,v3)` then `(v3,v1,v2)` order.

The ordinary seven-word arithmetic is implemented by
`project_common_vertex` in `src/camera/camera_math.hpp`. The native tests
cover the live Warehouse constants and projected output, the raw clip bits,
the 0x30 polygon prefix contract, half-texel UVs, and multi-face output order
and working-record offsets. `render_polygon_bucket_test.cpp` covers winding,
all/any clip outcomes, near-plane output order and interpolation, depth modes
and f32-derived bucket indices, forced state flags, same-bucket LIFO links,
arena success/rollback, slot reuse, and dispatch lifetime.

## Confidence and exclusions

- Confirmed: packet header/stream offsets, eight-byte ordinary source records,
  seven-word transformed records, ordinary projection arithmetic, polygon
  prefix/format/count, textured UV dimensions, stage ordering through polygon
  construction, clip all/any rejection, projected winding, depth quantization,
  bucket head-prepend links, near-plane edge/reprojection arithmetic, the
  `0x14` view-record cursor advance, `0xc0` slot reservation, failed-clip
  rollback, and arena-owned storage through linked dispatch.
- Observed: the helper's unwritten high color byte, the alternate source-flag
  path, global/material depth-mode resolution, per-face color table details,
  and final backend state/primitive dispatch.
- Open: complete mode-selection inputs across all renderer paths, final
  bucket-head iteration order, special source-flag geometry, and Direct3D
  device calls.

The present boundary remains the separate `0x004d0ca4` DirectDraw `Flip`
callsite. No native packet-build event is treated as a displayed frame.
