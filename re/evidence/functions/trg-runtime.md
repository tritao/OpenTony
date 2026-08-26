# Warehouse TRG node loading and runtime object path

Status: confirmed trigger-file parse, object-manager dispatch, type-5 powerup-to-PSX item/medal model handoff, type-12/14 trick-object allocation/checksum handoff, all three type-1 constructor families selected by the PC executable, constructor field initialization, TRG link/command records, live disk-node to constructor/object correspondences, the type-1 script runner/field contract, traffic-object update/interaction consumers, and the common link-command cursor ABI; broader gameplay opcode semantics remain partial
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004c5130`, `0x004c8130`, `0x004c8650`, `0x004c5460`, `0x00403000`, `0x0049f250`, `0x00412640`, `0x004128a0`, `0x004128c0`, `0x00412960`, `0x00413000`, `0x004136e0`, `0x00480240`, `0x004802c0`, `0x00402bb0`, `0x00403410`, `0x004c84d0`, `0x004c5c00`, `0x004c5ac0`, `0x004c58b0`, `0x004c5b00`, `0x004c5b60`, `0x004c5c70`, `0x004c5d70`, `0x004c5dc0`, `0x004c7c50`, `0x004c8550`, `0x004c7a00`, `0x004b3270`, `0x004b32f0`, `0x00464710`, `0x004ca270`, `0x004ca2d0`

The TRG path is a parallel runtime family to the PSX scene-object path. It
creates gameplay/trigger objects and link records; its pointers must not be
treated as pointers to the 0x4c-stride PSX environment records.

## Warehouse trigger file

The normal Warehouse load opens `SKWARE_T.TRG` and the controlled run observed:

```text
size        10590
SHA-256     335a13d5041c473f0a2578ec97f16db78d6ea506646534e6cecfc6212b0a581d
magic       _TRG
version     2
node count  313
```

The offline node histogram is:

```text
type 1   baddy          53
type 2   crate           45
type 3   point            2
type 4   autoexec         1
type 5   powerup         12
type 6   command_point   64
type 8   restart          9
type 10  rail_point      60
type 11  rail_def          5
type 12  trick_object    52
type 13  camera_point      9
type 255 terminator       1
```

The names above are the OpenTony offline names. For type 1, the next two
little-endian words in the node record are also useful runtime discriminators:
the 53 baddy nodes split into 12 subtype `0x00cb` records and 41 subtype
`0x0192` records. The runtime dispatch below is
the independent evidence for which node families participate in object
construction, link creation, restart setup, and command processing.

## Cross-level variant audit

The extracted corpus has 32 `.TRG` files, including the normal level files,
`SKHVN.TRG`, `SKHVN_T-OLD.TRG`, and `SKHVN.PSX_T.TRG`. All 32 use version 2;
the offline parser found strictly bounded node tables and no structural
failures. The node populations vary substantially—small rail-only/editor
variants have fewer than 100 nodes, while `SKHVN.PSX_T.TRG` has 1536—but the
same header and offset-table format holds.

The executable's first-pass dispatcher is narrower than the offline type
enumeration. `0x004c8130` only enters its main type switch for values below
`0x1f5` (501), and it has no action case for type 15. Corpus-only/high-number
families therefore have an explicit runtime boundary:

```text
type 15   autoexec2     present in several files, no first-pass action case
type 501  off_light     excluded by the type < 0x1f5 guard
type 1000 script_point  excluded by the type < 0x1f5 guard
```

Types 14 and 500 do have executable branches: type 14 shares the small
trick/goal-object path, while type 500 is decoded with `0x004c8650` and
initialized through `0x0045c390` into a 0x58-byte record. This cross-corpus
check prevents treating every offline node name as a constructed PC object.

## File load and relocation

`0x004c5130` is identified by the embedded source path as `trig.cpp`. It:

1. formats the level-specific `_T.trg` path;
2. opens it through `0x00449030`, allocates through `0x0046f490`, and loads it
   through `0x00449230`/`0x00449660`;
3. checks `_TRG`, version 2, and a zero high half-word in the version field;
4. reads the node count from the header;
5. stores the relocated node-offset table in `DAT_0056e210` and its count in
   `DAT_0056e214`; and
6. frees the input buffer after deriving the final terminator-aligned pointer.

The node-offset entries are absolute runtime pointers after relocation. All
later trigger functions use `DAT_0056e210[index]`, so an offline node index is
stable across the disk/runtime boundary even though its pointer is heap-local.

### Trigger-driven PSX resource selection

The trigger command interpreter is also the level-scene resource selector.
Its resource opcodes have an exact lower-level contract:

```text
0x7e <NUL-terminated name> -> PSX_QueueResource(name, mode=0, ...)
0x80 <NUL-terminated name> -> PSX_QueueResource(name, mode=1, ...)
0x81                     -> PSX_Spooler drain
```

Mode 0 parses a raw auxiliary PSX region through `0x004b2450` without the
environment-list attachment step. Mode 1 runs the same parser and then calls
`0x004b2ac0`, which finalizes and links the region into the attached
environment list. `0x004c8050` chooses type-4 `AUTOEXEC` for the ordinary
level path and type-15 `AUTOEXEC2` when the two-player path is active.

The Warehouse file itself provides a direct disk-to-command witness: its
type-4 node 0 contains `0x7e SkWare_L` and `0x7e SkWare_O`, while type-8
restart command streams contain `0x80 SkWare` and `0x80 SkWare_2`. Thus the
auxiliary `SkWare_L.psx`/`SkWare_O.psx` parses and the attached main
`SkWare.psx`/alternate `SkWare_2.psx` paths are selected by trigger bytecode,
not by a filename convention inferred from the offline archive. The complete
13-level matrix and the normal/two-player auxiliary spellings are recorded in
[level-load.md](level-load.md).

## Node dispatch

`0x004c8130`, also in `trig.cpp`, walks the relocated node table. The first
pass has the following observed dispatch:

| TRG type | Runtime action |
| --- | --- |
| 1 baddy | tests a node flag and calls `0x004c5460` to construct a gameplay object |
| 2 crate, 9 (runtime-only/variant) | creates an 0x18-byte link record through `0x004c84d0` |
| 5 powerup | reads the decoded node position/state and conditionally calls the object factory, then `0x004a8e50` for powerup setup |
| 6 command point | creates an 0x18-byte link record through `0x004c84d0` |
| 7 restart variant | tests the same object-creation flag path as type 1; the generic parser otherwise rejects it as a seed path |
| 8 restart | collects decoded restart positions into `DAT_0056dcb8[]`, with a maximum of 40 |
| 12 trick object, 14 (if present) | allocates a small record and calls `0x004bd760` |
| 13 | calls the node-specific setup at `0x00411f30` |
| 500 | allocates a 0x58-byte record and initializes it through `0x0045c390` |

The type-12/14 allocation, model-checksum correspondence, record fields, and
player/rail/update consumers are detailed in
[trick-object-runtime.md](trick-object-runtime.md). It is a separate runtime
record family from both the 0x4c-stride PSX environment objects and the larger
TRG gameplay-object families.

The type 2/6 link records have this observed minimal layout. The names below
follow the constructor's register-level argument flow, not an assumed public
class definition:

```text
+0x00  command/diagnostic payload pointer supplied by the dispatcher
+0x04  state byte, initialized to 0
+0x05  state byte, initialized to 0
+0x06  initial-pulse byte, initialized to 0
+0x07  pulse count/state byte, initialized to 0
+0x08  state word, initialized to 0
+0x0a  source TRG node index
+0x0c  target value supplied by the node
+0x10  next link in the global list
+0x14  next link bucketed by low 8 bits of +0x0c
```

`0x004c84d0` allocates this record, pushes it on the global list headed by
`DAT_0056e220`, and inserts it into `DAT_0056dd64[target & 0xff]`. The global
accounting word `DAT_0056e224` advances by `0x1c` per record even though the
allocation request is `0x18`; that accounting unit is retained as an observed
runtime detail rather than being interpreted as a C++ size.

The node payloads explain the two call forms in `0x004c8130`:

```text
type 2 node:
  u16 type = 2
  u16 variable_prefix_words = 0 in Warehouse examples
  aligned u32 target value/hash
  record +0x00 = static `DAT_005477f0` diagnostic string

