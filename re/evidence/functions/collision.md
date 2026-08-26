# Collision/query subsystem

Status: confirmed interface and static/dynamic hit arithmetic; native PSX scene
replay matches live PC results; linked-object prefix, winning-object pointer,
and linked broad-phase handoff are runtime-confirmed

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

Addresses: `0x0049db80`, `0x00497f40`, `0x00496550`, `0x00494210`,
`0x00499710`, `0x004993f0`, `0x004624d0`, `0x00466090`, `0x004660b0`,
`0x004628f0`, `0x004638d0`, `0x00462a20`, `0x00463d50`, `0x0048ea80`,
`0x004f43e0`, `0x004f4940`, `0x004f4b00`, `0x004f4c50`, `0x004f4130`,
`0x004f4240`, `0x004f5540`, `0x004e23a0`, `0x004e2070`, `0x004667e0`,
`0x00420fa0`, `0x0043d88e`, `0x004801d0`, `0x004801f0`

## Result

The strongest reconstruction is a cdecl swept-line query whose wrapper is
`0x00466090`. It prepares and traverses a query record, then leaves the hit
record in caller-owned memory. The wrapper itself returns zero unconditionally;
hit/no-hit is represented in the record, not in `EAX`.

Conceptually, without asserting final C++ names or packing:

```text
Skater_PhysicsDispatcher (0x0049db80)
  ├─ ground/collision handler 0x00496550
  ├─ in-air handler 0x00497f40
  ├─ other physics handler 0x00494210
  └─ other physics handler 0x00499710
       ↓
  query setup 0x004624d0
       ↓
  query wrapper 0x00466090(q, mode)
       ↓
  scene/zone traversal 0x004660b0(q, mode)
       ├─ dynamic/linked zone path 0x004628f0
       │    └─ object cull 0x004f43e0 → oriented/model checks
       │         0x00463e50 → 0x004f4b00/0x004f4c50
       └─ zone/block candidate path 0x004638d0
            ├─ candidate cull 0x004f4940 → 0x004f4130/0x004f4240
            └─ face/triangle test 0x00462a20
                 ↓
            hit finalization 0x00463d50
                 ↓
            q: hit, contact, normal, face, distance, flags
```

The query wrapper and scene traversal are reusable below the skater physics
handlers. The triangle tester is the lowest clearly identified primitive in
this path; it performs a segment-plane and triangle-side test rather than
merely forwarding to another opaque collision function.

## Candidate interface table

These are cdecl-shaped signatures inferred from stack arguments and return
sites. `SLineInfo` is the provisional record name used in the reconstruction
schema; the original PC code uses caller-local storage.

| Address | Main callers/callsites | Tentative signature | Output/return behavior |
|---|---|---|---|
| `0x004624d0` | ground `0x00496944`, `0x00496fc6`; in-air `0x00498a4e`, `0x00498aae`, `0x00499394`; other handler callsites `0x00494fa5`, `0x00495002`, `0x00495677`, `0x00495868`, `0x00499acf`, `0x00499dbb` | `void InitLineInfo(SLineInfo*)` | fills length, basis, bounds, sentinels, and query stamp |
| `0x00466090` | `0x00496950`, `0x00496fd2`, `0x00498a61`, `0x00498abc`, `0x004993a3`, plus other engine callers | `int QueryWrapper(SLineInfo*, int mode)` | calls scene traversal; returns constant zero; result remains in record |
| `0x004660b0` | wrapper `0x00466090`; a few direct level/physics paths | `void QueryScene(SLineInfo*, int mode)` | visits linked zones and spatial cells, then calls finalizer |
| `0x004628f0` | scene traversal `0x004660b0` when mode is nonzero | `void VisitLinkedZones(void* root, SLineInfo*)` | tests dynamic/linked collision objects; writes candidate fields |
| `0x004638d0` | scene traversal calls at `0x004662fa`, `0x0046670f`, `0x004667a5` | `void VisitModelFaces(void* list, SLineInfo*)` | builds/reuses face AABBs and invokes the face tester |
| `0x004f43e0` | linked traversal `0x004628f0` | `void CullLinkedObjects(void* root, ..., SLineInfo*, uint16 stamp)` | walks `node+0x20`, applies object bounds/cross tests, and marks tested nodes |
| `0x004f4940` | candidate traversal `0x004638d0` | `void CullCandidateObjectLists(void* head_array, ..., SLineInfo*, uint16 stamp)` | walks the selected null-terminated head array and applies the same object broad phase |
| `0x004f4b00` | oriented-object path `0x00463e50` | `uint16_t TransformModelVertices(Model*, DynamicVertex*, uint32 line_length, const int32 origin[3])` | transforms each model vertex through the prepared Q12 matrix, writes 8-byte `{x,y,z,clip}` records, and returns their AND clip mask |
| `0x004f4c50` | oriented-object path `0x00463e50` | `void ScanDynamicFaces(Model*, DynamicVertex*, SLineInfo*, LinkedObject*)` | applies face masks, selects the projected triangle, and writes the winning dynamic distance/body/face/model fields; special `0x20000` faces update a sideband model selector instead |
| `0x004f5540` | direct calls `0x0045fcbc`, `0x0045fd9c`, `0x00460dce`, `0x00460eaf`, `0x00461c5f`, `0x00464062`, `0x00464095` | `void ScaleObjectMatrix(void* object, int16_t matrix[9])` | scales matrix columns from signed Q12 tail words; writes the matrix in place and has no consumed return value |
| `0x00420fa0` | model/resource setup `0x00420e2e` | `void InitModelCacheSlot(void* object)` (`thiscall`, `ECX`) | reads the setup selector at `object+0x84`, allocates a 0x50-byte slot payload through `0x0046f490`, stores it at `DAT_0056d43c[index*0x11]`, advances the slot cursor by four bytes, writes the `0x13` cache count, and clears 19 payload words |
| `0x004e24b0` | dynamic face pass `0x004f4c50` | `void TransformNormalQ12(const int16_t basis[9], const int16_t normal[3])` | multiplies the model normal by the composed query/object matrix, shifts by 12, and saturates to signed shorts |
| `0x004e2930` | dynamic face pass `0x004f4c50` | `void ProjectCandidate(...)` | combines the selected transformed vertex and transformed normal into the signed dot/line-limit values consumed by the dynamic distance test |
| `0x004f5780` | linked transform `0x00463e50` | `void ComposeQueryBasis(int16_t matrix[9])` | left-composes the object/scale matrix with the query line basis; the resulting basis is used by vertex and candidate-normal transforms |
| `0x00462a20` | model-face traversal calls at `0x004639b2`, `0x00463d03` | `void TestFace(void* model, SLineInfo*, void* cache)` | segment/triangle test; updates nearest hit fields; no useful return |
| `0x00463d50` | scene traversal call at `0x004667c3` | `int FinalizeHit(SLineInfo*)` | returns `1` iff `q+0x68` is nonzero and writes the normal |
| `0x0048ea80` | ground/in-air calls `0x0049587e`, `0x00495a4f`, `0x0049695a`, `0x00496fdc`, `0x00498a6e`, `0x00498ac9`, `0x004993b0`, `0x00499ae5`, `0x00499dd5` | `void ConsumeHitFlags(SLineInfo*)` | translates face metadata into shared collision/material flags |

