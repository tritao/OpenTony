# PSX animation resource to runtime animation state

Status: confirmed `sk2anim.PSX` load, decompression, hierarchy calculation order,
pose-buffer/build, and part/hook-position consumer path
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004b46a0`, `0x004b37a0`, `0x004b40a0`, `0x00452390`, `0x00469a30`, `0x004305f0`, `0x00430920`, `0x00464b90`, `0x00464bb0`, `0x00464c00`, `0x00464d10`, `0x00464e90`, `0x00465060`, `0x00465110`, `0x00465300`, `0x00480730`, `0x00480950`, `0x00480fa0`, `0x00480f50`

This is the next PSX family after level geometry. It uses the common PSX
parser, but its post-model data is transformed into animation tables rather
than blockmap cells or scene-object lists.

## Asset and load evidence

OpenTony's offline parser reports this exact extracted asset:

```text
file       SK2ANIM.PSX
size       604740
SHA-256    70cf84ac83e86a99815472325fa8ea875e01ef5c50152eff144731152f9ba1e5
objects    19
models     19
blockmaps  0
vertices   405
normals    739
faces      334
```

`Front_LoadGame` calls `0x004b46a0("sk2anim", 1)` after loading the level
resources. `0x004b46a0` first searches the already loaded PSX slot table with
`0x004b3230`. If absent, it allocates a free slot, formats `sk2anim.psx`,
loads it through the spool/file layer, parses it with `0x004b2450`, then calls
`0x004b40a0` to construct the default/extra animation tables.

The lower queue helper `0x004b37a0` records the requested asset in one of 20
spool entries, assigns the target PSX slot, and distinguishes normal and
special load modes. The common `0x004b39b0` spooler then uses the same
`0x00449030`/`0x0046f490`/`0x00449230` path already proven for Warehouse.

## Animation table construction

`0x004b40a0(slot, default_animation_list)` is the transformation boundary.
When the slot is not already in final state 2, it:

1. consumes the animation metadata pointer at `DAT_0056d444[slot * 0x11]`;
2. copies the animation count and the table's 8-byte records into a runtime
   area beginning at raw PSX buffer `+0x14`; operationally, the two record
   words are a source offset/pointer and a frame-count/stream-length word;
3. filters/compacts source byte streams against the default and extra animation
   index lists, whose indices are checked below `0x200`; and
4. replaces `DAT_0056d448[slot * 0x11]` with the compacted hierarchy/source
   table used by animation playback.

The function also validates that source-byte boundaries match the table's
record lengths. This means the runtime animation table is derived from the
PSX post-model data, not from the scene object's model table alone. The
low-byte read in `0x00480730` is retained as an operational detail of the
runtime table; it should not be used to rename the on-disk pair until the
intermediate compact-table layout is reconstructed completely.

## Compressed animation payload

The Warehouse `SK2ANIM.PSX` post-model tags make the animation input
concrete. Its first post-model tag is:

```text
tag type       0x2c
tag size       586160 (0x8f1b0)
payload        0x4804
animation count 218
record stride  8 bytes
source stream  payload + 4 + 218 * 8 = 0x4ed8
```

The 218 source records are pairs of little-endian 32-bit words. Their first
words are monotonic source offsets in the tag payload and their second words
are frame-count/stream-length values. The second words have zero high halves
in this asset. The next post-model tag is type `0x52454948` (`REIH`), size
`0x28`, with the hierarchy payload at `0x939bc`.

The same leading table invariant holds for the smaller type-`0x2a` resources:
the first word is `N`, the second is `4 + N * 8`, and the `N` eight-byte records
precede the source stream. The PC animation builder at `0x004b40a0` consumes
this shared layout without branching on the tag value. The two numeric tag
values therefore remain an authoring/asset-selection distinction, not two
different proven PC runtime object types.

`0x004305f0` is the compressed-channel decoder called by
`0x00430920`. The first byte selects both an interpolation/repetition count
(high nibble) and a channel encoding (low nibble): encoding zero stores
literal 16-bit targets with interpolation, encodings one through thirteen
read signed packed deltas, `0xe` repeats one 16-bit value, and `0xf` emits
zeroes. It returns the next byte in the stream, allowing six channels to be
decoded consecutively for each model part and frame. The exact channel order
(rotation convention versus translation convention) remains open.

## Hierarchy and pose/deformation handoff

`0x00430920` (`Decomp_GetAnimTransform`, identified by the executable's
`decomp.cpp` source string) is the first direct disk-animation-to-pose
boundary. For a runtime animation object it reads the region slot at `+0x1f`,
animation index at `+0xf6`, and frame at `+0xf4`. For type `0x2c` data it:

1. allocates `object +0xec` for `(maximum record word + 2) * model_count *
   0x0c` bytes, where Warehouse has 19 models/parts;
2. allocates a `model_count * 2`-byte calculation-order array at `+0xe8`;
3. reads the `REIH` hierarchy, finds the single root, and emits a parent-
   before-child order;
4. decodes six signed 16-bit channels per part/frame into 0x0c-byte records;
   and
5. converts each part through the parent transform, adding the parent's
   world translation to the child output. The returned current pose is an
   array of 0x18-byte per-part records at `object +0xec`; their translation
   words are at `+0x12`, `+0x14`, and `+0x16`.

The raw `REIH` payload contains 20 16-bit values, but the runtime model
count is 19 and the decompressor's calculation-order loop is bounded by 19.
The twentieth value is therefore not promoted here to a twentieth runtime
joint.

`0x00464b90` (`M3dUtils_ReadHooksPacket`) binds the region's hook packet into
the slot-indexed hook table by storing `packet + 4` at the selected region
slot's `+0x24` metadata/hook pointer. The player-side caller selects the
packet at `0x00534620 + object[+0x2cc0] * 0x14`; the static packet values and
the downstream transform contract are listed in [skater-asset-runtime.md](skater-asset-runtime.md).
`0x00464c00` (`M3dUtils_ReadLinksPacket`) binds a
link packet to the object: it stores the packet payload at `+0x140`, allocates
0x18-byte pose records at `+0x138`, allocates 0x0c-byte packet-link records at
`+0x13c`, and resolves packet IDs to pose slots. `0x00464bb0`
(`M3dUtils_ClearPoseStuff`) releases those two object-owned buffers. `0x00465300`
then builds the final per-part pose cache, composing any
packet-provided local transforms with the decompressed animation pose and
filling untouched parts from the `+0xec` pose array. This is the first
runtime deformation-oriented structure recovered here; the full packet
record and matrix-stack semantics remain open.

`0x00464d10` and `0x00464e90` are downstream part-position and hook-position
helpers. They select either the decompressed pose, the tween buffer at
`DAT_00567cb0`, or the cached `+0x138` pose, transform it with the part/hook
record and object rotation, and return a fixed-point world position after
adding object position at `+0x08/+0x0c/+0x10`. The hook helper is consumed by
runtime effects such as the rail/camera effect at `0x0046da70` and the dust
effect at `0x00431320`. This proves that animation pose data reaches live
world-space consumers. The renderer-side per-part model consumer is
`0x004610f0`: it takes the same 0x18-byte pose records (or the `+0x138` cache),
composes each record with the object transform, and submits each corresponding
PSX model packet through `0x004d11d0`. The recovered operation is rigid
per-part transform composition; no vertex-weighted skinning is claimed.

The hook record's first three words have a narrower proven contract than their
public names suggest. `0x00464e90` reads the final signed-16 word at
`hook + 0x06` as a part index and selects that part's 0x18-byte pose. It then
passes the hook record, pose record, and the object's transform block at
`object + 0x118` to `0x004f5160`. That helper copies hook words `+0x00`,
`+0x02`, and `+0x04` into three global transform inputs, copies the complete
12-word pose record, copies six transform words from `object + 0x118`, and
invokes the lower matrix/position helpers. The words are therefore live
transform inputs, while their axis/vector meanings remain intentionally raw.

`0x00465060` and `0x00465110` implement the alternate/tween path. The latter
selects adjacent source frames from the animation record, computes a 12-bit
interpolation fraction, and writes 0x18-byte joint records into the global
`DAT_00567cb0` buffer. `0x00480f50` ties the object-side setup together by
initializing the hook/pose buffers, updating the tween state, and—when the
object flags permit—calling the pose builder. Its direct external caller is
not recovered, so the method contract is recorded without claiming that
this exact entry is the only live update route.

`0x00452390` is the adjacent region-sharing helper used by the front-end. It
copies animation/hierarchy pointers from one loaded model region to another
and optionally shares the raw PSX buffer. This is the supported way for two
player/model regions to reuse an animation resource.

## Runtime animation consumer

`0x00480730`, identified by its source string as `ob.cpp`, starts an animation
on a runtime object. The object fields and table lookup are:

```text
object +0x1f  PSX/model region slot
object +0xf6  animation index selected by the caller