type 6 node:
  u16 type = 6
  u16 variable_prefix_words
  variable prefix words
  aligned u32 target value
  trailing u16 command stream, terminated by 0xffff
  record +0x00 = pointer to the trailing command stream
```

For example, Warehouse command-point node 18 at file offset `0x774` is:
`type=6`, prefix length `1`, target `0x947969a2`, followed by command words
`0x000c, 0xffff`. The larger node 1 at `0x540` ends with target `0` and
command words `0x0086, 0x0001, 0x0003, 0xffff`. These are direct disk
examples of the target/command split; the meaning of the variable prefix
words remains open.

## Position decoding

`0x004c8650` is the shared node-position decoder used by the TRG dispatch. It
resolves the node pointer through `DAT_0056e210`, selects the position payload
after the node's variable-length prefix, and writes three 32-bit coordinates
to the caller's temporary vector. Each coordinate is shifted left by 12 before
being returned. The type-5 dispatch calls this decoder before it invokes the
object factory.

For node 17, the decoder therefore turns the disk words
`(7673, -427, 12132)` into `(0x01df9000, 0xffe55000, 0x02f64000)`, which is
also the position stored in the constructed runtime object. This is the
runtime-side reason the node/object comparison above uses fixed-point values,
not the raw 16-bit disk units.

## Object factory and node identity

`0x004c5460` is the trigger object factory. It validates the supplied node
index, reads the node's object subtype, and chooses a size/constructor family:

```text
object subtype 0xcb       -> allocation 500 (0x1f4), constructor 0x00403000
object subtype 0xd5..0xdc  -> allocation 0x1e8, constructor 0x00412640
object subtype 0x192       -> allocation 0x218, constructor 0x0049f250
```

After construction it writes the originating TRG node index at object `+0xb0`.
It also updates object flags at `+0x6a` and inserts the object into the
appropriate game-object list, including `DAT_0056af40` for the list selected
by the constructor. A controlled Warehouse run observed a game-owned pointer
for a trigger object created from node index 17 (`0x005f404c0` in that run).

That pointer is intentionally kept separate from the PSX scene object 17
pointer (`0x005f34f6c` in the corresponding load run): equal numeric indices
do not imply equal object families.

The two type-1 constructor families are distinguishable before any gameplay
consumer runs:

```text
TRG type 1 / subtype 0x00cb
  allocation: 0x1f4 bytes
  constructor: 0x00403000
  vtable word: object +0x00 = 0x005183b0
  object type: object +0x3c = 0x00cb
  source node: object +0xb0 = TRG node index
  script cursor: object +0x17c = post-parameter command-stream pointer
  base flags: constructor sets object +0x04 bit 0x41

TRG type 1 / subtype 0x0192
  allocation: 0x218 bytes
  constructor: 0x0049f250
  vtable word: object +0x00 = 0x005194f8
  object type: object +0x1f2 = 0x0192
  source node: object +0xb0 = TRG node index
  script cursor: object +0x17c = post-parameter command-stream pointer
  base flags: constructor preserves the common object header and sets
              object +0x04 bits 0x111 after clearing bit 1
