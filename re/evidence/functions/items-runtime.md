# ITEMS, BITS, and SKMEDALS PSX runtime paths

Status: confirmed level item/medal PSX load, BITS tag parse and named-resource
lookup, trigger-subtype model selection, powerup-object allocation, list
ownership, and renderer/update handoff
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004524a0`, `0x004b37a0`, `0x004b3df0`, `0x004b34f0`,
`0x004b3680`, `0x004c8130`, `0x004c5460`, `0x004a8e50`, `0x004a7c50`,
`0x004b1de0`, `0x004a8ac0`, `0x00467c90`, `0x0046a8d0`

This is the level item family that was previously only named in the frontend
loader. It is separate from the Warehouse environment PSX object array: item
and medal instances are TRG-created gameplay objects, while their visual model
data lives in named PSX regions. The adjacent `BITS.PSX` resource is a
different type-`0x45` PSX payload used by named effect/bit resources such as
`Shadow` and `Smoke`; it does not contain scene models.

## Disk assets and level-load handoff

The extracted assets are:

```text
ITEMS.PSX
  size          39096
  SHA-256       0daae61113ff6f4cc91556d2ee98c95f31a5af31893a12da2fdfa10c37b39707
  objects/models 13/13
  vertices/normals/faces 249/258/258
  inline images 13 (all 4-bit, 16-color)

SKMEDALS.PSX
  size          24012
  SHA-256       1293ce0cd8a8198983804c087ddd266f2e613507e2a6ddf5dd6a16e71cd4a36b
  objects/models 3/3
  vertices/normals/faces 276/156/156
  inline images 9 (six 4-bit and three 8-bit)

BITS.PSX
  size          8636
  SHA-256       3b6fb52c5353c85a3ba25f0f655dbdfff3d761a0891d50c491c398fcc1f54a42
  tag           type 0x45 at file offset 0x10
  named groups  FONT, SHADOW, SMOKE, ribbon, Buttons
```

`Front_LoadGame` queues the ordinary item and BITS regions before the trigger
file is parsed:

```text
0x004b37a0("items", mode=0, heap=0, flags=1) -> DAT_0056dcac
0x004b37a0("bits",  mode=0, heap=0, flags=1) -> DAT_0056dd5c
0x004b3df0() -> drain the PSX spool queue
```

On levels whose static level record enables medals, the same function logs
`loading Skmedals from front_loadgame`, queues `SkMedals` through
`0x004b37a0`, stores the returned region slot in `DAT_0056dcad`, and drains the
queue again. The queue then follows the common `0x00449030` open,
`0x0046f490` allocation, `0x00449230` read, and `0x004b2450` PSX parse path.
The slot values are bytes; `0xff` is the no-region sentinel.

### BITS type-0x45 payload

The common PSX parser dispatches tag type `0x45` to `0x004b34f0`. That parser
calls `0x004b20f0` for the resource tables, creates an 8-byte linked-list node
at `DAT_0056db38`, and rewrites each named group's entry pointers to the
runtime records used by the bit/effect subsystem. `0x004b3680(name)` performs
a case-insensitive walk of that list and returns the matching named group's
runtime data pointer. This is a separate named-resource lookup contract from
the scene material hash table.

The startup/level path independently consumes this result:

```text
BITS.PSX type 0x45
  -> 0x004b34f0 named-group/runtime-record table
  -> DAT_0056db38 linked list
  -> 0x004b3680("Shadow")
  -> shadow animation/texture record in 0x0046a8d0
