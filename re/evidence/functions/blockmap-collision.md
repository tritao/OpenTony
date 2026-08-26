# Warehouse PSX blockmap to runtime collision

Status: confirmed disk blockmap to runtime-object collision path; the PC zone
loader's treatment of the per-cell header words is also recovered
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004b2450`, `0x004667e0`, `0x004660b0`, `0x004638d0`, `0x00462a20`

This is the strongest current bridge from an offline PSX scene structure to a
runtime consumer. It does not assign meanings to the two per-cell words whose
semantics remain unknown.

## Offline Warehouse data

`SKWARE.PSX` is the exact file used by the normal Warehouse load:

```text
size          131124
SHA-256       d77cc2d18c684410c201edcd6fe05c27f90901f7c2cb0706161b20540b3c95b5
blockmap tag  offset 0x1ca00, type 0x0000000a
bounds fixed  (8716288, 6692864, 61304848, 59281424)
bounds        (2128.0, 1634.0, 14967.00390625, 14473.00390625)
cells         20 x 20
references    1480
```

The offline parser stores each cell as two words, a reference count,
the object-index array, and a zero terminator. Two cells containing scene
object 17 are:

```text
cell 147 (x=7,z=7): unknown=(0,0), objects=
  17, 23, 113, 117, 151, 158, 162, 165, 170, 211, 251

cell 167 (x=7,z=8): unknown=(0,0), objects=
  17, 113, 117, 131, 151, 158, 162, 165, 211
```

The selected offline object is object 17, whose disk record has position
`(7245, -1229, 6728)` in the parser's fixed-point scene units and model index
17. Its model is at file offset `0x3e3c` and has 20 vertices, 12 normals, and
12 faces.

## Runtime blockmap loader

`0x004b2450` parses PSX tags after it has relocated the model-offset table.
For a type-10 tag it obtains the tag payload and calls `0x004667e0`.

The decompilation of `0x004667e0` identifies `m3dzone.cpp` and establishes the
following layout:

1. The four payload words are copied to the zone bounds at
   `DAT_00567f84`, `+0x88`, `+0x8c`, and `+0x90`, with a per-zone byte stride
   of `0x660` (the equivalent global-array stride is `0x198` integers).
2. The low and high 16-bit halves of payload word 4 become the X and Z cell
   counts. Both are checked to be below `0x15`; Warehouse uses `20 x 20`.
3. The loader computes the X cell size as `(bound_2 - bound_0) / x_count`.
4. For each row and column, it skips the first two per-cell words, reads the
   disk reference count at the third word, and stores a pointer to the
   reference array in the cell table at `DAT_00567fa0`. The exact PC loader
   never reads the two skipped words again; they are source-format metadata
   that are not part of the runtime broadphase contract.
5. Each integer object index is rewritten in place to a runtime object pointer:

   ```text
   runtime_object =
       DAT_0056d438[DAT_00537c80[env] * 0x11] + object_index * 0x4c
   ```

   `DAT_00537c80[env]` maps the environment/zone index to the loaded PSX slot.
   `DAT_0056d438[slot * 0x11]` points at runtime record zero; the allocation
   has a four-byte count prefix immediately before that pointer. Therefore
   object 17 is record zero plus `17 * 0x4c`, matching the object-17 pointer
   printed by the PSX finalizer in the controlled Warehouse run.

This is a direct conversion of the offline object-number reference into the
same 0x4c-stride object record allocated by the PSX parser. It is stronger than
matching positions or model names: the game mutates the loaded cell array into
the runtime pointer array used by collision.

## Collision consumer

`0x004660b0`, also identified as `m3dzone.cpp`, is the zone broadphase. It
tests a query segment/AABB against the zone bounds, converts the query into
cell coordinates using the recovered cell size, and calls `0x004638d0` for each
intersected cell. The same routine handles paths that cross multiple cells by
clipping the segment to the zone rectangle and stepping through the grid with
integer cell coordinates; it therefore does not only test the start cell.

`0x004638d0`, identified as `m3dcolij.cpp`, walks the cell's runtime object
pointers. For each object it:

- reads the slot byte at object `+0x1f` and model index at `+0x1a`;
- selects the model through `DAT_0056d43c[slot * 0x11]`;
- reads the model's face count at model `+0x06` and its packed face stream after
  the vertex and normal arrays;
- computes world-space face bounds from the object position at `+0x08/+0x0c/+0x10`
  and the model vertices, using the `0x1000` fixed-point scale; and
- passes the object and face cache to `0x00462a20` for the actual face/query
  tests.

The runtime object record is more than position/model/link data. The PSX
parser's count-prefixed allocation runs the record constructor before copying
the 36-byte disk object fields. That constructor installs the record vtable,
zeros the three transform components at `+0x14/+0x16/+0x18`, and initializes
the three transform defaults at `+0x28/+0x2a/+0x2c` to `0x1000`. The parser
then copies the disk flags at `+0x04`, the three 16-bit transform components,
and the trailing source word at `+0x24`; finalization sets a model-derived bit
at `+0x19`. `0x00462a20` checks `+0x14`, `+0x16`, and `+0x18` together and
reports `Can't handle rotated objects` if any are nonzero. This independently
identifies the transform triplet as a real object field and marks the rotated
object case as a known runtime limitation.

