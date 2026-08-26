# Warehouse TRG node loading and runtime object path

Status: confirmed trigger-file parse, object-manager dispatch, all three type-1 constructor families selected by the PC executable, constructor field initialization, TRG link/command records, and live disk-node to constructor/object correspondences; broader node payload semantics remain partial
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004c5130`, `0x004c8130`, `0x004c8650`, `0x004c5460`, `0x00403000`, `0x0049f250`, `0x00412640`, `0x00480240`, `0x004802c0`, `0x00402bb0`, `0x00403410`, `0x004c84d0`, `0x004c5ac0`, `0x004c58b0`, `0x004c5b00`, `0x004c5dc0`, `0x004c8550`, `0x004c7a00`

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
object state word at `+0x178`. Other recognized commands advance the cursor by
their fixed payload sizes. This proves that the post-constructor `+0x17c`
pointer is a live runtime command-stream boundary and gives one concrete
trigger-object consumer beyond list insertion.

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
The following word `0x429e` is outside the three-u16 helper input and remains
an unassigned family-specific field.
The corresponding runtime object is therefore expected to carry
`(+0x860000, +0x1000000, +0x2d51000)` at its common position fields when this
node is constructed. The later `0xcb` bytes remain object-family-specific.

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
  link lookup, command-stream dispatch, the `0xcb` post-preamble
  position/parameter handoff, and the object-manager consumer.
- `observed`: Warehouse node histogram, one relocated node pointer, the node-17
  runtime object pointer, and its fixed-point position/source-index handoff.
- `inferred`: some offline type labels for runtime variants, link-record field
  names, and the full constructor semantics for each object subtype.

The remaining type-1 work is to assign the family-specific bytes after the
  three-u16 helper output. The type-5 node-17 position/index bridge and the
type-6 disk-command/runtime-link bridge are already proven without conflating
either with the PSX geometry index.
