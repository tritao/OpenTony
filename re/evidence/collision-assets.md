# Collision asset/runtime linkage

Status: strong static linkage; runtime object/model/normal linkage confirmed

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

## Package entries

`build/disc/files/SETUP/data/ALL.PKR` is a `PKR2` archive. Its index has 21
directory buckets and 3,771 fixed-size file records. The collision-relevant
level resources are in the `data/` bucket:

| resource | payload size | parsed scene |
|---|---:|---|
| `data/SKHAN.PSX` | `0x3430c` | 470 objects, 471 models, one 20×20 blockmap, 1,805 object references |
| `data/SKWARE.PSX` | `0x20034` | 252 objects, 288 models, one 20×20 blockmap, 1,480 object references |

`SKHAN` is the packaged Hangar level and `SKWARE` is the Warehouse level
selected by the PC loader. The measurements above come from the conservative
PSX reader already used by the asset tests; no texture or gameplay metadata is
needed for the collision linkage.

## PSX model bytes versus the PC collision model

The version-4 PSX model block has the same geometry organization consumed by
the PC collision code:

```text
model +0x00  u16 flags
      +0x02  u16 vertex_count
      +0x04  u16 normal_count
      +0x06  u16 face_count
      +0x08  u32 radius
      +0x0c  six int16 bounds values
      +0x18  u32 unknown
      +0x1c  8-byte vertex records
              8-byte normal records
              length-delimited face records
```

Each PSX face record stores a byte length in its upper halfword. The PC static
walker advances by that length and the dynamic walker uses the same value. The
PC face word is a converted form of the PSX face word: the runtime path reads
the normal index from bits `3..15` of the shifted normal word, while the PSX
asset stores the normal index directly. A native loader must perform that
conversion (and preserve the raw surface/base flags) rather than treating the
PSX face word as a byte-for-byte PC heap record.

## Runtime cross-check

The existing `collision-face2` trace repeatedly records a static hit with:

```text
model index = 171
model kind  = 6
model counts = (14 vertices, 6 normals, 6 faces)
normal      = (1, -3867, -1351)
surface     = 0x0010
PC normal word = 0x0020  (normal index 4 << 3)
```

`data/SKHAN.PSX`, model 171, has exactly 14 vertices, 6 normals, and 6 faces;
its normal 4 is `(1, -3867, -1351)`. Its corresponding face has surface flags
`0x0010` and PSX normal index `4`. This is an evidence-backed connection from
the live PC collision model table through the query result to the packaged
Hangar geometry, not merely a name match.

The runtime PC model cache also stores world AABBs as
`model_origin + vertex * 0x1000`, while query endpoints are shifted by 12 bits
before local face tests. That agrees with the PSX 12-bit fixed-point geometry
scale and explains why the short normal values are approximately 4096 in the
runtime traces.

## Runtime linked-object confirmation

The `collision-chain` Hangar trace captured the pointer stored at `q+0x68`
after the shared wrapper returned. On all 8 hits in the 20-query bounded run,
the pointer was `0x05f2e844`; its collision-facing fields were:

```text
flags       = 0
position    = (-4100096, -6782976, 9408512) signed fixed32
angles      = (0, 0, 0)
model       = index 171, kind 6
next        = 0x05f2e890
```

The next 15 linked records were contiguous at a 0x4c-byte stride and carried
model indices 172 through 186, all with model kind 6. The level-building path
also computes `count*0x4c + 4`, stores the count in the leading word, and
writes each element through `+0x4a`. The runtime and static evidence therefore
identify the full element stride and count-prefixed array shape. The
constructor initializes the scale words at `+0x28/+0x2a/+0x2c` to Q12
identity (`0x1000`), and the loader copies them field-for-field; the remaining
tail field meanings remain open.

The follow-up three-call `collision-cull-scale1` Hangar capture sampled the
same linked-root path at `0x004f43e0`/`0x0046297e`. Every readable sampled node
had `matrix_scale_q12 = [4096, 4096, 4096]`; the observed flags were `0x110`
and `0x111`, and all three cull returns reported zero face-test survivors.
This is negative evidence for a non-identity transform in the normal Hangar
object set, but it confirms that the newly exposed tail reads are valid on the
live 0x4c-byte records.

The query result independently reported model index 171/kind 6. The same
record resolved through the live kind-6 table at `0x05da6d18` to model data
with 14 vertices, 6 normals and 6 faces, including normal 4
`(1, -3867, -1351)`. Thus the runtime chain is now evidenced as:

```text
q+0x68 linked node
  -> node +0x1a/+0x1f: model index 171/kind 6
  -> kind-6 model table
  -> model data / face cache
  -> contact + normal result
```

The heap addresses are allocation-specific. The stable result is the field
layout and the agreement between the node, model table, face geometry and
query result.

The controlled `collision-dynamic-positive5` run supplied that model-171
origin to the first live linked node for one query, then restored the node
prefix before rendering resumed. The node survived `0x004f43e0`, and the
dynamic path produced a positive result through `0x00463e50`,
`0x004f4b00`, and `0x004f4c50`: body `0x05f26c84`, face `0x05db87e4`, model
171, traveled distance 29, and contact `[-4100096, -8710784, 11472896]`.
The dynamic path left `q+0x8c` at its sentinel and derived contact from
`q+0x40`, unlike the static path's `0x4000` parameter. This is the positive
runtime linkage from a live linked node through transformed model vertices and
faces to a hit result.

Static loader ownership is now partly separated as well. `0x004667e0`, called
from `0x0043e03c` and `0x004b29e6` and carrying `m3dzone.cpp` diagnostics,
initializes the live zone record and fills the per-cell candidate-pointer
table. Its serialized input has four fixed-point bounds words and a packed
cell-count word at `+0x10`; cell blocks start at `+0x14`, carry a count at
block `+0x08`, raw entries from `+0x0c`, and one trailing zero word. The loader
rewrites the entries to kind/model-table pointers before the query sees them.
The
`LevelGen.cpp` path around `0x0043d88e` builds the count-prefixed `0x4c`-byte
linked-object array. `0x00420fa0`
populates the kind-strided model table and its collision-cache entry. The
collision query therefore consumes two loader products: a spatial array of
object-list heads and a model-kind/index table; neither product is itself the
serialized PSX blockmap.

The `collision-root` capture also sampled the two nearby engine roots. The
global root used by `0x004628f0` was `0x05f26c84` and had a different model-kind
chain; the adjacent `0x0056af44` root was null. The winning node at
`0x05f2e844` therefore belongs to the selected spatial candidate list, not
automatically to the global dynamic-object root.

## What remains separate

The PSX 20×20 blockmap is a useful source-level spatial partition and a good
loader cross-check, but it must not be equated with the live PC zone records:
the PC query reads zone records at `0x660` bytes and candidate pointers at
`0x198` entries per zone. The conversion from PSX objects/blockmap to those
heap structures still needs a loader-side runtime snapshot for ownership and
creation timing. This document therefore identifies the asset source and
collision-facing linkage without attempting the full level collision
serialization.
