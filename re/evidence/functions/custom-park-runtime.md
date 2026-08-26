# Custom park PRK disk and runtime path

Status: confirmed versioned PRK decode into level-generation state, generated
piece/gap objects, and the shared runtime PSX object/model path
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x00432c60`, `0x00433280`, `0x004397d0`, `0x0043a280`,
`0x0043c050`, `0x0043c480`, `0x0043b410`, `0x0043b8d0`, `0x0043d400`,
`0x00440190`, `0x00444380`, `0x004667e0`

Custom parks are a distinct asset family from Warehouse's PRE/WAD scene, but
they converge on the same runtime level-region tables and object stride.
The extracted corpus contains 50 `PARK%d.PRK` files. For example,
`PARK0.PRK` is 2560 bytes with SHA-256
`5b9cc29a7dc029d79a5e34e400538ca9d5d8b0f3ab87319fd4339b1f66152b1f`; its
header is `0x4e25`, version `0`, map variant `1`.

## Disk format

The parser at `0x0043c050` accepts the current magic `0x4e25` and the older
`0x4e24` format. The first 12 bytes are:

```text
+0x00 u32 magic       0x00004e25 (current), 0x00004e24 (old)
+0x04 u32 version     version/table selector
+0x08 u32 map_variant  copied to the level-generation output
```

The version selects the packed grid dimensions from executable tables:

```text
version 0 -> 16 x 16 cells
version 1 -> 24 x 24 cells
version 2 -> 30 x 30 cells
version 3 -> 30 x 18 cells
version 4 -> 60 x  6 cells
```

Each cell is eight bytes on disk. The first five bytes are compact item or
tile references; byte five carries five 2-bit values; the final 16-bit word
contains the remaining packed per-cell values. `0x0043c050` expands each
cell into a 16-byte runtime cell: five translated `u16` references followed
by the unpacked flag/value bytes. Reference bytes other than `0xff` are
translated through `0x00444380`; `0xffff` remains the empty reference.

After the grid, the parser expands ten fixed-size park item records from
36-byte disk records into 0x2c-byte level-generation records. It copies the
two position/type words, unpacks the flag fields, copies the item byte, and
copies the bounded item name into the runtime record. A 0x40-byte trailing
table is copied to `DAT_0056a1fc`. The exact semantic names of every editor
item field are not assigned here; the packed-to-runtime widths and offsets
are directly visible in the parser.

For the current `0x4e25` load branch, the operational item-record mapping is:

```text
disk +0x00..+0x05  -> runtime +0x00..+0x05      two endpoint/index groups
disk +0x06 u16     -> runtime +0x08..+0x0b      four nibbles, low-to-high
disk +0x08 u16     -> runtime +0x06,+0x07,+0x0c 2-bit, 2-bit, remaining byte
disk +0x0a         -> runtime +0x28             item byte (current 0x4e25 load)
disk +0x0b..       -> runtime +0x0d             NUL-terminated visible name (current load)
```

There is a one-byte compatibility quirk worth preserving. The older `0x4e24`
load branch reads the item byte at disk `+0x09` and starts the name at `+0x0a`;
the current `0x4e25` load branch reads the item byte at `+0x0a` and starts the
name at `+0x0b`. The `0x4e25` serializer also writes the item byte at `+0x0a`
and the name at `+0x0b`, so a faithful recreation should retain these
observed offsets rather than normalize them into one abstract record. In the
`PARK0` witness, `+0x0a = 0x06` is the current-load/serialized item byte and
the visible name begins at `+0x0b`.

The split is literal: the parser extracts four 4-bit values from the
`+0x06` word, extracts two 2-bit values from the low four bits of the `+0x08`
word, and preserves the shifted remainder as the runtime byte at `+0x0c`.
This explains why the expanded record does not retain either source word as a
single integer. The two endpoint/index groups remain six independent bytes;
the generator later reads their low/high endpoint bytes as the three-index
tuple for each side.

The generator's use of that tuple is independently visible. For each side it
reads runtime bytes `+0/+2/+4` (or `+1/+3/+5` for the second side), validates
the first and third against the active grid dimensions, and indexes
`cell_model_table[third][second + first*5]`. The diagnostics identify the
first index as X and the third as Z; the middle index is the table's five-wide
subindex. Thus the record can be recreated as two endpoint index triplets,
without treating the six bytes as a packed coordinate integer.

This is a width/offset correspondence, not a semantic claim for every editor
label. A concrete `PARK0.PRK` example starts its ten-record table at file
`+0x80c`; item 0 begins:

```text
02 0a 04 04 0e 0e 30 03 78 01 06
59 4f 55 27 52 45 20 41 4c 4c 20 4f 56 45 52 20 54 48 45 20 4d 41 4c 4c 00
```

The parser turns its first six bytes into three two-byte coordinate/index
columns: each column contains the value for endpoint 0 followed by endpoint 1.
The gap generator consumes the low/high byte of each column as one endpoint's
three-index tuple, resolves that tuple through the generated cell-model table,
and preserves the name `YOU'RE ALL OVER THE MALL` in the runtime item record.
The remaining packed bytes are endpoint-specific grid metadata; their exact
editor labels remain intentionally unassigned.