```

The `0x0192` constructor's collision-facing inputs are now more explicit.
`0x0049f250` passes the current resource-name pointer from `DAT_0056e218`
through `0x0047fe30`; that helper resolves the name against the 20-entry
region table, writes the region selector to object `+0x1f`, and initializes
the model index at `+0x1a` to zero. The constructor then calls
`0x00480240` with the factory's position payload and `0x004802c0` with the
three-u16 orientation payload, storing them at `+0x08..+0x10` and
`+0x14..+0x18`. It writes `+0x04 = (+0x04 & ~0x0002) | 0x0111`, so a later
factory clear of bit `0x2` leaves the observed `0x0110` form without changing
the model/position inputs. The originating TRG node index is written at
`+0xb0` by the factory after the constructor returns. This connects the
trigger-side allocation to the same region/model/position/angle prefix read
by the dynamic collision path, while leaving the live heap body pointer and
the later model-index update explicit.

### Type-192 model-selection lifecycle

The type-192 vtable is installed at `0x005194f8`. Its virtual handler at
`0x004a1060` dispatches a compact object command by the low 16 bits of the
command word. The model-selection cases are important to collision ownership:

- one case consumes a `u16` model index, stores it at object `+0x1a`, then
  resolves `DAT_0056d43c[object+0x1f * 0x11]` and tests the selected model
  header byte for bit `0x10`; it sets or clears object flag `+0x04` bit
  `0x20` accordingly;
- another case aligns the command cursor to a four-byte boundary, consumes a
  `u32` model-name/checksum, calls `0x004b1de0(checksum, object+0x1f)`, and
  stores the returned model index at `+0x1a`; it performs the same model-header
  test and also clears object flag `+0x04` bit 0; and
- the constructor initializes `+0x1a` to zero through the shared
  `0x0047fe30` helper, while `+0x1f` is the region slot returned by the
  region-name lookup. Thus the initial model is region-local index zero, not
  a TRG subtype or source-object index.

The handler's cursor is the post-position/orientation stream pointer stored at
`+0x17c`; the constructor's `0x00480240`/`0x004802c0` helpers consume the
three fixed-point position words and three orientation words before saving
that cursor. In the type-192 dispatch table, the proven command IDs are:

```text
0x2123  consume one u8 -> object +0x16e
0x2124  consume one u16 model index -> object +0x1a; mirror model-header bit 0x10 to flags bit 0x20
0x2127  consume three u16 values -> object +0x70/+0x72/+0x74
0x2128  consume three resolved u16 values -> object +0x4c/+0x50/+0x54 after << 12
0x212f  align to 4, consume one u32 checksum -> 0x004b1de0(checksum, region); store returned model index at +0x1a
0x2133  consume three u16 values -> object +0x14/+0x16/+0x18
0x2136  consume three u16 values -> object +0x76/+0x78/+0x7a
```

The checksum case also clears object flag bit `0x1`, while both model cases
resolve the selected region-local model through
`DAT_0056d43c[object+0x1f * 0x11]`. The native TRG layer now records the
relative offset of this saved cursor beside the original node bytes; it does
not interpret the remaining stream as fixed records. This gives a concrete
update path from a TRG-created object to the same region/model table consumed
by collision broad phase, while leaving the command handler's heap pointer
ownership unresolved.

The boundary is also observable on shipped data with the native inspector:

```text
opentony_trg_inspect SKHAN_T.TRG --print-factory-cursors
factory_cursor node=7 subtype=0x192 relative=30 remaining=154
```

The remaining byte count is node-specific; the stable result is the relative
cursor location. It is obtained from the same absolute-alignment rule used by
the retail constructor, so it remains meaningful even when the node’s
file-table offset changes.

The live Hangar probe `tony-trg-type192-probe 128` confirms the command-table
interpretation on the normal object-update path. After dismissing the level
summary, it recorded 90 type-192 runner entries and 38 completed calls to
`0x004a1060`; all 38 handler calls were raw command `0x212f`. The calls covered
38 distinct region-4 objects. Their model indices changed from constructor
default `0` to region-local values `7`, `9`, or `10` (with one each of `1`,
`2`, `3`, and `4`), and their flags changed from `0x0110/0x0111` to
`0x0110/0x0130` according to the selected model header bit. The handler
returned through `0x004a114e` or `0x004a1169`, exactly the two model-header
branches, and advanced the saved cursor by 4 bytes when already aligned or 6
bytes when it first skipped two bytes to reach the aligned checksum word.
This is a runtime confirmation of `0x212f` as checksum/model selection and of
the cursor being a live command-stream pointer. The raw trace is retained as
`build/debug/trg-type192-hangar3.trace.ndjson`; its footer is intentionally
incomplete because the debugger was stopped after the bounded capture.

The selector table does not provide a supported semantic mapping for `0x2137`:
its byte is `0x90`, which does not point at one of the handler entries. The
proven three-vector cases are therefore `0x2127`, `0x2128`, `0x2133`, and
`0x2136`; `0x2137` should remain unclaimed until a real stream exercises it.

The adjacent `0x004a12d0` consumer scans `DAT_0056af40` through each object's
`+0x20` link, requires object flag `0x0100` and object state bit
`+0x178:0x10`, rejects `+0x178:0x20`, resolves the object's region slot/model
index through `DAT_0056d43c`, and compares the selected model bounds against
the player's position/AABB. A qualifying object is passed to `0x0049f4c0`
with the player response vector. This is a ground/platform response consumer,
not the skater's line-query primitive, but it proves that the type-192
object-manager list is a live downstream collision/ground input.

The model-overlap gate is now exact enough to reproduce independently. For
each player axis, `0x004a12d0` takes the ordered pair of the live position
(`+0x08/+0x0c/+0x10`) and the corresponding player extent (`+0xbc/+0xc0/+0xc4`),
then expands both ends by `0x14000` (20 Q12 world units). It adds the selected
model's signed-short bounds, shifted left by 12, to the object position and
rejects when any model minimum is above the expanded player maximum or any
model maximum is below the expanded player minimum. The selected model bounds
are read as runtime header pairs at `+0x0c/+0x0e` (X), `+0x10/+0x12` (Y), and
`+0x14/+0x16` (Z), with the endpoint order resolved by the comparisons rather
than by assuming a min/max serialization order. Before this test the function
sets object flag `+0x04` bit `0x20` for an admitted object; it then rejects
objects with state bit `+0x178:0x20` and calls `0x0049f4c0` only for the
survivors. This gives the future ground/platform adapter its exact broad phase
while keeping its object-manager record and platform type separate from the
PSX environment records.

The constructors also expose stable sentinel/header initialization beyond the
shared position and source-node fields. The `0xcb` constructor stores the
current runtime context from `DAT_0056a954` at `+0x1d8`, initializes `+0x1ec`
to `-1`, clears the word at `+0xae`, and marks `+0x16c` active. The `0x0192`
constructor stores the same context at `+0x1d8`, initializes `+0x208` to `-1`,
sets `+0x1a0` to `0x20`, and also marks `+0x16c` active. These are constructor
facts, not semantic class names; they give a faithful recreation a safe set of
initial values before the later object-manager consumer mutates them.

The remaining type-1 constructor is a distinct traffic/car family.
`0x00412640` allocates 0x1e8-byte objects for subtypes `0xd5..0xdc`, stores
the subtype at `+0x3c`, keeps a per-object sequence value at `+0x1e0`, and uses
the same aligned payload cursor for the common position and three-u16
parameter helpers. Its subtype-specific model-name and sound selection is
visible in the constructor's own switch:

```text
0xd5  c_taxi    sound id 0x071
0xd6  c_police  sound setup disabled by the constructor's active flag
0xd7  c_bus     sound id 0x121
0xd8  c_cable   sound id 0x086
0xd9  c_kart    sound id 0x111
0xda  c_mar     sound id 0x0dd
0xdb  c_bull    sound id 0x145
0xdc  c_gull    sound id -1
```

Each model name is first passed to `0x0047fe30(this, name)`, which resolves a
case-insensitive match through the 20-entry region-name table at
`DAT_0056d428`/`+0x44` stride, stores the selected PSX region slot at object
`+0x1f`, and resets the model index at `+0x1a`. The numeric ID is then passed
to `0x004ad7d0` as a positional sound request; its returned sound handle is
stored at `+0x1d0`, and `0x004ad950` is used for the corresponding sound update.
The constructor also
sets `+0x108=0x10000`, `+0x1ac=0x34`, `+0x1ae=0x20`, `+0xb4=0x3c`, and subtracts
`0x6e000` from the decoded Y position. These are constructor writes useful to
a faithful recreation, not semantic names for the fields.

The named model assets are present in the extracted PC data. `C_TAXI.PSX` is
89,532 bytes (6 objects/models, 10 inline texture names) and `C_BULL.PSX` is
also a complete named region (8 objects/models, 5 inline texture names); the
matching PSH manifests are present as well. The proven runtime flow is:

```text
C_TAXI.PSH/PSX or C_BULL.PSH/PSX
  -> named PSX spool/load path populates DAT_0056d428 region names
  -> 0x0047fe30 resolves c_taxi/c_bull to a loaded region slot
  -> traffic object +0x1f/+0x1a selects that region/model
  -> 0x004ad7d0 attaches the subtype sound handle