The two important “boolean” distinctions are therefore explicit: the
low-level face tester is write-only from the caller's perspective, the
finalizer returns a real hit boolean, and the wrapper intentionally does not.

## Original source alignment

The packaged PSX debug symbol database is not being used as a PC address map,
but it provides strong names for the shared source subsystem. It lists
`SLineInfo` and `SHit` in `m3dcolij.h`, `SFaceCache` and `SColCache` in
`m3dcolij.cpp`, and the public routines `M3dColij_InitLineInfo`,
`M3dColij_LineToItem`, `M3dColij_LineToItemZoned`,
`M3dColij_LineToThisItemCached`, `M3dColij_GetLineInfo`, and
`M3dZone_LineToItem`. The PC functions below have matching source paths,
data flow, and diagnostics. The exact one-to-one public name for each PC
address is still provisional, but the query record is best treated as the
PC port's `SLineInfo`-like object rather than an ad-hoc skater-only struct.

## Static observations

### Query setup and wrapper

- `0x004624d0` receives one pointer and initializes a query record. The first
  six words are consumed as two three-component fixed-point positions:
  `q+0x00..0x08` and `q+0x0c..0x14`. It computes the displacement, a
  fixed-point length, and a Q12 direction/transform basis at
  `q+0x48..0x58`. For a vertical-only line the basis is exactly
  `[0x1000,0,0; 0,0,-s; 0,s,0]`, where `s` is `-0x1000` for a downward
  line and `+0x1000` otherwise. For a nonvertical line it normalizes the
  horizontal and total squared lengths, constructs the basis from the
  horizontal X/Z ratios and vertical ratio, and applies three Q12 matrix
  multiplies through `0x004e3130`. It then computes an axis-aligned bounds
  region at `q+0x18..0x2c`. More precisely, it derives
  `q+0x44` as the integer square-root of the squared endpoint delta after
  each component is shifted down by 12 bits. It initializes the nearest
  candidate fields to `0x7fffffff`, clears the hit pointer, and sets the
  per-query flags byte at `q+0x88` to zero.
- `0x00466090` takes the query pointer at `[ESP+4]` and a mode at `[ESP+8]`,
  calls `0x004660b0`, then executes `xor eax,eax; ret`. A safe tentative
  signature is therefore:

  ```text
  int Collision_QueryWrapper(CollisionQuery *q, int mode);
  ```

  The return value is not a hit boolean in this build.
- `0x004660b0` asserts `No zone information` with the embedded source path
  `H:\TonyHawk\Pc2\m3dzone.cpp`, derives collision masks from globals, and
  traverses the loaded zone data. It calls `0x004628f0` when the mode is
  nonzero, loops over a zone table with a **`0x660`-byte stride**, rejects
  zones whose bounds do not overlap the query bounds, and passes candidate
  face lists to `0x004638d0`. It finishes by calling `0x00463d50(q)`. In the
  recovered PC control flow the local zone index starts at zero and the
  post-pass increment returns after the first entry; the generic stride
  arithmetic is present, but this query routine normally consumes the active
  zone entry at index zero rather than scanning an unbounded list.

### Level/scene data entering the path

The level data is already live in process structures by the time physics calls
the wrapper; the query does not parse a file on each call.

- `DAT_0056af40` is passed as the first argument to `0x004628f0`, which then
  follows a linked list through node offset `+0x20`. This is a strong global
  root candidate for dynamic/linked collision zones.
- The static/block candidate path is separate. `0x004660b0` indexes the
  candidate-pointer table at `DAT_00567fa0`; `0x004638d0` receives the selected
  pointer, invokes `0x004f4940` to prefilter its linked object heads, and then
  walks the same node `+0x20` links for face testing. The selected candidate
  pointer is treated as a null-terminated array of object-list heads: the
  walker checks `[candidate+0]`, then `[candidate+4]`, and so on until a null
  head. This is the source path for the per-cell object record that can be
  written to `q+0x68`; it is not interchangeable with the global
  `DAT_0056af40` root.
- `DAT_00567f80` is the base of the zone table used by `0x004660b0`; the loop
  index is multiplied by `0x660` before indexing it. In each zone record the
  presence word is at `+0x00`, bounds are at `+0x04/+0x08/+0x0c/+0x10`, the
  grid-cell divisor is at `+0x14`, and signed cell counts are at
  `+0x1c/+0x1e`. The candidate-list table at
  `DAT_00567fa0` is addressed with the exact index expression
  `zone*0x198 + cell_x*0x14 + cell_z`, then multiplied by four for a pointer
  load. Thus `0x198` is a stride in 32-bit candidate-pointer entries, and the
  resulting address stride is also `0x660` bytes. It is not a 0x198-byte
  zone-record stride and should not be described as `51*8` bytes.
- The first loader-side writer that can be identified statically is
  `0x004667e0`, called from the level setup path at `0x0043e03c`. Its embedded
  diagnostics are `H:\\TonyHawk\\Pc2\\m3dzone.cpp`, `ZONE HEIGHT TOO LARGE`,
  `ZONE WIDTH TOO LARGE`, and `EnvIndex not zero`. It copies the active zone
  bounds/divisor/count fields into `DAT_00567f80`'s zone record, sets the zone
  presence flag, and populates the corresponding `DAT_00567fa0` candidate
  entries. Its per-cell input blocks have a recoverable intermediate shape:
  `0x004667e0` receives a serialized-zone buffer whose five-word header is
  bounds at `+0x00..+0x0c` and packed cell counts at `+0x10`; cell blocks start
  at `+0x14`. For each block it reads a count at block `+0x08`, publishes a
  candidate-table pointer to block `+0x0c`, converts that many raw entries in
  place, and then advances past one trailing zero word before the next cell.
  The raw
  entry values are resolved through the kind-strided model table rooted at
  `DAT_0056d438`; after conversion they are the linked-object pointers later
  consumed by `0x004638d0`. The first two words of each source block remain
  unknown. This is the strongest recovered zone/candidate-table ownership
  boundary; it still does not expose the file serialization or allocator
  interface.
- A targeted runtime probe confirmed that boundary in the Hangar load. The
  call from `0x004b29e6` returned at `0x004b29eb` with source buffer
  `0x05dd2dfc`; after the loader's diagnostic/argument setup, `ESI=0` and
  `EBX=0x05dd2dfc`. The source prefix was
  `fe3ca000, fd669000, 045cf00c, 0386e00c, 00140014`, i.e. signed bounds
  `[-29581312,-43610112]..[73199628,59170828]` and packed `20x20` cell
  counts. The live zone prefix changed from absent to present and contained
  those bounds, divisor `5139047`, and counts `20,20`. The candidate table
  changed from null entries to populated pointers; entry zero was
  `source+0x20`, and the first three cell-block cursors were
  `source+0x14`, `source+0x24`, and `source+0x3c`, demonstrating the
  count-dependent block size. The first published candidate view began with
  `[0,0,0,2,46,47,0,0]`, so this probe also confirms that the published
  pointer is an array view over the source cell's trailing/succeeding words,
  not a separately allocated object header.