The serializer at `0x0043c480` is the inverse witness. It writes the same
magic/version/map-variant header, repacks the runtime cells and ten item
records, appends the 0x40-byte table, and allocates:

```text
((cell_count * 8) + 0x22f) & ~0x7f
```

This predicts the observed corpus sizes, including 2560 bytes for version 0,
5120 for version 1, 7680 for version 2, 4864 for version 3, and 3328 for
version 4.

## Runtime load and convergence

The custom-park state machine at `0x00432c60` selects a park index, constructs
`park%d.prk`, opens/reads it through the common game file layer, and reports
the loaded park name through `0x00432980`. Its state transitions are consumed
by the level-generation loop. `0x00433280` allocates a 0x1e00 temporary
buffer, invokes `0x0043c050` for the packed PRK decode when a saved park
buffer is pending, and passes the decoded version/variant to `0x0043a280`.

`0x0043a280` selects the level-generation version tables, sets the active
map/variant globals, rebuilds the level-generation model state, and calls
`0x0043c6b0`. The subsequent `0x0043d400` finalization is the important
runtime bridge: it allocates a new PSX slot, builds the model-pointer table,
creates count-prefixed `0x4c` object records, and publishes them through the
same globals used by the Warehouse scene path:

```text
PARK%d.PRK
  -> 0x00432c60 file read
  -> 0x00433280 / 0x0043c050 packed-grid and item decode
  -> 0x0043a280 level-generation rebuild
  -> 0x0043d400 PSX-slot/model/object finalization
  -> DAT_0056d438[slot*0x11] object records
  -> DAT_0056d43c[slot*0x11] model pointers
  -> DAT_0056d440[slot*0x11] raw region
  -> DAT_0056db28 attached-environment consumer
```

The object records created here use the already-proven `0x4c` stride and the
same `+0x1a` model index, `+0x1f` slot, and `+0x20` list-link fields. The
finalizer calls `0x004667e0`, which is also the Warehouse blockmap/zone
consumer boundary. That establishes convergence at runtime rather than
assuming that custom-park editor records are themselves render objects.

## Generated-object correspondence

The level-generation bridge between the decoded grid/item records and the
published PSX object array is explicit in `0x0043b410`, `0x0043b8d0`, and
`0x0043d400`:

```text
translated PRK cell reference
  -> 0x0043b410 allocates a zeroed 0x238-byte generated piece object
  -> 0x00440190 attaches the source model descriptor at object +0x224
  -> object +0x21c/+0x21e/+0x220 receive generated short-grid coordinates
  -> object +0x230 links generated pieces into the level-generation list
  -> 0x0043d400 allocates the published 0x4c runtime object record
  -> runtime position = generated coordinates << 12
  -> runtime +0x1a = source model-index byte from object +0x224 + 8
  -> source object +0x22c = pointer to the published runtime record
```

`0x00440190` also gives a stable generated-piece prefix contract: it copies
three source-model dimensions to `+0x10/+0x12/+0x14`, clears the intermediate
state beginning at `+0x18`, stores the source model's real-height-cell byte at
`+0x0b`, and initializes the generated member-list/state links before the
piece is placed in the level-generation chain. The later generator writes
the cell position at `+0x21c/+0x21e/+0x220`, while `+0x224` remains the source
descriptor and `+0x22c` becomes the reverse link to the published 0x4c object.