```

`0x0046a8d0` checks the `Shadow` lookup result, requires its animation and
texture pointers, adjusts their shared reference counts, and later uses the
record to initialize the shadow effect. The executable therefore proves a
BITS disk-to-runtime named-resource handoff even though the individual bit
stream opcode meanings are not needed for the item/model path.

## Trigger subtype to model lookup

`0x004c8130` dispatches TRG type-5 powerup nodes through
`0x004c5460`. The factory decodes the node position with `0x004c8650`, then
calls `0x004a8e50`, which allocates `0x100` bytes and enters the powerup
constructor `0x004a7c50`.

The constructor first resolves the named region (`"items"` or
`"skmedals"`) through `0x0047fe30`. It then calls `0x004b1de0(checksum,
region_slot)`. Despite the decompiler's `void` return type, the caller uses
the returned value as the model index; the function scans the region's
model-name checksum array and returns the matching index. This gives an exact
subtype-to-disk-model contract:

| TRG object subtype | region | checksum passed to lookup | offline model index |
| ---: | --- | ---: | ---: |
| `4` | `items` | `0x2328a71c` | 4 |
| `5` | `items` | `0x311d55d4` | 3 |
| `6` | `items` | `0x2ebf22ca` | 5 |
| `10` | `items` | `0x29b68a16` | 7 |
| `15` | `items` | `0x34524351` | 6 |
| `16`, `18` | `items` | `0x7c1b2c4a` | 8 |
| `24` | `items` | `0x694ed947` | 11 |
| `25` | `items` | `0x260f4f80` | 10 |
| `26` | `items` | `0xcc4e141f` | 9 |
| `33` | `items` or current level region | conditional | conditional |
| `0x664` | `skmedals` | `0x54636518` | 0 |
| `0x665` | `skmedals` | `0xba6d0434` | 2 |
| `0x666` | `skmedals` | `0x2364558e` | 1 |

The full `ITEMS.PSX` model-name table is independently:

```text
model 0  0x6f0f6b9f       model 7  0x29b68a16
model 1  0xf6063a25       model 8  0x7c1b2c4a
model 2  0x81010ab3       model 9  0xcc4e141f
model 3  0x311d55d4       model 10 0x260f4f80
model 4  0x2328a71c       model 11 0x694ed947
model 5  0x2ebf22ca       model 12 0x4605a8d0
model 6  0x34524351
```

`SKMEDALS.PSX` has model 0 `0x54636518`, model 1 `0x2364558e`, and model 2
`0xba6d0434`. The runtime lookup therefore bridges the TRG subtype to an
offline PSX model without assuming that the subtype numerically equals the
model index.

## Runtime powerup object

`0x004a7c50` initializes the `0x100`-byte object returned by
`0x004a8e50`. Independently supported fields are:

```text
+0x00  vtable 0x00519684
+0x08  fixed-point position X
+0x0c  fixed-point position Y
+0x10  fixed-point position Z
+0x3c  TRG item/medal subtype
+0x1a  model index returned by 0x004b1de0
+0x1f  PSX region slot when the constructor selects an alternate region
+0x20  intrusive-list next pointer
+0x34  intrusive-list previous pointer
+0x6a  object/activation flags
+0xb0  source TRG node index, written by 0x004c5460
+0xd1  visual/object activation flag
+0xd2  motion/seed state flag
+0xc8  lazily-created glow/sprite runtime object
```

The constructor inserts the object through `0x004801d0(&DAT_0056b830)`. The
same list is consumed by the renderer path: `0x00467c90` calls
`0x0045f530(DAT_0056b830)` after the environment and baddy lists, so item
objects enter the normal object/model render traversal. The virtual update
path reaches `0x004a8ac0` in `powerup.cpp`; it advances the object's fixed-point
motion, updates its active position, creates/updates its glow state, and uses
the object state flags before the generic renderer consumes the object.

## Concrete Warehouse bridge

Warehouse `SKWARE_T.TRG` node 17 is a type-5 powerup with subtype `6`.
The controlled run already captured its relocated node and returned runtime
object:

```text
SKWARE_T.TRG node #17 @ file +0x758
  type/subtype       = 5 / 6
  disk position      = (7673, -427, 12132)
  relocated node     = 0x005f47218
  runtime object     = 0x005f404c0
  object +0xb0       = 17
  object +0x08..+10  = (0x01df9000, 0xffe55000, 0x02f64000)
```

The subtype-6 constructor branch independently selects checksum
`0x2ebf22ca`; the offline `ITEMS.PSX` table identifies that checksum as model
5. Thus the supported disk/runtime bridge is:

```text
SKWARE_T.TRG node #17 / subtype 6
    -> 0x004c8130 type-5 dispatch
    -> 0x004a8e50 / 0x004a7c50 0x100-byte powerup object
    -> "items" region slot DAT_0056dcac
    -> checksum 0x2ebf22ca
    -> ITEMS.PSX model #5
    -> runtime object +0x1a model index / +0x1f region slot
    -> DAT_0056b830 object list
    -> 0x004a8ac0 update and 0x0045f530 renderer traversal
```

This is the missing item-family counterpart to the Warehouse environment
object #17 bridge. Heap addresses are run-specific; the subtype, checksum,
model index, allocation size, source-node field, list head, and consumer calls
are stable executable facts.

## Confidence and limits

- `confirmed`: all three PSX asset identities and their recovered structural
  metadata; ordinary/optional region queueing; type-5 trigger dispatch;
  0x100-byte allocation; item/medal
  subtype-to-checksum mapping; checksum-to-model-index lookup; source-node and
  position fields; item-list insertion; powerup update; generic renderer
  traversal; and the BITS type-0x45 named-group list/lookup handoff.
- `observed`: Warehouse node 17's live relocated-node/runtime-object pair and
  the type-5 powerup branch.
- `inferred`: public names for the remaining powerup state fields and the
  higher-level meanings of individual BITS group payload fields.