- Model-kind/cache population is a separate setup stage. The routine
  `0x00420fa0`, called directly at `0x00420e2e`, reads a selector from its
  setup object at `+0x84`, allocates a fixed 0x50-byte payload through
  `0x0046f490`, stores that allocation in the selected 17-word slot rooted at
  `DAT_0056d43c`, advances the slot's first pointer by four bytes, writes the
  `0x13` count word, and clears the following 19 words. The adjacent setup
  routine populates the other region-slot fields, including the model/object
  products consumed by collision. This explains why the query can resolve
  `model_kind`/`model_index` without consulting the zone candidate table, and
  keeps the two loader responsibilities separate in the reconstruction. The
  bounded `tony-collision-model-kind-probe` captures the selector and the
  before/after 0x44-byte slot without walking unrelated table entries.
- The adjacent finalizer `0x004647c0` explains why a PSX object cannot be
  promoted directly to a faithful linked collision node from its source
  fields alone. Given a region slot, it walks the model table and then the
  count-prefixed runtime object array; for each object it writes the
  intrusive `+0x20` next link, writes the attached region slot into `+0x1f`,
  resolves `+0x1a` through that region's model table, and adds object flag
  bits from model-header and face metadata. The parser has already copied the
  three collision-facing shorts at `+0x14/+0x16/+0x18`, but the final flags
  and `model_kind` are attachment/finalization products. The native linked
  object seam therefore keeps `flags`, `model_kind`, and `body_id` caller-owned
  until this finalizer is reconstructed.
- The parser copy loop gives those three source shorts an exact byte mapping:
  disk object `+0x10` low 16 bits -> runtime `+0x14`, disk `+0x10` high 16
  bits -> runtime `+0x16`, and disk `+0x14` -> runtime `+0x18`. The native
  `PsxObject::collision_angles` view preserves this order without assigning
  a stronger asset-level name. `PsxScene::linked_collision_object_from_source`
  now builds the corresponding collision-facing view while requiring the
  finalizer's region slot, finalized flags, and attachment body ID explicitly.
- The finalizer's object loop is more specific than a generic “attachment”
  step. At `0x00464a15` it enters the first record's `+0x20` link field and
  advances by `0x4c`; it writes the next record address (or null) at `+0x20`,
  the region argument's low byte at `+0x1f`, and then resolves the record's
  `+0x1a` model index through the region-specific model table. If the model
  header byte has bit `0x10`, it ORs `0x20` into the record's flags; if the
  selected model in the global region table has bit `0x20`, it ORs `0x1000`
  into the flags. These are the only per-object flag writes proven in this
  loop; the live `0x0110` dynamic-node example therefore cannot be explained
  by this finalizer alone.
- `0x004638d0` indexes model/face data through `DAT_0056d43c`, builds cached
  face AABBs in the `DAT_005643b0` area, and then calls `0x00462a20` for each
  candidate face. Its reusable model-cache entries at `DAT_00567a70` are
  0x10 bytes each: model pointer, model-data pointer, face count, and the
  starting index in the 0x1c-byte face-AABB cache. Each AABB cache record
  stores the low 16-bit face flags followed by min/max X/Y/Z fixed-point
  bounds. These names are address-level labels only; the complete level
  collision file format was not attempted here.

### Candidate testing and result finalization

- `0x004638d0` is a candidate-list/model-face traversal routine. Its embedded
  diagnostics include `H:\TonyHawk\Pc2\m3dcolij.cpp` and
  `pFaceCache->pItem is wrong in line`, and its inner loop computes a face
  bounding box before calling `0x00462a20`.
- `0x00462a20` takes a model/object pointer, the query record, and a cached
  face item. It checks the query AABB, computes signed plane values at the
  two query endpoints, performs triangle-side tests using double-precision
  cross products, and only replaces the stored candidate when the new hit is
  nearer than the current `q+0x8c` value. Its diagnostics include
  `Can't handle rotated objects`, `pModel Wrong`, `Faces Wrong`,
  `Illegal number of faces`, and `Multiple trigger poly hit`.
- The static face-record/data path is now concrete enough to constrain a C++
  loader, even though the full level format is still out of scope. The model
  kind is read from `model+0x1f`, the model/item index from `model+0x1a`, and
  the selected model-data pointer comes from the kind-specific table at
  `DAT_0056d43c`. In that model-data block, counts are read at `+0x2`,
  `+0x4`, and `+0x6`; vertex records begin at `+0x1c`, normal records follow
  the vertex count at eight bytes per record, and the face-record region
  follows the normal count at the same stride. Each vertex/normal record is
  read as three signed 16-bit components plus padding.
- The cached face record's word at `+0x0` supplies the triangle/quad choice
  through bit `0x10`; its vertex-index bytes are **`+0x4`, `+0x5`, `+0x6`, and
  `+0x7`**. The lower 16 bits of `face+0xc`, shifted right three, select the
  normal record. The `+0x1` byte is not the first vertex index; this was
  corrected after comparing the face-test assembly with the runtime face
  words. Ghidra's decompilation of the x87 block at `0x004630ae` now resolves
  the side predicate as oriented triple products. With all vertices expressed
  relative to the query start in model-local units and `d = end - start`, the
  triangle path accepts when all three expressions are nonnegative:

  ```text
  (v2 × v0) · d >= 0
  (v0 × v1) · d >= 0
  (v1 × v2) · d >= 0
  ```

  The latter two signs are easy to lose when translating the emitted
  expressions because the compiler reverses their subtraction order. The
  quad path uses the first two tests, then checks `(v1 × v3) · d` and
  `(v2 × v3) · d`. The zero constant is the double at `0x00518d68`; equality
  takes the alternate branch visible at `0x004631a4`, but the same oriented
  tests and final nonnegative comparison apply. This is now implemented in
  `collision_reference.hpp`; it supersedes the earlier deliberately
  unverified cross-product guess.
- The first face word packs a 16-bit base flag field and a 16-bit payload
  length. The dynamic walker advances by `face_word_zero >> 16` bytes; the
  static walker advances by `face_word_zero >> 18` `uint32_t` words, which is
  the same byte length for the observed four-byte-aligned records. The
  recovered model-data view
  is therefore:

  ```text
  model_data+0x00  uint16 unknown/flags
  model_data+0x02  uint16 vertex_count
  model_data+0x04  uint16 normal_count
  model_data+0x06  uint16 face_count
  model_data+0x08  uint32 radius
  model_data+0x0c  six int16 bounds values
  model_data+0x18  uint32 unknown
  model_data+0x1c  vertex records, 8 bytes each
                  normal records, 8 bytes each
                  variable-length face records
  face+0x00       uint16 base_flags, uint16 length_bytes
  face+0x04       four vertex-index bytes
  face+0x08       four render/GPU bytes
  face+0x0c       uint16 normal_index<<3, uint16 surface_flags
  ```

  The layout is directly constrained by the PC loads and the packaged
  `SLineInfo`/model artifacts; it does not imply that the complete zone file
  parser is solved.
- Before those side tests, the face tester applies the asymmetric plane gate
  visible at `0x00462ecd..0x462ef7`: reject when `plane_start < 0` or
  `plane_end > 0`; reject coplanar endpoints; and, when `plane_start < 0x800`,
  require `plane_end <= -0x800`. This gate is independent of the later
  nearest-candidate update documented below.