animation_table = DAT_0056d444[slot * 0x11]
animation_count = animation_table[0]
record          = animation_table + 8 + animation_index * 8
frame_count     = low byte of record[0]

object +0x106  frame-count/current animation byte
object +0x0f4  current frame
object +0x0f6  current animation index
object +0x0f8  animation mode/state
object +0x100  playback direction
object +0x101  playback end frame
object +0x102  signed playback alternate/next endpoint byte
object +0x104  fixed-point frame accumulator
```

The exact `record[0]` semantic is called a frame count here because the
consumer bounds the requested frame range against it and stores its low byte
at `object +0x106`. The second 32-bit record word is independently tied to
source data by `0x004b40a0`: it computes each source range's end from that
word, verifies the ranges are contiguous, and either copies the selected
bytes into the compact hierarchy stream or zeroes the record's relocated
source pointer. It is therefore a source-stream boundary/offset field, not an
additional frame-count byte.

The update routine `0x00480950` advances the same object fields for forward,
reverse, ping-pong, and held animation modes. The bounded mode-3 clock phase,
including signed remainder and equal-target behavior, is recorded in
[animation-mode3-clock.md](animation-mode3-clock.md). `0x00480fa0` invokes the
object's virtual update hooks after the animation-state update.

The playback update also fixes several fields that are useful for a faithful
object recreation. `0x00480730` initializes the selected range at `+0xf4` and
`+0x101`, stores the original/start frame at `+0x114`, clears the fractional
accumulator at `+0x104`, and sets the completion byte at `+0x107` when the range
is already a single frame. The updater treats `+0xf4` and `+0x104` as the
high and low 16-bit halves of one signed 16.16 frame accumulator, adding or
subtracting a time-scaled `+0x108` rate. Modes `0` and `2` use the selected
endpoint range, mode `1` wraps through the selected frame count, mode `3`
derives a ping-pong frame from `+0xfa/+0xfc` and the time origin at `+0xfe`,
and mode `4` reverses at the endpoint while exchanging the active endpoint
with the saved start at `+0x114`. The numeric mode values are proven; public
names for them remain provisional.

### Endpoint-transition slice result

The focused mode-0/2 transition at `0x00480950` is an inclusive endpoint
state machine. The update reads the signed `+0x102` byte before applying the
next fixed-point step:

```cpp
if ((mode == 0 || mode == 2) &&
    ((direction == 1 && frame >= endpoint) ||
     (direction == -1 && frame <= endpoint))) {
    if (static_cast<std::int8_t>(alternate_endpoint) < 1) {
        finished = true;
    } else {
        swap(endpoint, alternate_endpoint);
        direction = -direction;
    }
}
```

The positive alternate is therefore a next endpoint, not a blend value.
Byte `0` and signed-negative bytes—including the wrapper's `-1`, stored as
`0xff`—are terminal sentinels. The fixed-point step can cross the endpoint;
the mode-0 post-step clamp stores the reached inclusive endpoint. Native
tests cover exact-frame completion, signed-negative sentinels, positive
endpoint exchange in both directions, a 1.5-frame crossing, and the rule that
only `-1` substitutes the last frame during request setup. Confidence:
confirmed from the decompiled branch/field accesses and the existing runtime
field evidence; native behavior is deterministic but not a new retail trace.

The gameplay-side consumer is `0x00469a30`. Called from the per-level gameplay
update, it walks the active player pointers at `DAT_0056a858` and
`DAT_0056a85c`, filters for the gameplay object state used by the player model,
and calls `0x00480730` when the selected animation index is the normal or one
of the two special transition indices (`0x5d`/`0x5f`). This closes the runtime
consumer side: the animation table is not only load-time state; the gameplay
update selects an object animation and reaches the common start routine.

## Proven bridge

```text
SK2ANIM.PSX
  -> common PSX file/spool/parser path
  -> 0x004b40a0 animation-table compaction
  -> DAT_0056d444[slot * 0x11]
  -> 0x00480730 object animation selection
  -> 0x00480950 frame/update state
  -> 0x00430920 compressed-frame decode and hierarchy composition
  -> 0x00465300 pose/hook composition
  -> 0x00464d10 / 0x00464e90 world-space part and hook consumers
