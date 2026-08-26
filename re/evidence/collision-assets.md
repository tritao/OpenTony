# Collision asset/runtime linkage

Status: strong static linkage; one runtime model/normal match

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

## What remains separate

The PSX 20×20 blockmap is a useful source-level spatial partition and a good
loader cross-check, but it must not be equated with the live PC zone records:
the PC query reads zone records at `0x660` bytes and candidate pointers at
`0x198` entries per zone. The conversion from PSX objects/blockmap to those
heap structures still needs a loader-side runtime snapshot. This document
therefore identifies the asset source without attempting the full level
collision serialization.