- On a nearer hit, `0x00462a20` writes the following strong result
  candidates:

  | Query offset | Observed use | Confidence |
  |---|---|---|
  | `q+0x68` | model/object pointer; zero means no stored hit | observed / confirmed by runtime |
  | `q+0x6c..0x74` | interpolated three-component contact candidate | observed; semantic label inferred |
  | `q+0x78..0x7c` | three signed 16-bit components written by finalizer | observed; normal candidate inferred |
  | `q+0x80` | pointer to the winning cached/model face record; its `+0xc` word is consumed as flags | observed; confirmed nonzero on runtime hits |
  | `q+0x84` | winning model/item index copied from the tested model's `+0x1a` field | observed; exact higher-level meaning open |
  | `q+0x88` | per-query flag byte used in face filtering | observed |
  | `q+0x8c` | nearest-hit segment parameter, initialized to `0x7fffffff` | observed; `0..0x4000` scale |

  The tester also updates `q+0x40`, a traveled-distance field. The arithmetic
  is now recoverable from the machine code:

  ```text
  plane_start = dot(start - face_vertex, face_normal)
  plane_end   = dot(end   - face_vertex, face_normal)
  t = trunc(plane_start * 0x4000 / (plane_start - plane_end))
  q+0x8c = t                         // line parameter, 0..0x4000
  hit = start + trunc((end-start) * t / 0x4000)
  q+0x40 = trunc(q+0x44 * t / 0x4000) // distance from start
  ```

  The original code uses signed integer division/truncation helpers. The
  winning candidate comparison is against `q+0x8c`, so nearest-hit selection
  is parameter-ordering along the segment; `q+0x40` is the same parameter
  converted to the line's integer length. This resolves the former
  `q+0x40` versus `q+0x8c` ambiguity.
- `0x00463d50` tests `q+0x68`. On a hit it transforms the model normal data
  at `q+0x68 + 0x14`, writes the resulting three short components to
  `q+0x78..0x7c`, copies auxiliary normal data to globals, and returns `1`.
  With no hit it returns `0`. This is the strongest normal-output candidate,
  but the axis/sign convention is not finalized.
- `0x0048ea80` is a result consumer immediately after several physics queries,
  not the geometric tester. If `q+0x68` is nonzero, it reads `q+0x80` and the
  face record's word at `+0x0c`, then derives global collision/surface flags
  (`DAT_0056b768`, `DAT_0056b7b8`, `DAT_0056b7a8`, `DAT_0056b7ac`, and
  `DAT_0056b7e8`) and saves the face pointer in `_DAT_0056b77c`. These are
  material/terrain flag candidates, not confirmed final names.

  The bit extraction is concrete even though the higher-level names are not:

  | Derived global | Source expression |
  |---|---|
  | `DAT_0056b768` | `(face[0xc] >> 16) & 0x40` |
  | `DAT_0056b7b8` | `(~face[0xc] >> 24) & 1` |
  | `DAT_0056b7a8` | `(~face[0xc] >> 23) & 1` |
  | `DAT_0056b7ac` | `face[0] & 0x80` |
  | `DAT_0056b7e8` | `(face[0xc] >> 25) & 0xf` |

  The ground and in-air consumers branch on these values for wall/ground
  handling, wall riding, and surface-class behavior. They should be exposed
  to a later C++ reconstruction as raw flags plus a provisional decoded view,
  not yet as named materials.

  The packaged cross-build model notes make several aliases plausible, but
  they remain aliases rather than final PC material names: base `0x10` is the
  triangle bit, base `0x80` is the non-physical/invisible bit, surface `0x10`
  is wall-rideable, surface `0x40` is the large-polygon/quarter-pipe class,
  and surface `0x100` is cleared for skateable faces. The C++ view keeps both
  raw words and these provisional interpretations so a later PC runtime
  comparison can falsify them without changing the byte layout.