The face cache is a runtime acceleration structure, not a second copy of the
scene model. For each visited object/model pair, the cell walker writes one
`0x1c`-byte entry per face into the shared cache at `DAT_005643b0`. The entry
starts with the face's packed flags/word and then stores the minimum and
maximum world-space X, Y, and Z values. A 20-entry working table at
`DAT_00567a70` remembers the object pointer, model pointer, face count, and
cache-base index for recently visited models; this is why repeated objects can
reach the face test without rebuilding the bounds.

The shared face-cache storage is bounded at 500 `0x1c`-byte records:
`0x00463580` uses the literal `0x1f4` for its allocation/cursor checks, so the
cache occupies `0x36b0` bytes from `DAT_005643b0` before the 16-byte gap to the
model-cache base.

The face test applies the query's fixed-point bounds/segment against the
cached face AABB before reading the model vertices. It uses the face's normal
and vertex indices for the oriented intersection test and, on a hit, writes
the object pointer, model index, face pointer, contact point, and hit distance
through the collision-query record. The exact writes in `0x00462a20` are:

```text
query +0x40  hit metric, initialized to 0x7fffffff and replaced by the best hit
query +0x68  RuntimePsxObjectRecord pointer
query +0x6c  contact X, clamped along the query segment
query +0x70  contact Y, clamped along the query segment
query +0x74  contact Z, clamped along the query segment
query +0x80  RuntimePsxFaceRecord pointer
query +0x84  model index (u32)
query +0x8c  hit distance/metric, initialized to 0x7fffffff
```

`0x004624d0` initializes the same fixed 0x90-byte query before broadphase
submission. It derives the horizontal/vertical segment length into `+0x44`,
computes the segment AABB at `+0x18..+0x2c`, initializes the 3x3 signed-12-bit
basis matrix at `+0x48..+0x58`, and clears `+0x68`, `+0x80`, `+0x84`, `+0x88`,
and the two hit metrics. Its generation word at `+0x8a` comes from the
incrementing global query generation counter. The face walker copies that
generation into each visited object/cache record, so repeated model visits can
reuse the 20-entry face-bound cache during one query. When the selected object
carries a collision-surface component, `0x00463d50` runs after the zone pass
and publishes the final surface basis into query `+0x78`, `+0x7a`, and `+0x7c`
as three signed 16-bit components. This is the concrete query-result boundary
used by the skater physics path; the public orientation names remain
intentionally unassigned.

The upper collision entry point also establishes the ordering between dynamic
and level geometry. `0x004628f0` first prepares the query against the global
game-object list headed by `DAT_0056af40`, using the same query-generation word
to avoid rebuilding an object's collision data twice in one query. The zone
broadphase then scans the static PSX blockmap. After the zone pass,
`0x00463d50` copies the selected object's collision-surface output into the
query-side normal fields when that object carries a collision component. This
ordering is useful for a faithful recreation: dynamic object collision and
static Warehouse geometry share the query record but enter through separate
passes.

Thus the proven consumer path is:

```text
SKWARE.PSX type-10 blockmap
  -> 0x004b2450 PSX tag dispatch
  -> 0x004667e0 zone grid and index-to-pointer rewrite
  -> 0x004660b0 broadphase cell selection
  -> 0x004638d0 runtime-object/model face walk
  -> 0x00462a20 face collision test
```

## Confidence and limits

- `confirmed`: Warehouse tag type and location, 20 x 20 grid, cell reference
  layout, skipped per-cell metadata words, pointer rewrite formula, runtime
  object stride, model-table lookup, collision call chain, fixed 0x90-byte
  query initialization, endpoint-derived AABB, basis-matrix initialization,
  generation/cache reuse, selected object/model/face outputs, contact
  coordinates, hit metrics, and surface-basis publication.
- `observed`: the exact object-17 lists in cells 147 and 167 from the offline
  parser and the matching runtime object/model records from the load run.
- `inferred`: the semantic names of the two source-format words and the
  complete interpretation of collision response fields.

The remaining useful experiment is to stop inside `0x004638d0` during a normal
Warehouse gameplay query and print the cell/object/model indices together. The
static pointer rewrite already proves the disk-to-runtime relationship; that
experiment would add a dynamic consumer-side witness.