```

### Traffic update and downstream consumers

The traffic constructor installs vtable `0x005184e0`. Its update entry is the
function at `0x00412960` (vtable slot `+0x0c`), and the update is a real
runtime consumer of the object created from the TRG node. The first operation
uses `+0x1d4` and `+0x1d0` as a sound-update gate. When both are usable, it
periodically calls `0x004ad8b0(handle, &object+0x08, 0)` and restores the
frame counter used for the periodic test. The sound handle is therefore tied
to the traffic object's current fixed-point position, not just to the model
resource selected by the constructor.

The update then calls `0x00461bf0(this)`, which refreshes the object's integer
position/cache fields from `+0x08/+0x0c/+0x10` (and applies the base object's
orientation/transform path). If global runtime state queried by
`0x004cde70` equals 2, the traffic update takes a special branch through
`0x004cdef0` and does not integrate movement. Otherwise it resolves the source
node through `+0x1dc`, decodes the node position with `0x004c8650`, subtracts
the same `0x6e000` Y offset used by the constructor, and computes a movement
step with `0x004cabf0`/`0x00465f60`.

The movement state is independently bounded by the object writes at
`+0x4c/+0x50/+0x54`. The update initializes these three fixed-point response
components from the target delta, chooses one of three dominant-axis modes,
scales the step by the global fixed-point factor at `0x0056865c` and the
constructor's `+0x18` parameter, then advances `+0x08/+0x0c/+0x10` through
`0x004f5fc0`. When the target is outside the current step interval, each
response component decays by `value - (value >> 4)`. This is enough to recreate
the loaded-TRG-object movement contract without assigning game-specific names
to the response components.

Two additional consumers make the bridge concrete:

```text
traffic runtime object
  -> vtable +0x0c = 0x00412960
     -> positional sound update
     -> source-node decode and fixed-point movement
     -> 0x004136e0 proximity/interaction test

traffic runtime object + runtime context object
  -> 0x00413000
     -> fixed-point distance and swept angular tests
     -> subtype-specific traffic response sounds/effects
     -> latch at traffic object +0x1e4
```

`0x004136e0` scans the active runtime-object array at `0x0056a858`, accepts
nearby objects only when the fixed-point distance is below 2000, and uses two
cross-product tests to determine whether the object lies in the relevant
angular interval. It emits subtype-specific sound IDs and latches
`traffic +0x1e4` when a match is found. `0x00413000` is a separate traffic
interaction path: it rejects self/disabled states, performs the same fixed
point distance and swept-bound checks against a runtime context object, and
updates the traffic response components before emitting subtype-specific
effects. These paths establish a consumer beyond the PSX region lookup and
show which runtime fields are actually read after construction.

### Traffic object teardown

The first vtable entry at `0x005184e0` is the deleting-destructor wrapper
`0x004128a0`. It calls the destructor body at `0x004128c0` and frees the
0x1e8-byte allocation through `0x0047fd60` when the wrapper's destruction flag
requests it. The body restores the traffic vtable, decrements the traffic
sequence/global count at `DAT_0055faec`, removes the object from the
`DAT_0055f6bc` intrusive list through `0x004801f0`, and then invokes the base
object cleanup at `0x004012f0`.

This closes the ownership loop supported by the executable:

```text
TRG traffic node
  -> 0x00412640 allocates 0x1e8 bytes
  -> 0x004801d0 links object into DAT_0055f6bc
  -> 0x00412960 updates sound/position/response state
  -> 0x004128a0 / 0x004128c0 unlink, base-clean, and optionally free