- The alternate model path `0x00463e50` contains the diagnostic
  `Collision check on a model with normals` and calls `0x004f4b00` and
  `0x004f4c50`; it is a reusable oriented/dynamic-object branch underneath
  the same scene traversal, not a replacement for the triangle path. The
  linked-object path is now constrained further:

  - `0x004628f0` copies q's 3x3 short basis, calls `0x004f43e0` to cull a
    linked-object list, then follows each object through `node+0x20`. A node
    is visited once per q stamp (`node+0x06`) and enters `0x00463e50`.
  - `0x004f43e0` reads each object's fixed-point origin at `+0x08..0x10`,
    collision flags at `+0x04`, model kind/index at `+0x1f/+0x1a`, and
    optional Euler/rotation shorts at `+0x14..0x18`. It calls
    `0x004f4130`/`0x004f4240` for object-space broad-phase culling before the
    transformed face pass.
    The minimum node prefix is now represented as a 0x24-byte view: the
    opaque first word is followed by flags, query stamp, three fixed-point
    position words, X/Y/Z angle shorts, model index, model kind, and the
    32-bit next pointer. This is an in-memory ABI view only; it does not
    claim that heap nodes are serialized in a level file.
  - The list primitive at `0x004801d0` inserts a node at a caller-supplied
    root: it writes `node+0x20 = old_head`, `node+0x34 = 0`, updates the root,
    and writes `old_head+0x34 = node` when a successor exists.
    `0x004801f0` is the reciprocal unlink path. Construction paths around
    `0x0049f265` and `0x0049f360` call the object initializer and insert into
    `DAT_0056af40`; this establishes list ownership and a backward-link
    offset without claiming the full object allocation size.
  - The type-192 object vtable at `0x005194f8` has a model-update handler at
    `0x004a1060`. Its dispatch table identifies `0x2124` as the numeric-model
    case and `0x2127` as the checksum-model case. The former writes a
    region-local `u16` directly to `+0x1a`; the latter aligns the command
    cursor, calls `0x004b1de0(checksum, +0x1f)`, and writes that returned model
    index to `+0x1a`. Both cases resolve
    `DAT_0056d43c[+0x1f * 0x11][+0x1a]` and mirror the selected model header
    bit `0x10` into object flag `+0x04` bit `0x20`; `0x2127` additionally
    clears flag bit `0x1`. The same table proves `0x2128` writes a resolved
    three-component vector at `+0x4c/+0x50/+0x54`, while `0x212f`, `0x2133`,
    and `0x2137` write three-u16 fields at `+0x14/+0x16/+0x18`,
    `+0x70/+0x72/+0x74`, and `+0x76/+0x78/+0x7a`, respectively. This explains
    why `model_kind`/`model_index` are mutable runtime products rather than a
    sufficient PSX object identity. The separate `0x004a12d0` consumer scans
    `DAT_0056af40`, performs model-bound overlap against the player's AABB,
    and calls `0x0049f4c0` for platform/ground response; it should not be
    mistaken for the line-query wrapper.
  - A separate level-building path around `0x0043d88e` (with the embedded
    source path `H:\\TonyHawk\\Pc2\\LevelGen.cpp`) computes an object
    allocation size of `count*0x4c + 4`, stores the element count in the
    leading word, and constructs `0x4c`-byte elements after that header. Its
    copy loop advances by `0x4c` and writes the element through `+0x4a`,
    including the collision prefix and the `+0x34` backlink. Together with
    the runtime chain spacing, this establishes the full element stride and
    allocation shape for this loader product. The constructor initializes
    `+0x28/+0x2a/+0x2c` to `0x1000`; these three signed-short words are later
    consumed as per-column Q12 matrix factors, while the rest of the tail is
    still opaque. The second source-copy loop copies every field through
    `+0x4a` field-for-field, so source and destination scale words retain
    those exact offsets.
  - `0x004f4050` orders the two query endpoints independently on X/Y/Z and
    returns a three-bit reflection mask. `0x004f4130` expands the selected
    model bounds by two units and reflects them around the endpoint sum for
    each set mask bit. When the object has high-byte flag bit `0x02`
    (`flags & 0x0200`), `0x004f43e0` then scales each min/max axis with its
    `+0x28/+0x2a/+0x2c` Q12 factor using x87 truncate-toward-zero conversion.
    `0x004f4240` first performs strict lower-bound/upper-bound endpoint gates,
    then runs the short cross-product edge test using the query delta. These
    are now available as tested native helpers, which closes the object-space
    broad-phase arithmetic. The source of the node bytes is still loader-owned
    heap state, but its collision-facing prefix is now runtime-confirmed below.
  - `0x00463e50` transforms the query into object/model space and calls
    `0x004f4b00` to transform all model vertices into a temporary buffer.
    Each temporary record is three signed shorts plus a 16-bit clip mask.
    The transform uses the query's Q12 basis and the object-origin minus
    query-start displacement in model units. The returned AND mask must have
    `(mask & 0x60f) == 0` before `0x004f4c50` scans faces. The model data
    layout and face strides are the same as the static path. The dynamic face
    walker consumes face index bytes in +4,+5,+6,+7 order and applies the
    same scene collision-mask words before its projected-face test.
  - `0x004f4c50` applies scene collision masks, reconstructs face vertices
    from the transformed buffer, performs a projected cross-product test,
    and writes the dynamic hit body, traveled distance, face, and model-index
    fields. Its nearest comparison is against `q+0x40` (traveled distance),
    whereas the static `0x00462a20` path compares the `0x4000` segment
    parameter at `q+0x8c`. This is a real behavioral distinction, not just a
    decompiler naming artifact. The decompiled dynamic face-hit branch does
    not itself assign `q+0x6c..0x74`; those contact words are assigned by the
    static tester and by the dynamic routine's post-face-loop contact
    calculation, using the winning traveled distance. A geometrically
    accepted face whose surface word contains `0x20000` takes a separate
    branch: when `q+0x88` is nonzero it stores the model's `+0x1a` selector in
    `DAT_00567a64`, but does not publish a physical q+0x68 hit.
  - The dynamic helpers are now algebraically constrained at the machine-code
    level. `0x004e2f80` computes the three 2-D determinants from the
    transformed X/Y vertex pairs. For a triangle, a negative determinant for
    the `(v1,v2)` pair rejects the face; for a quad, the walker retries with
    `v3` in the first-vertex slot and negates the other two determinants.
    `0x004e24b0` applies the composed query/object Q12 basis to the model
    normal, and `0x004e2930` combines that result with the selected vertex
    before the candidate distance is compared against `q+0x40`. For the
    alternate quad winding, the selected vertex is the fourth transformed
    vertex rather than `v0`. The native reference now
    models the same strict-nearer update and the no-face fallback: after the
    scan it computes `u = trunc((q+0x40) * 0x1000 / (q+0x44))`, then writes
    `start + ((end-start) >> 12) * u` into `q+0x6c..0x74`. `0x004e2070` is the
    shared signed-short saturation helper: it obtains `2^15`, converts with
    the game's truncate-toward-zero x87 helper at `0x005004f4`, and clamps each
    integer component to `[-32768, 32767]`. This closes the arithmetic gap;
    The controlled `collision-dynamic-positive5` run now validates the outer
    linked-list handoff and the complete transformed-face path as well.
  - The `0x0200` matrix-transform branch has one more recoverable input boundary:
    `0x00463e50` calls `0x004f5540` with the object record and the temporary
    3x3 short matrix. That helper reads object tail words `+0x28`, `+0x2a`,
    and `+0x2c`; each is sign-extended and used as a Q12 factor by
    `0x004e23a0`, which passes the resulting matrix triplet through the shared
    signed-short saturation helper before the matrix is written back. This
    confirms that the tail is transform input, not part of the cull admission
    flags. The helper scales matrix columns independently: `+0x28` multiplies
    entries 0/3/6, `+0x2a` multiplies 1/4/7, and `+0x2c` multiplies 2/5/8;
    each product is SAR 12 and passed through the signed-short saturation
    helper. `0x004f5780` then left-composes that scaled object matrix with
    the query basis, so the dynamic vertex and candidate-normal basis is
    `query_basis * object_rotation * scale`. The ordinary Hangar objects
    sampled so far all carry identity Q12 scale, but the native linked-object
    API accepts non-identity factors directly. The final normal path remains the unscaled
    `object_rotation * model_normal` transform.
  - `0x00463d50` finalizes a winning normal by building a Q12 rotation basis
    from the object rotation at `body+0x14`, applying it to the cached model
    normal at `DAT_00564390/94/98`, and writing the three signed shorts at
    `q+0x78..0x7c`. The three helpers use one full turn per `0x1000` angle
    units and compose Y (`0x004e7de0`), X (`0x004e7c60`), then Z
    (`0x004e7f60`) Q12 rotations. Their angle conversion is also pinned down:
    each loads the float constants at `0x00518910` (`1/4096`) and
    `0x00519a08` (`6.283185482...`), executes x87 `fcos`/`fsin`, multiplies
    by the double `4096.0` at `0x00519900`, and truncates toward zero. Its
    return is `1` iff `q+0x68` is nonzero.

## Physics callsites

Static callsites to the shared wrapper (`call 0x00466090`) are:

| Physics routine | Wrapper callsites | Shared-path observation |
|---|---|---|
| `0x00496550` | `0x00496950`, `0x00496fd2` | ground/collision resolution; both are preceded by `0x004624d0` |
| `0x00497f40` | `0x00498a61`, `0x00498abc`, `0x004993a3` | in-air routine; each follows a query-record setup |
| `0x00494210` | `0x00494fb4`, `0x00495011`, `0x00495686`, `0x00495874` | another state-specific handler with repeated sweeps |
| `0x00499710` | `0x00499adb`, `0x00499dcb` | another state-specific handler with repeated sweeps |
| `0x004993f0` | none observed | writes/rotates response data but does not directly enter this query path |

The dispatcher at `0x0049db80` selects among these handlers and, on one
combined path, calls `0x004993f0` before the in-air routine. This establishes
the shared query dependency without assigning names to the physics-state
enum.

In the main skater sweep setup, the caller-side code places the previous/history
position words at `player+0xbc`, `+0xc0`, `+0xc4` in the first query vector and
the live or derived position at `player+0x08`, `+0x0c`, `+0x10` in the second
vector. Therefore the primary interpretation is `start = old/previous
position`, `end = current or swept position`; some branches replace the end
with a derived collision probe position. This is static data flow, not a final
semantic name for either player vector.

### Ground and in-air result consumers

The callers do more than test `q+0x68`; they consume the result as a collision
response basis:

- In `0x00496550`, the stack query begins at `local_cc`. Thus `iStack_64` is
  `q+0x68`, `iStack_8c` is `q+0x40`, `uStack_54/uStack_50` are the three
  normal shorts at `q+0x78..0x7c`, and `iStack_4c` is the face record at
  `q+0x80`. On a hit the routine feeds the normal into `0x0049bad0`,
  `0x00490610`, and `0x00490680`, and uses traveled distance against
  correction thresholds before deciding whether to retain or rewind the
  position.