```

This is a runtime-object bridge analogous to the Warehouse object/model bridge,
but the object is an animation-capable gameplay object and the asset-side
identity is an animation table index rather than a scene blockmap object index.

The native counterpart is split at the same seam. `PsxAnimationRuntime` retains
the count/record/source-stream products and the raw `REIH` payload, while
`PsxAnimationPlaybackState` owns the recovered object playback bytes at
`+0xf4/+0xf6/+0xf8`, `+0x100..+0x108`, `+0x114`, and implements the explicit
fixed-point update clock for modes 0 through 4. The caller supplies the retail
time-scale and animation clock, so replay tests do not depend on a host frame
timer. `decode_psx_animation_channel` implements the adjacent
`0x004305f0` boundary: literal/interpolated values, 1..13-bit signed packed
deltas, repeat, zero-fill, and the returned consumed-byte position. This is a
runtime animation-object boundary; channel order and the pose matrix/hook
packet semantics remain separate evidence-backed seams.

## Confidence and limits

- `confirmed`: exact `sk2anim.PSX` asset identity, load call, common PSX parser
  boundary, animation-table compaction, region sharing, runtime table
  lookup/start/update fields, compressed-channel decoder, hierarchy root and
  calculation order, pose-buffer geometry, and the gameplay-side
  `0x00469a30` selection to `0x00480730` consumer call.
- `observed`: 19-object/19-model offline counts and the 8-byte animation record
  stride used by the consumer, and six decoded channel words per part/frame.
- `confirmed`: the record's first word supplies the source offset/pointer and
  its second word supplies frame-count/stream-length data to the decompression
  path; selected source bytes are copied into the compacted hierarchy stream.
- `inferred`: the public names for the numeric playback modes, exact channel
  order, and hook-record prefix/header semantics. The playback and pose field names
  above are operational names
  based on direct reads/writes, not claims about original public members.