```

The destructor body does not directly release the named PSX region slot. The
region/model selection is stored as object metadata, and the shared-region
release owner is not assigned from this destructor alone. That distinction
prevents a faithful recreation from freeing shared `C_TAXI.PSX` or
`C_BULL.PSX` data once per traffic instance.

All three families use the same intrusive list primitive at `0x004801d0`:
the new object is pushed at the supplied head, its `+0x20` next pointer takes
the old head, and its `+0x34` back pointer is cleared/installed in the old
head. The factory's `0xcb` and `0x192` constructors insert into
`DAT_0056af40`; the traffic constructor inserts into `DAT_0055f6bc`. The
activation helper `0x004802f0` performs a virtual object callback, writes its
argument at `+0x64`, calls the object's `0x004801f0` hook, then inserts into the
adjacent `DAT_0056af44` list and sets bit 0 of `+0x6a`. This distinguishes the
object-manager list operations from the PSX scene-object arrays.

Both constructors also store the return value of the shared three-word
parameter helper at `+0x17c`. The helper consumes the three position words at
the current aligned payload pointer, shifts them left by 12 into object
`+0x08/+0x0c/+0x10`, then consumes three `u16` values into
`+0x14/+0x16/+0x18`. The returned pointer is the next command/script cursor,
not a disk pointer or an opaque scalar: the virtual script interface at
`0x00402bb0` reads register words and command operands from it, advancing the
cursor by two bytes or by the command-specific payload size.

### Trigger-object script consumer

The `0xcb` object's virtual script-variable handler at `0x00402bb0` provides a
downstream runtime consumer for the constructed trigger object. It exposes the
source TRG node index at `+0xb0`, the fixed-point object position at
`+0x08/+0x0c/+0x10` (also as integer coordinates shifted right by 12), object
state bytes at `+0x16f/+0x170`, and script register values at `+0x1b0` selected
by a byte read from the `+0x17c` cursor. It also queries the current script
target object through `DAT_0056a960` and resolves linked command records through
`0x004c8550`.

Its paired command handler at `0x00403410` consumes the same cursor for
position and sound commands. Opcode `0x4506` copies the current script target
position into the object; `0x4507` and `0x4508` read one or two `u16` sound
operands and create a positional sound handle at `+0x1e8`, while `0x4509`
stops and clears that handle. Opcode `0x4700` toggles bit `0x100` in the
object state word at `+0x178`. The other recognized cursor operations are
`0x429b` and `0x42a1` (advance by 12 bytes), and `0x4504` (advance by 8 bytes
and then the common 12-byte payload); `0x429f` and `0x4503` consume no extra
bytes in this handler. Unrecognized commands are delegated to `0x004015e0`.
This proves that the post-constructor `+0x17c` pointer is a live runtime
command-stream boundary and gives one concrete trigger-object consumer beyond
list insertion.

### Type-1 script runner and variable/field contract

The shared runner at `0x00401520` makes the command-stream boundary
reproducible. It stores its input cursor at object `+0x17c`, reads one `u16`
opcode, advances the cursor by two bytes, and stops at `0x4100`. Words with
bit `0x4000` set dispatch through the object's virtual slot `+0x1c`; words
with bit `0x2000` set (and bit `0x4000` clear) dispatch through virtual slot
`+0x20`. A command handler returning zero terminates the runner. The operand
helper at `0x004027c0` likewise consumes one `u16`; a non-immediate operand
(bit `0x2000` set, bit `0x8000` clear) is resolved through virtual slot
`+0x24`. This is the exact disk/runtime script ABI: the stream is a sequence
of little-endian words with handler-specific payloads, not a fixed-size record
array.

For the `0xcb` vtable, virtual slot `+0x20` reaches `0x00403400`, which
forwards to `0x004027f0`. The supported field-writing words and their proven
cursor effects are:

```text
0x2100  consume one u16 -> object +0xb4
0x2101  consume one u16 -> object +0xae
0x2114  consume one u16 -> object +0x1bc
0x2115  consume one u16 -> object +0x1be
0x2120  read register index, resolve one operand -> u16 object +0x1b0[index]
0x2121  consume signed u16 -> sign-extended object +0x108
0x2122  consume one u16 -> object +0x1ac
0x2125  consume attribute index and one u16 -> object +0x1a0[index]
0x212c  consume one u16, retain its low byte -> object +0x170
0x212d  consume one u16, retain its low byte -> object +0x16f
0x212e  consume one u16 without storing it
0x212f  align cursor to 4, consume one u32/name token -> object +0x1a
0x2131  consume signed u16; set/clear object-flags bit 0x08
0x2135  align cursor to 4 and skip one u32
0x2140  resolve one operand -> position X at object +0x08
0x2141  resolve one operand -> position Y at object +0x0c
0x2142  resolve one operand -> position Z at object +0x10
```

The index checks are also explicit: `0x2120` accepts register indices below
six and `0x2125` accepts attribute indices below six. The six register words
at `+0x1b0` are read by variable `0x2120`; the getter at `0x00402bb0` also
exposes node index `0x212a`, linked-node state `0x212b`, the two script bytes
`0x212c/0x212d`, fixed-point position variables `0x2140..0x2142`, and target
position variables `0x2150..0x2152`. Variable `0x2129` consumes one token and
passes it through `0x004c9340`; `0x2132/0x2133`, `0x2136`, and `0x212e` remain
raw runtime queries because their higher-level names are not independently
established. This closes the type-1 post-preamble stream layout and identifies
its field consumers without assigning unsupported gameplay semantics.

### Base baddy script bytecode

The base handler at `0x004015e0` consumes the same cursor and provides the
broader script language used by the `0xcb` object family. Its label operations
are exact: `0x4104` records the current cursor in one of eight label slots at
object `+0x180`, `0x4105` scans and records labels until `0x4100`, and `0x4101`
or `0x4102` reads a label index and jumps to the saved cursor. `0x4106` reads a
node index, decodes the referenced script node through `0x004c8650`, and moves
the cursor to that node's post-header command bytes when the node is type
1000.

The control-flow and gameplay families independently supported by the switch
are:

```text
0x4110/0x4111  arithmetic/assignment form using virtual operand resolution
0x4112..0x4116 comparisons and bit tests; failure enters the conditional skip helper
0x4120          no-op
0x4121/0x4122   player flag test/set/clear
0x4123          goal-bit test
0x4201          consume animation index and cycle/direction byte; call
                `0x00480890` and reset the animation to frame zero
0x4202          consume animation index; call `0x00480730` with default
                frame range and hold parameter
0x4203/0x4204  clear or set object state bits
0x4205          invoke the object's virtual callback
0x4226/0x4227  clear or set script-local state fields
0x4240          clear the active byte at object +0x16c
0x4290/0x4291  non-positional or positional sound request
0x4292          repeated effect/particle request
0x4296/0x4297  audio-event requests
0x4298          map a shake selector to type 0/1/2 and apply it to all active
                player cameras