- In `0x00497f40`, the main query begins at `iStack_e4`. Its first sweep is
  `old_position -> current_position`. If the query hits, it may repeat the
  same query with `DAT_00567c7c` set, which changes the scene mask in
  `0x004660b0`. The in-air path then classifies the hit using the face flags
  from `q+0x80`, projects velocity/response with the normal, and can enter
  wall-ride handling. It performs another old-to-current sweep later in the
  function, so one physics tick can issue multiple semantically distinct
  line queries.
- The in-air decompilation uses the normal's middle short in the wall-ride
  threshold test (`normal_middle < 0xcce`) and stores all three normal
  components into the skater response/orientation fields. This establishes
  that the normal is consumed as a signed fixed-point basis vector, not just
  as a boolean surface marker.

The later in-air callsites have distinguishable setup rather than being
aliases: `0x00498abc` repeats the old-to-current sweep after toggling the
scene mask when the first result requires a second classification, while
`0x004993a3` builds a probe from the current position offset along the stored
response/normal before querying. The shared wrapper therefore covers both
ordinary motion sweeps and post-hit follow-up probes.

## Runtime confirmation

Experiment traces: `build/debug/collision.trace.ndjson`,
`build/debug/collision-air4.trace.ndjson`,
`build/debug/collision-flags1.trace.ndjson`, and
`build/debug/collision-cull.trace.ndjson` (generated in the isolated
`re/collision` worktree; each header contains the build hash above).

The bounded probe breaks at wrapper entry `0x00466090` and at the post-query
instruction `0x0046609f`, recording the query endpoints before the call and
the result fields after it. The grounded Hangar run produced 240 completed
records: 72 with `q+0x68 != 0` and 168 with `q+0x68 == 0`.

A second bounded Hangar run recorded 2,000 completed calls while the level
was loading and settling, with 608 hits overall. The in-air handler
contributed 27 calls from one wrapper callsite, `0x00498a66`
(`0x00497f40 + 0xb26`); one of those calls hit. That hit had:

```text
line_length:       71
hit_parameter:     2101 / 0x4000 = 0.128234...
hit_distance:      9
normal_s16:        [1, -2897, 2897]
normal magnitude:  4096.98
model index:       132
face flags:        0x04200008
```

The observed distance ratio is `9/71 = 0.12676...`, differing from the
parameter only by the expected integer truncation. This is a runtime
confirmation from the in-air physics caller, not just a static
reconstruction. Its face pointer was non-null, confirming that the same
scene/face path supplies metadata to the airborne handler.

For that hit, interpreting the raw endpoint words as signed 32-bit fixed
point gives `start = [-4098781, -647119, -16628281]` and
`end = [-4098749, -420744, -16813694]`. The recorded contact is exactly
`start + trunc((end-start) * 2101 / 0x4000)` component by component, further
confirming the contact candidate's interpolation semantics.

The hit records had nonzero model and face pointers, finite distance
candidates, and contact candidates. The no-hit records retained
`q+0x68 == 0`, null face pointers, and the initialized distance sentinel
`0x7fffffff`. The same wrapper was reached from both physics-related callers
(`Skater_PhysicsDispatcher+0x1242`, `Skater_PositionWritePath+...`) and other
level-loop callers, confirming that `0x00466090` is a shared scene query rather
than a ground-only helper.

For hit records, the three short values at `q+0x78` behaved like a normalized
fixed-point normal: examples included `[1, -4093, -160]` and
`[1, -3867, -1351]`, both with magnitude approximately `4096`. The second
component was close to `-4096` on near-flat contacts and changed on sloped
contacts. Values from no-hit records were not interpreted because they can be
stale/uninitialized.

The static arithmetic explains the scale seen in the trace: `q+0x8c` values
such as `61` are small fractions of a `0x4000`-scaled segment, while
`q+0x40` values such as `29` are the corresponding integer travel distance.
The grounded trace therefore independently agrees with the recovered
parameter/distance relationship.

The native C++ scene boundary provides an independent asset-to-query replay in
[`src/collision/psx_scene_test.cpp`](../../src/collision/psx_scene_test.cpp).
Parsing the packaged `SKHAN.PSX` scene (470 objects, 471 models, one 20×20
blockmap) and querying the same signed endpoints as the PC `collision-face2`
trace produces:

```text
model=171 body=171 face=0x15cf4 parameter=61 distance=29
contact=-4100096,-8700784,11472896 normal=1,-3867,-1351
```

The native metadata result identifies object index `170`, model-face index `4`,
PSX base flags `0x1003`, and surface flags `0x0010`; its body handle is the
scene convention `object_index + 1`, while its face handle is the PSX source
offset. This is an exact replay of the corresponding PC result fields while
keeping host pointers out of the native API.

An independent replay of all 608 hit records in `collision-air4.trace.ndjson`
recomputed the signed endpoint interpolation and distance formulas. All 608
contact vectors matched exactly, all 608 traveled distances matched exactly,
and all 608 finalized normal magnitudes fell between `4094` and `4098` in the
observed short-vector scale. The same audit found 27 calls from the in-air
wrapper callsite and one in-air hit.

That audit is reproducible with the tracked standard-library checker:

```text
python re/evidence/verify_collision_trace.py \
  build/debug/collision-air4.trace.ndjson
python re/evidence/verify_collision_trace.py \
  build/debug/collision-dynamic-positive5.trace.ndjson
```

The first command checks all 2,000 wrapper records, including the 608 static
hits; the second checks the controlled dynamic hit's distance-derived contact
fallback. The checker intentionally validates recorded arithmetic invariants
only; it does not replace the native query implementation.

The probe implementation is tracked in `re/gdb/opentony/collision.py` and is
registered as `tony-collision-probe`; it records the wrapper `mode`,
`start_raw`, `end_raw`, hit, contact, normal shorts, model/face pointers, face
flags, line length, hit parameter, traveled distance, query stamp, the nine
short basis values at `q+0x48`, and the query-record mode bytes. The two later
phase attempts were stopped before level physics settled and are not used as
evidence. When the linked root is non-null, the probe additionally captures at
most 32 node prefixes (`+0x04..+0x23`) and reports null, cycle, limit, or
unreadable termination. Each readable node also gets a bounded backlink read
at `+0x34`. On a hit it also snapshots a bounded 16-node chain
from `q+0x68`, the pointer consumed by `0x00463d50`, so the result pointer and
its neighbors can be compared with the linked-node ABI without an unbounded
memory walk.

The companion bounded probes are registered as
`tony-collision-flags-probe`, `tony-collision-dynamic-probe`, and
`tony-collision-dynamic-cull-probe`. The transform-tail probe is registered as
`tony-collision-transform-probe`; it pairs `0x004f5540` with all seven direct
return sites currently identified and records the object tail words plus
temporary matrix before and after the helper. The two return sites at
`0x00464067`/`0x0046409a` are the linked-object collision path; the other five
show that the matrix helper is shared by additional model/object transforms.
The cull probe pairs entry/return at
`0x004f43e0`/`0x004f492c`, snapshots the linked prefix, and reports which
nodes still have a stale query stamp and therefore proceed to
`0x00463e50`.