The item records have a parallel consumer. `0x0043b8d0` reads each runtime
item's two endpoint/index groups, resolves each endpoint through the generated
cell-model table, allocates a zeroed 0x58-byte gap-member record, stores the
source item ordinal at `+0x1c`, and links the two members through `+0x08`.
When the selected gap list is finalized, `0x0043d400` writes each gap record's
computed X/Z coordinates into another 0x4c runtime object and stores the new
runtime index back at gap record `+0x0c`. This proves that PRK items influence
runtime objects through generated gap/model records; they are not themselves
the final PSX object array.

For `PARK0` item 0, the two endpoint groups are `(2,4,14)` and `(10,4,14)`.
Those values are sufficient to reproduce the two generated gap members and
follow their later source-record back-pointers, even though the editor's
user-facing label is not treated as a model/class name.

The two members are created in endpoint order. The first receives
`+0x00 = 1` and the second `+0x00 = 0` in the observed constructor path;
each member's `+0x08` points to its partner, while the list chain uses
`+0x0c`. Disabled item marker `0x81` clears member `+0x38` and publishes the
last such member through the level-generation disabled-item state; ordinary
items set `+0x38` active. This separates endpoint pairing from list order and
from the editor marker byte.

The gap-member structure is now field-supported beyond the pair link. The
constructor `0x004410b0` clears the record and initializes `+0x38` to 1 and
`+0x39` to 0. `0x0043b8d0` then stores the selected source gap model at `+0x04`,
the partner at `+0x08`, the next member at `+0x0c`, and the source PRK item
ordinal at `+0x1c`. `0x00441630` copies the item marker into `+0x38` (normal
items are active; the `0x81` disabled marker clears the active path), and
`0x00441240` writes the fixed words `0x864`, `0x3c8`, `0x1c`, `0x48`, and
`0x31` at `+0x3c..+0x4c`.

`0x004410e0` is the concrete geometry consumer. It receives the item
orientation and two packed endpoint values, stores the orientation at `+0x50`
and the derived local offset at `+0x54`, then computes four orientation-
dependent edge coordinates at `+0x20/+0x24/+0x28/+0x2c` and two extents at
`+0x30/+0x34` from the source model's grid position and dimensions. The
formulas are fixed-point cell-size arithmetic, so a recreation can reproduce
the gap bounds without assigning a user-facing editor meaning to each byte.

During `0x0043d400`, each gap member receives the final published 0x4c object
pointer at `+0x14`; the generated object is written back to the source model
at its reverse-link field. This closes the item-side path:

```text
PRK endpoint tuple + item marker
  -> generated cell/model lookup
  -> paired RuntimeGeneratedGapMember records
  -> edge coordinates/extents and fixed constants
  -> published RuntimePsxObjectRecord
  -> gap member +0x14 back-pointer
```

## Limits

- Confirmed: magic/version/map-variant header, five supported version grid
  dimensions, eight-byte packed cells, 16-byte expanded cells, ten 36-byte
  item records, the nibble/2-bit item unpacking and name/item-byte mapping,
  serializer size formula, common file read, PSX-slot creation,
  object/model table publication, and zone-consumer convergence.
- Observed: the 0x1e00 temporary buffer used by the asynchronous/state-machine
  path and the 0x40-byte trailing table copy.
- Still open: semantic names for all custom-park item fields and the final
  post-sort runtime index for every editor item. The disk endpoint -> generated
  gap/model record -> published 0x4c object correspondence is now established;
  the remaining uncertainty is editor semantics and ordering, not the loader
  boundary.

## Native recreation boundary

`src/assets/custom_park_asset.*` implements the bounded `0x4e25`/`0x4e24`
reader. It preserves packed 8-byte cells, expands the five compact references
and packed value bytes, keeps the retail reference-translation table as an
explicit input, and materializes the ten 0x24-byte disk items into the proven
0x2c-byte runtime view. The current/legacy item-byte and name offsets remain
distinct. `custom_park_runtime.*` then owns the expanded 0x10/0x2c generation
images and the finalizer seam: caller-supplied generated placements publish
the proven 0x4c object records at Q12 positions, and paired generated gaps
retain the observed 0x58 member links, source-item ordinal, active byte, and
published-object back-indices. The unresolved source-model translation table
and platform pointer allocation remain explicit inputs. Native tests cover
the real parser boundary plus a synthetic PRK-to-generation-to-published-
object/gap trace.