0x429a/0x42a6  explosion request, with the latter using the current object position
0x429c          ten-byte effect/event payload
0x429e          update the shared script camera/position words from object state
0x42a0          powerup request with four u16 operands
0x42b1/0x42b2  send or execute a command for a referenced TRG node
0x42b3/0x42b4  dispatch a command to object lists or pulse a referenced node
0x42c0          conditional event request
```

The exact operand cursor behavior is visible for the concrete forms: audio
and powerup commands advance by their consumed `u16` count, `0x42b0` skips a
null-terminated string and aligns the cursor, and `0x4101..0x4106` advance by
one or two words before replacing the cursor. Unknown opcodes report through
the base handler's unknown-command path. This turns the previously open
post-parameter bytes into a reproducible script stream while retaining raw
opcode values where the higher-level gameplay name is not independently
supported.

The animation opcodes are now tied to the common animation object state:
`0x4201` consumes two words `(animation_index, cycle_direction)` and calls
`0x00480890`, which stores the index at `+0xf6`, the direction byte at
`+0x100`, the selected frame-count byte at `+0x106`, resets `+0xf4` and
`+0x104`, and enters animation state 1. `0x4202` consumes one animation index
and calls `0x00480730` with `param_3 = param_4 = -1` and a zero hold/alternate
frame argument, causing the default range to be selected from the runtime
animation record. These commands are therefore direct trigger-script entries
into the animation runtime documented in [animation-runtime.md](animation-runtime.md).

Opcode `0x4298` consumes one selector operand, resolves it through the virtual
operand interface when needed, maps values 0, 1, and all other values to
camera shake types 2, 1, and 0 respectively, and calls `0x0040bd40` for every
active player. The camera helper selects the corresponding amplitude/timing
constants and propagates the shake through both the attached PSX environment
and TRG object lists. This is a proven script-to-camera runtime edge; the
camera constant table itself is outside the asset path and remains unnamed.

The conditional skip helper is also exact: `0x004014a0` consumes nested script
words, increments nesting for `0x4112..0x4116`, ignores `0x4120` no-ops while
inside the block, and returns at the matching `0x4100`; an unmatched
conditional terminator is reported as malformed.

The `0xcb` family also exposes a concrete post-preamble parameter layout. The
factory finds the type-1 parameter stream by adding the node's variable offset
(`node word +0x06`, in words) plus four words, then scans the byte stream with
`0x004c5420`/`0x004c5440`. Presence of bytes `0x02` and `0x04` becomes factory
state, and the first `0xff` terminates the byte preamble. The constructor then
uses the aligned bytes after that terminator as follows:

```text
after preamble  +0x00  signed/u32 source position X, shifted << 12 -> object +0x08
                +0x04  signed/u32 source position Y, shifted << 12 -> object +0x0c
                +0x08  signed/u32 source position Z, shifted << 12 -> object +0x10
                +0x0c  u16 parameter 0                         -> object +0x14
                +0x0e  u16 parameter 1                         -> object +0x16
                +0x10  u16 parameter 2                         -> object +0x18
```

This is supported by the constructor helpers rather than by naming the
remaining bytes heuristically: `0x00480240` reads and shifts the three `u32`
words, while `0x004802c0` reads the three `u16` words. For Warehouse node 2 at
file `+0x598`, the preamble is `00 02 04 ff`; the following words begin
`(0x860, 0x1000, 0x2d51)` and the next three `u16` values are `(0, 0, 0)`.
The following word `0x429e` is outside the three-u16 helper input, but it is
not promoted to a family-specific object field: the helper return is stored at
object `+0x17c`, and the virtual script interface consumes the post-parameter
bytes from that cursor. Its exact opcode/operand interpretation remains open.
The corresponding runtime object is therefore expected to carry
`(+0x860000, +0x1000000, +0x2d51000)` at its common position fields when this
node is constructed. The later `0xcb` bytes are object-family-specific and
are consumed by the script runner and virtual handlers described below.

The constructor decompilation independently fixes the remaining stable header
initialization: `0x00403000` installs vtable `0x005183b0`, stores the runtime
context at `+0x1d8`, clears the word at `+0xae`, sets `+0x16c` to 1, writes the
helper return at `+0x17c`, clears bit `0x10` in `+0x6a`, sets `+0x1ec` to -1,
and stores subtype `0xcb` at `+0x3c`. The `0x0192` constructor similarly
installs vtable `0x005194f8`, zeros `+0x1fc/+0x200/+0x20c/+0x210/+0x214`,
sets `+0x1a0` to `0x20`, `+0x16c` to 1, `+0x208` to -1, and stores subtype
`0x0192` at `+0x1f2`. These fields are exact initialization contracts even
where their later gameplay meanings remain unnamed.

For the first Warehouse type-1 record, node #2 is at file offset `0x598`, is
34 bytes long, and begins `01 00 cb 00`. Its variable payload begins at
`+0x08` with `00 02 04 ff`; the `0xff` terminator is consumed by the small
payload scanner before the `0x00403000` constructor is selected. The payload
scanner and the remaining baddy parameters are not renamed until a matching
runtime object dump is captured; the subtype/constructor/size correspondence
is independently established for all 53 type-1 nodes.

### Live Warehouse type-1 capture

A controlled run launched Warehouse (level 12) with the factory and both baddy
constructors instrumented. It captured the same disk record at both the
relocated node table and the constructor call:

```text
SKWARE_T.TRG node #147 @ file +0x18c4
  disk subtype       0x00cb
  disk preamble      01 02 04 ff
  disk position      (0x1a50, -2797, 1671)
  disk parameters    (0, 0x4104, 0)

relocated node ptr   0x005f48384
constructor object   0x005f43e40
constructor payload  0x005f48390
constructor words    (0x1a50, 0xfffff513, 0x687)
constructor params   (0, 0x4104, 0)
source node index    147
```

The constructor payload pointer is exactly the aligned post-preamble address
of the relocated disk record. The position and parameter words printed at the
live constructor entry match the offline bytes, while the static constructor
decompilation independently proves their writes to object `+0x08..+0x18` and
the factory proves the source-index write at `+0xb0`. This is a direct
disk -> relocated node -> heap object-constructor witness, rather than a
numeric index coincidence. The same run also reached subtype `0x0192`
constructors, including node #131 at object `0x005f3fe94`, proving that the
second baddy family is active in the normal Warehouse load.

### Concrete Warehouse node #17

The offline `SKWARE_T.TRG` node table places node 17 at file offset `0x758`,
with a 28-byte record. Its bytes are:

```text
05 00 06 00 00 00 00 00 f9 1d 00 00 55 fe ff ff
64 2f 00 00 00 00 01 00 ff ff ff ff
```

The offline parser labels this as a type-5 powerup. The controlled runtime run
relocated the same record to `0x005f47218`, dispatched it through the type-5
branch, and reached the common factory postamble with:

```text
TRG node index       17
relocated node ptr   0x005f47218
runtime object ptr   0x005f404c0
object +0xb0         0x0011   # source node index 17
```

The first three position words are independently visible on both sides. The
disk words at node `+0x08/+0x0c/+0x10` are `(7673, -427, 12132)`; the runtime
object at `0x005f404c0` has `+0x08/+0x0c/+0x10` equal to
`(0x01df9000, 0xffe55000, 0x02f64000)`, exactly those values shifted left by
12. This is a stronger correspondence than matching the node index alone:

```text
SKWARE_T.TRG node #17 @ file +0x758
    -> relocated node 0x005f47218
    -> 0x004c8130 type-5 dispatch
    -> 0x004c5460 / powerup construction
    -> runtime object 0x005f404c0
       +0x08/+0x0c/+0x10 = node position << 12
       +0xb0 = 17