The `collision-chain` Hangar capture armed 20 completed wrapper calls after
the frontend was advanced into `PLAY_GAME`. It produced 8 hits, all from mode
1 and all for model kind 6/index 171. Every hit had the same direct result
pointer and the node-shaped prefix decoded consistently:

```text
q+0x68 / model pointer = 0x05f2e844
flags                  = 0x0000
query stamp            = 1 (then incremented on later queries)
position               = [-4100096, -6782976, 9408512]  (signed fixed32)
angles                 = [0, 0, 0]
model index/kind        = 171 / 6
next                   = 0x05f2e890
```

The next 15 records were contiguous at a 0x4c-byte link-to-link stride, with
model indices 172 through 186 and the same model kind 6. The observed flags
were `0`, `0x41`, and `0x8041`; all three angle shorts remained zero in this
static Hangar object chain. The level-building path independently allocates
`count*0x4c + 4` bytes and writes through element offset `+0x4a`, so this is
now a strong full-element/array-shape identification. The tail fields are
still kept opaque in the native view except for the recovered Q12 scale words
at `+0x28/+0x2a/+0x2c`.

The follow-up `collision-root` capture sampled both engine roots at query
entry. The `DAT_0056af40` value was `0x05f26c84` and began a different chain
whose first 32 records were mostly model kind 4; `DAT_0056af44` was null. The
winning `q+0x68` value was `0x05f2e844`, outside that root chain, and matched
the per-cell candidate-list object chain above. This runtime separation keeps
the two linked-list owners distinct in the native reconstruction.

The first hit also exposed model data at `0x05db86b4`, through the kind-6
table at `0x05da6d18`, with 14 vertices, 6 normals, 6 faces and normal
`[1, -3867, -1351]`. The pointer's `+0x1a/+0x1f` fields agree with the query
result's model index/kind on all eight hits. This directly confirms that
`q+0x68` is the linked collision object record consumed by the normal
finalizer, not merely an unrelated model-data pointer. The runtime addresses
are allocation-specific; the offsets and field agreement are the evidence.

A separate bounded linked-cull capture confirmed the negative dynamic result
at the current Hangar start position. Forty calls entered `0x004f43e0` from
`0x0046297e` with the same root `DAT_0056af40 = 0x05f26c84`; the first 32
nodes of each walk were readable, and the cull return was followed by the
outer `0x004628f0` pass. In the first capture all sampled nodes had flags
`0x110` or `0x111`, which pass the linked-object flag gate in
`0x004f43e0`; after culling, every sampled node had its `+0x06` query stamp
set to the current stamp, leaving zero face-test survivors. A later movement
sample saw additional flag values (`0x130`, `0x8110`, `0x8130`, `0x8171`),
but still left zero survivors in the bounded prefix. The companion
`0x00463e50` probe recorded no calls in this run. This is useful negative
evidence: the missing dynamic-face events are explained by the linked
object-space broad phase rejecting the sampled objects, not by the probe
missing the mode-1 query path. It does not claim that every node beyond the
bounded prefix was absent or rejected.

The follow-up `collision-cull-scale1` capture made three bounded cull calls
after Hangar loaded. All 32 readable prefix nodes in each call reported
`matrix_scale_q12 = [4096, 4096, 4096]`; the only sampled flags were `0x110`
and `0x111`, and each call returned zero face-test survivors. This confirms the
live tail values are Q12 identity for this ordinary level-object chain, while
leaving the non-identity `0x0200` asset case unobserved.

The separate `collision-transform-followup` capture closes the identity case
for the full-word `0x0200` transform branch. It recorded 500 completed calls
to `0x004f5540`, all from return site `0x00461c64`, with the live player object
`0x05f39530` as the object argument. Every object snapshot had flags
`0x0202`, tail words `+0x28/+0x2a/+0x2c = [4096, 4096, 4096]`, and
`+0x30 = 0`; the one transient `+0x24` zero was otherwise `0x00403800`.
The temporary matrix varied across the run with player orientation, including
flat and ramp-like bases, but the before/after matrices matched for every
sample. This is runtime confirmation that the high-byte `0x02` branch consumes
the three signed Q12 scale words and leaves the matrix unchanged for identity
factors. It does not establish the loader values for a non-identity object.

The controlled `collision-dynamic-positive5` capture then changed only the
first live linked node during one collision call, restoring every changed
prefix field when `0x00463e50` returned. It supplied the known model-171
Hangar origin `[-4100096, -6782976, 9408512]`, zero angles, model index 171,
model kind 6, and flags `0x0110`. Offline parsing of the same `SKHAN.PSX`
scene identifies this as source object index `170` (the source object uses
model `171` at that exact position). The query was the same vertical line used by
the static and airborne captures:

```text
start = [-4100096, -8822784, 11472896]
end   = [-4100096,  23945216, 11472896]
```

The cull marked the node as a face-test survivor. The dynamic query then
entered `0x00463e50`, and its two lower calls were observed with these live
argument roles:

```text
0x004f4b00(model_data, transformed_vertices, 8000, transformed_origin)
0x004f4c50(model_data, transformed_vertices, query, linked_object)
```

The transformed-vertex pass returned a zero `0x60f` clip mask, allowing the
face walker to run. It published `q+0x68 = linked_object`,
`q+0x80 = 0x05db87e4`, `q+0x84 = 171`, and `q+0x40 = 29`. The dynamic contact
was `[-4100096, -8710784, 11472896]`; `q+0x8c` remained its initialized
`0x7fffffff` sentinel because this path orders candidates by traveled distance
and reconstructs contact from `q+0x40`. The native asset replay resolves this
dynamic candidate as model face 5, with surface/normal word `0x00100028` and
finalized normal `[1, -4093, -160]`; the static replay selects its own nearest
face and `0x4000` parameter independently. This is a positive end-to-end
runtime confirmation of the linked dynamic primitive and its PSX source
provenance; the temporary field
mutation is why it is labeled controlled rather than a claim about ordinary
gameplay objects at every position.

The executable's direct cull disassembly sharpens the flag interpretation:
both `0x004f43e0` (linked objects) and `0x004f4940` (static candidates) test
only the low byte. They reject low-byte bit `0x20` and reject the case where
low-byte bits `0x01` and `0x40` are both set; high-byte bits are not admission
rejections. The transform code at `0x00463e50` tests high-byte bit `0x02`,
which is full-word `0x0200`, while nearby object appearance setters write
high-byte bit `0x04` (`0x0400`). Native code must preserve that distinction.

The face-flag consumer was also observed directly. The initial 32-call
capture at `0x0048ea80` contained eight non-null hit bodies, all from the
ground caller `0x00496fe1`. A broader `collision-materials1` capture collected
500 shared queries while the skater moved and ollied in Hangar: 160 queries
hit geometry and 121 flag-consumer calls had non-null faces. It showed these
raw/derived combinations:

| face base | surface flags | derived result |
|---:|---:|---|
| `0x1083` | `0x0010` | `face_bit_80=128`, inverse bits `1/1`, class `0` |
| `0x1083`, `0x1883`, `0x30a3`, `0x38a3` | `0x0000` | `face_bit_80=128`, inverse bits `1/1`, class `0` |
| `0x18a3`, `0x38a3` | `0x0400` | `face_bit_80=128`, inverse bits `1/1`, class `2` |
| `0x18a3` | `0x0420` | `face_bit_80=128`, inverse bits `1/1`, class `2` |
| `0x0823` | `0x0440` | `face_bit_80=0`, inverse bits `1/1`, `surface_bit_40=64`, class `2` |

