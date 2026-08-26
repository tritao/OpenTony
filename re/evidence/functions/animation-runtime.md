# PSX animation resource to runtime animation state

Status: confirmed `sk2anim.PSX` load and animation-table consumer path
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004b46a0`, `0x004b37a0`, `0x004b40a0`, `0x00452390`, `0x00469a30`, `0x00480730`, `0x00480950`, `0x00480fa0`

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
   area beginning at raw PSX buffer `+0x14`; each record is two 32-bit words,
   with the first consumed as the frame count and the second used as a
   source-stream end marker;
3. filters/compacts source byte streams against the default and extra animation
   index lists, whose indices are checked below `0x200`; and
4. replaces `DAT_0056d448[slot * 0x11]` with the compacted hierarchy/source
   table used by animation playback.

The function also validates that source-byte boundaries match the table's
record lengths. This means the runtime animation table is derived from the
PSX post-model data, not from the scene object's model table alone.

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
object +0x102  playback start/alternate frame
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
reverse, ping-pong, and held animation modes. `0x00480fa0` invokes the object's
virtual update hooks after the animation-state update.

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
```

This is a runtime-object bridge analogous to the Warehouse object/model bridge,
but the object is an animation-capable gameplay object and the asset-side
identity is an animation table index rather than a scene blockmap object index.

## Confidence and limits

- `confirmed`: exact `sk2anim.PSX` asset identity, load call, common PSX parser
  boundary, animation-table compaction, region sharing, runtime table
  lookup/start/update fields, and the gameplay-side `0x00469a30` selection to
  `0x00480730` consumer call.
- `observed`: 19-object/19-model offline counts and the 8-byte animation record
  stride used by the consumer.
- `confirmed`: the record's second word is used as a source-stream boundary by
  the compaction pass; selected source bytes are copied into the compacted
  hierarchy stream.
- `inferred`: the public names for the numeric playback modes and the full
  meaning of the compacted hierarchy stream. The playback fields
  above are operational names based on direct reads/writes, not claims about
  the original public class members.