```

The object dump also shows a constructor/list pointer at `+0x20` and a second
object/list pointer at `+0x34`; their exact list ownership is not assigned here
because the factory selects among several constructor families.

The type-5 branch is expanded in [items-runtime.md](items-runtime.md): its
0x100-byte powerup object resolves `ITEMS.PSX`/`SKMEDALS.PSX` model checksums
and enters the separate `DAT_0056b830` render list. This keeps the trigger
node/object identity separate from the Warehouse PSX environment object while
still providing a disk-to-runtime model bridge.

## Downstream command/object-manager consumer

`0x004c8550` is the TRG link lookup helper. Given a node index, it returns the
node's link list for the supported node types. `0x004c7a00` consumes that list
when sending a kill/pulse command:

- for referenced type-1 baddy nodes, it walks `DAT_0056af40` through `+0x20`;
- it matches the object's `+0xb0` node index, checks flags at `+0x04`, and
  checks state at `+0x178`;
- it marks the object, sends the associated command through `0x004c5b00`,
  records state at `+0x171`, and invokes `0x0049f4c0`; and
- for type-2/9 links it calls `0x004b1ef0` on the node payload instead.

The command-point path is independently visible in the helper chain:

```text
0x004c58b0  send pulse to a node
  -> 0x004c5ac0 find the type-6 link record by source node index
  -> increment link +0x07
  -> 0x004c5dc0 interpret link +0x00 as a u16 command stream
  -> command 0x0b/0x0c can re-enter 0x004c7a00

0x004c5b00  execute a counted u16 node-command list
  -> 0x004c58b0 for each referenced node
```

`0x004c5ac0` only accepts a type-6 source node and walks the global link chain
through `+0x14`, matching the record's `+0x0a` source index. This proves the
type-6 record is a command-point runtime object rather than merely an index
cache. `0x004c5dc0` consumes the pointer stored at `+0x00` as a variable-length
command stream and uses the `0xffff` terminator convention visible in the
Warehouse bytes.

The command-stream payloads around the link consumer are also recoverable:

```text
u16 command 0x0002  -> read NUL-terminated cheat strings; store up to 20
u16 command 0x0003  -> execute the source node's counted command list
u16 command 0x0004  -> activate/suspend the source node's link targets
u16 command 0x0005  -> activate/suspend the source node's link targets
u16 command 0x000a  -> u16 count followed by that many node indices;
                       0x004c5b60 signals linked type-1/type-6 nodes