The winning face pointers matched `q+0x80`. Calls with a null hit body left
these globals unchanged. This confirms that the bit extraction is used across
floor, sloped/large-polygon, and a distinct `surface_bit_40` class, while the
higher-level material names remain unresolved.

## Interpretation

The credible reconstruction target is therefore:

```text
bool-like hit state in q+0x68
    ├── contact candidate q+0x6c..0x74
    ├── normal candidate q+0x78..0x7c (signed shorts, ~4096 magnitude)
    ├── face/model metadata q+0x80 and q+0x84
    ├── nearest segment parameter q+0x8c (0..0x4000)
    ├── traveled distance q+0x40 (same line-length units as q+0x44)
    └── terrain/material flag candidates from face+0x0c and face word 0
```

This supports a later conceptual API such as
`QueryCollision(start, end, result)`, but the actual build uses a caller-owned
query record, a mode argument, fixed-point positions, and global level/cache
state. The wrapper's `EAX` return must not be modeled as the hit boolean.

## C++ reconstruction boundary

The compile-only reference at
`re/evidence/collision_reference.hpp` implements the evidence-backed portion
of that API without fabricating the unresolved zone/model format. It provides:

- x86-compatible arithmetic-shift and signed-truncation helpers;
- the explicit `0x90`-byte query-record ABI view, conversion to/from the
  semantic `QueryRecord`, and query preparation of bounds, integer line
  length, hit sentinels, and the recovered Q12 basis;
- little-endian model-data and variable-length face views, including vertex
  and normal lookup, face stride/index decoding, raw scene-mask filtering,
  cache-compatible face-AABB generation, and a reusable `query_model_faces`
  primitive;
- zone bounds overlap, candidate-table indexing, and the integer 2-D grid
  DDA used to visit candidate lists;
- model-local plane gating and the recovered triangle/quad oriented triple
  products from the x87 face predicate;
- the `0x4000` segment parameter, interpolated contact, and traveled-distance
  calculations;
- nearest-candidate record updates after a caller-provided triangle predicate;
- raw face-word flag decoding using the exact expressions from `0x0048ea80`;
- the `0x004f4b00` dynamic transformed-vertex stream, six-bit clip mask, Q12
  object-normal helpers, and the signed-short saturation used by
  `0x004e2070`;
- the projected dynamic-face winding gate, candidate distance, nearest-hit
  update, and the dynamic contact fallback arithmetic.
  The native scene result also preserves the dynamic `0x20000` query-mask
  sideband as an optional model selector instead of turning it into a hit.
- the recovered linked-object prefix view, model-bound expansion/reflection,
  the exact linked-object flag gate, and the complete short-arithmetic
  object broad-phase prefilter.

The asset-facing native layer at
[`src/collision/psx_scene.hpp`](../../src/collision/psx_scene.hpp) adds the
version-4 PSX model/object/blockmap decoder, the recovered line/grid walk over
the blockmap's spatial cells, the static scene query, stable scene handles,
raw base/surface metadata, and direct wrappers for the dynamic
transformed-vertex preprocessing and projected-face candidate arithmetic. It
deliberately does not claim that the PC heap linked-list serialization or its
broad-phase object records have been reproduced.

The asset-facing API also exposes `PsxLinkedCollisionObject` and
`PsxScene::query_linked_objects`. This is the native equivalent of the
collision-facing portion of `0x004628f0`: each supplied node is checked with
the exact `0x004f43e0` flag gate and short-arithmetic broad phase, then its
model is transformed and scanned by the recovered dynamic face path. The
method returns the nearest dynamic candidate by `q+0x40`, preserves the
caller-supplied body identity in `q+0x68`, and leaves the unresolved heap
loader/serialization boundary explicit. Its `model_index` is a caller-
resolved native scene index; the original PC `model_kind`/index pair still
selects one of the unresolved type-specific model tables.
When a node is constructed with
`PsxScene::linked_collision_object_from_source`, the native result preserves
the source PSX object index separately from the dynamic-span index. This is a
provenance handle for the reconstruction, not a claim that the retail
`q+0x68` pointer is an integer object index.
For records taking the full-word `0x0200` matrix-transform branch, the original
consumes signed Q12 tail words at `+0x28/+0x2a/+0x2c` while constructing the
temporary matrix. The native view accepts those factors, but the PC loader
values and complete tail ownership remain unresolved.
`PsxScene::query_with_linked_objects` composes this dynamic result with the
PSX zone/blockmap result at the recovered native boundary. It compares the
shared traveled-distance field `q+0x40` and keeps the static candidate on an
exact tie, reflecting the executable's dynamic-before-static traversal and
strict-nearer updates.

The runtime integration keeps this linked-node boundary explicit. An opt-in
`GameplaySessionConfig::use_recovered_collision_scene` selects
`PsxScenePositionCollisionProbe`, and
`GameplaySession::set_recovered_linked_collision_objects()` supplies the
caller-resolved node records used by the aggregate static/dynamic query. It
does not derive PC heap pointers, model-kind lookup, or matrix-tail values
from the trigger scene registry.

`collision_reference_test.cpp` compiles with C++20 and checks the captured
airborne hit (`line_length = 71`, `t = 2101`, distance `9`, and the exact
contact point), the raw query layout, both line-basis branches, synthetic
model-face traversal, candidate filtering, zone indexing, a horizontal grid
walk, the linked-object prefix and broad-phase path, and the dynamic
projected-face candidate/fallback path. This is a
reference query layer rather than a complete level-file loader: the remaining
engine-specific work is wiring the PC linked-object loader/cache and its
level-to-heap ownership into these interfaces. The collision-facing node
prefix and object broad-phase records are now available. The Q12 object-angle
basis used by normal finalization is covered by `build_object_rotation_basis`.

## Open questions / falsifiers

- Confirm integer-division edge behavior at endpoint and coplanar hits; the
  non-degenerate path is already identified as a `0x4000`-scaled segment
  parameter.
- Confirm whether the contact candidate is exactly the interpolated segment
  point or a post-normalized/padded point in a branch with nonzero penetration.
- The bounded airborne run confirms the first in-air sweep and its hit result;
  distinguish the later in-air sweeps (ground versus wall/rail) with a longer
  run that deliberately holds a jump and moves across multiple surfaces. A
  linked-object cull and a positive transformed dynamic linked-face hit are
  now confirmed. A natural, non-injected gameplay capture through a moving
  prop remains useful but is not required to identify the primitive.
- Resolve the face-record flag bits before assigning material names, and map
  the PC globals onto the original `COLRESULT_*` concepts where possible.
- Map the complete caller-specific stack protocol around the two loader
  callsites if a drop-in PC loader is required; the collision-facing source
  prefix and table handoff are now runtime-confirmed.
- Capture a live non-identity `0x0200` object through `0x004f5540` and compare
  its three loader-provided factors and matrix before/after the helper; the
  identity case is now confirmed against the exact matrix-column arithmetic.
- Keep the unresolved tail of each 0x660-byte zone record and the allocator
  interface provisional; the runtime loader experiment now ties the active
  zone/table globals to a specific serialized-zone buffer.