u16 command 0x000b  -> send kill/pulse command with mode 0
u16 command 0x000c  -> send kill/pulse command with mode 1
u16 command 0x000d  -> consume one following u16 and update visibility
u16 command 0x007e  -> NUL-terminated PSX resource name, mode 0
u16 command 0x0080  -> NUL-terminated PSX resource name, mode 1
u16 command 0x0081  -> drain the pending PSX spool queue
```

The count-bearing forms are also fixed by the helper at `0x004c5c70`:
commands `0x0004` and `0x0005` consume `u16 count` followed by that many
`u16` node indices and pass the list to the activate/suspend helper. This is
the same list shape used by command `0x000a`; command `0x0003` has no inline
payload and executes the source node's own counted list.

## Additional link-command ABI

The remaining cases in `0x004c5dc0` are useful to a faithful runtime
recreation even when they are cutscene, camera, or platform leftovers. The
table records only direct cursor widths and direct helper/state effects; it
does not promote helper names to gameplay meanings without an independent
consumer.

| command | inline payload after the command | direct runtime effect |
| --- | --- | --- |
| `0x0068` | three `u16` | after the whole stream, call `0x00464710(a,b,c)`; the helper asserts the third value is a power of two and publishes the fog-range globals |
| `0x0069` | one `u16` signed sound value | if the active sound context is idle, call `0x004ad620(value, 0x2000, 0)` |
| `0x006a` | one `u16` signed sound value | if the active sound context is idle, call `0x004ad9f0(value)` |
| `0x007f` | NUL-terminated string, aligned to an even address | call `0x004b3270(string)` before skipping the string; this selects/clears a loaded PSX region through the named-region table |
| `0x0082`, `0x0087` | two `u16` | call the corresponding front-object helper `0x0040f140`/`0x0040f150` when `DAT_0055fa38` is live |
| `0x0083`, `0x0084` | one `u16` | call `0x00401000` or `0x00401030` |
| `0x0085` | `u16` mode, then repeated aligned six-`u32` records, terminated by `u16 0xff` | pass each pair of fixed-point 3D vectors to `0x004ca270`; that helper tests the bounds against the traffic, baddy, powerup, and auxiliary object lists |
| `0x0086` | one `u16` | on a type-6 link whose `+0x06` is clear, set `+0x06 = 1` and copy the operand to `+0x08` |
| `0x008a` | one `u16` | report `SeekXA` unsupported and consume the operand |
| `0x008b` | two `u16` words (the second is the shared `0xae` skip word) | report `PlayXA` unsupported |
| `0x008c`, `0x00b0` | NUL-terminated string, aligned | call the restart/resource helper `0x004c4c50`; horse-mode selection changes the source of the restart name, and a non-default level may call `0x00473650` afterward |
| `0x008d` | two `u16` flags, then repeated aligned six-`u32` records, terminated by `u16 0xff` | pass fixed-point vector pairs and two boolean flags to `0x004ca2d0`, which updates visibility bits across runtime object lists |
| `0x008e` | NUL-terminated string, aligned | save the cursor/name as the current track resource, resolve its loaded PSX slot with `0x004b3230`, and, in competition mode, copy the name into one of fifteen bounded spool buffers |
| `0x008f`..`0x0092` | two `u16` | call front-object helpers `0x0040f160`, `0x0040f170`, `0x0040f180`, or `0x0040f190` |
| `0x0093` | one `u16` | write the value to global `DAT_005685ec` |
| `0x0094` | one `u16` pulse-count comparison | if the type-6 link `+0x07` does not match, scan to the matching `0x0095` and skip the conditional body; nested blocks are rejected |
| `0x0095` | none | close one `0x0094` conditional block |
| `0x0096` | one `u16` | call `0x004adc60(value)` |
| `0x0097` | one `u16` time value | reset the command timer and call `0x004c5d90(value * 1000)` |
| `0x0099`, `0x009a` | one `u16` | write the operand through front-object helpers `0x0040bd00` or `0x0040bd10` |
| `0x009b`, `0x009c`, `0x009d` | one `u16` | call `0x004adc50`, `0x004adc30`, or `0x004ada20` respectively |
| `0x009e` | none | call `0x00466c10` |
| `0x009f` | NUL-terminated string, aligned | call `0x004ad5e0(string)` |
| `0x00a0` | one `u16` | write front-object field `+0x504` |
| `0x00a1` | one `u16` | call front-object helper `0x0040be60` |
| `0x00a2` | NUL-terminated string, aligned | report `LoadAI` unsupported and skip the string |
| `0x00a3` | one `u16` | if `DAT_0056a960` is live, write its field `+0x3198` |
| `0x00a4`, `0x00a5` | one `u16` | call front-object helpers `0x0040bd20` or `0x0040bd30` |
| `0x00a6`, `0x00a9`, `0x00aa` | one `u16` | write globals `DAT_00564368`, `DAT_0056436a`, or `DAT_00535af8` |
| `0x00a7` | two `u16` | call front-object helper `0x0040f1a0`; its direct implementation stores the second operand at `+0x410` and derives `+0x414` from the first operand when nonzero |
| `0x00a8`, `0x00ac` | one `u16` | write front-object fields `+0x434` or `+0x436` |
| `0x00ab` | aligned `u32` plus three `u16` | allocate a `0xcc`-byte record and pass the four decoded values to `0x00401060` |
| `0x00ad` | none | copy front-object field `+0x3a4` to `+0x3dc` |
| `0x00b1` | one `u16` | if `DAT_0056a960` is live, write its field `+0x319c` |
| `0x00c8` | three `u16` | combine the first two operands into `DAT_00563a60` as high/low halves |
| `0x00c9` | aligned `u32` checksum plus one `u16` | match the checksum against the runtime gap table at `+0x2f74`; on a valid entry, update the skater gap state and execute the source node's counted list |
| `0x00ca` | two `u16` | combine the operands into `DAT_0056114c` and call `0x0042fc70` |
| `0x00cb` | one `u16` flag below 8 | set the corresponding character-config bit when the career/level condition allows it |
| `0x00cc` | one `u16` flag below 8 | conditionally execute the following stream until `0x0095` based on the character-config flag |
| `0x00cd` | one `u16` goal below 11 | conditionally execute the following stream until `0x0095` based on the goal bit |

The interpreter's skip helper at `0x004c7c50` independently reproduces these
widths, including the aligned 24-byte records for `0x0085`/`0x008d`, the
aligned 10-byte payload for `0x00ab`, and the aligned 6-byte payload for
`0x00c9`. That second parser is important: it proves the cursor contract is
shared by conditional skipping and execution, not just an artifact of one
decompilation path.

The command interpreter advances by the payload-specific amount: ordinary
node/list commands consume no inline words, command `0x000a` consumes its
counted node-index list, command `0x000d` consumes one word, and resource
commands consume the aligned end of their NUL-terminated string. The `0x000a`
signal helper dispatches each referenced type-1/type-6 node through both the
traffic and baddy object lists. This identifies the link payload as a runtime
command stream with direct object-manager effects, rather than a passive
serialized relationship table.

The traffic list has a direct node-index consumer as well. The helper at
`0x004c5c00(list_head, node_index)` walks the intrusive `+0x20` chain, compares
each object's `+0xb0` source TRG node index, and calls `0x004802f0` on a match
with the matching object as `this` and the supplied list head as its argument.
The command-list interpreter around `0x004c5c70` uses this helper for command
code `5` on referenced type-1/type-6 nodes, once with the traffic head
`DAT_0055f6bc` and once with the baddy head `DAT_0056af40`. This independently
proves that traffic objects created from `SKWARE_T.TRG` are activated through
their own runtime list and node-index bridge, rather than being treated as
PSX geometry records.

The resulting trigger-side flow is:

```text
SKWARE_T.TRG node index
  -> DAT_0056e210 relocated node pointer
  -> 0x004c8130 dispatch
  -> 0x004c5460 constructor / 0x004c84d0 link record
  -> DAT_0056af40 object-manager list
  -> 0x004c7a00 node-link command consumer
```

## Confidence and limits

- `confirmed`: exact Warehouse TRG file path, header/version/count checks,
  relocated offset table, type-1 subtype split, all three type-1 constructor
  families, subtype-specific traffic resource selection, constructor
  allocation/entry points, node-index storage at factory object `+0xb0`,
  intrusive list insertion, link-record field offsets/list insertion, type-6
  link lookup, command-stream dispatch and skip-parser cursor widths, the
  `0xcb` post-preamble
  position/parameter handoff, script runner dispatch/termination, operand
  resolution, `0xcb` script field writes/cursor advances, traffic-list
  activation by source node, the object-manager consumer, traffic vtable/update
  entry, fixed-point target integration, positional-sound update, direct
  resource/audio/fog/bounds/visibility command effects, traffic
  proximity/interaction consumers, and the type-12/14 trick-object allocation,
  checksum, activation, and per-frame update path.
- `observed`: Warehouse node histogram, one relocated node pointer, the node-17
  runtime object pointer, and its fixed-point position/source-index handoff.
- `inferred`: some offline type labels for runtime variants, link-record field
  names, and the full constructor semantics for each object subtype.

The remaining TRG work is semantic naming for some base gameplay opcodes and
link-command payloads. The type-5 node-17 position/index bridge, the type-6
disk-command/runtime-link bridge, and the traffic-object consumers are already
proven without conflating any of them with the PSX geometry index.
