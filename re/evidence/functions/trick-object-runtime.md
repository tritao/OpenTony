# TRG type-12/14 trick-object runtime path

Status: confirmed disk checksum handoff, allocation, record layout, activation
from gameplay events, and per-frame runtime consumption; trick scoring and the
full `TRICKS.BIN` physics semantics remain separate open work
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004c8130`, `0x004bd760`, `0x004bd980`, `0x004bdc40`,
`0x004bdd00`, `0x004b1f40`, `0x004bdea0`, `0x00469de0`, `0x0048f8f0`,
`0x00490ae0`

This is the level-side trick-object bridge. It is distinct from the
`TRICKS.BIN` database path in [bin-runtime.md](bin-runtime.md): a TRG node
selects a model checksum and creates a small runtime record that watches for
player/rail/object events, while `TRICKS.BIN` supplies the broader trick
configuration tables.

## Disk witness

The Warehouse trigger file is `SKWARE_T.TRG`, a version-2 `_TRG` resource with
313 nodes. `0x004c8130` dispatches node types 12 and 14 into a small allocation
followed by `0x004bd760`. The constructor reads an aligned `u32` after the
node's variable-length prefix and stores it as the record's model checksum.

Several type-12 nodes independently cross-match this checksum against the
offline `SKWARE.PSX` model-name checksum table:

```text
TRG node 181  payload checksum 0xed3c7cd2  -> SKWARE.PSX model 86
TRG node 189  payload checksum 0xc02d64c7  -> SKWARE.PSX model 95
TRG node 196  payload checksum 0x8d071afa  -> SKWARE.PSX model 106
TRG node 206  payload checksum 0x5924357d  -> SKWARE.PSX model 116
TRG node 209  payload checksum 0x917bab5b  -> SKWARE.PSX model 119
TRG node 213  payload checksum 0x140e4b40  -> SKWARE.PSX model 123
TRG node 219  payload checksum 0x59907e0a  -> SKWARE.PSX model 135
TRG node 232  payload checksum 0xb2c3b10f  -> SKWARE.PSX model 156
TRG node 280  payload checksum 0x03321dfe  -> SKWARE.PSX model 228
```

For example, node 181 is a ten-byte type-12 record at file offset `0x1e2c`:
the first four bytes are the type/prefix header, and the aligned payload is
`0xed3c7cd2`. This is a direct disk-to-model-table correspondence; it does
not depend on a runtime pointer remaining stable between launches. The other
type-12 payload variants are not all assigned here because some node layouts
contain additional words whose semantic role is not independently established.

## Runtime allocation and record

The type-12/14 branch in `0x004c8130` requests `0x18` bytes and calls
`0x004bd760`. The constructor installs vtable `0x0051982c`, validates the TRG
node index and delay level, reads the checksum payload, and pushes the record
onto the intrusive list headed by `DAT_0056db90`. The list is released by
`0x004bd700`/`0x004bd720`.

The independently supported record is:

| Offset | Field | Evidence |
| --- | --- | --- |
| `+0x00` | vtable pointer | written by `0x004bd760` |
| `+0x04` | model-name checksum | aligned node payload written by the constructor; matched to PSX model tables above |
| `+0x08` | source TRG node index (`u16`) | written from the constructor argument |
| `+0x0a` | activated/state byte | cleared by construction and set by activation paths |
| `+0x0b` | player number byte | set by player/goal activation paths |
| `+0x0c` | delay/score threshold word | read and updated by activation/update logic; exact gameplay name remains open |
| `+0x10` | next trick-object record | list traversal and constructor insertion |
| `+0x14` | resolved runtime object/context pointer | written by activation paths and consumed by the update |

The record size and field offsets are also captured in
[`RuntimeTrickObject`](../../types/trg.yml).

## Activation consumers

`0x004bd980` is the main activation path. It receives a runtime object/context
pointer, the current player context, and an event/mode value. It obtains the
object's model-name checksum through `0x004b1f40`, then searches the trick
record list for a matching `+0x04` checksum. The helper's inverse is
`PSX_FindModelIndexByChecksum` at `0x004b1de0`: together they establish the
runtime-object region/model-index ↔ PSX model-name-checksum relationship.

When the record is eligible, activation marks the triggering runtime object
with the observed visual flag at `+0x05` and color word `+0x24 = 0x202020`,
calls the object/trick event helper at `0x004df2c0`, records the current player
and event state, follows the source node's link list through `0x004c8550`, and
dispatches linked type-12/14 nodes through `0x004bdc40`. The resolved object or
context is stored at trick record `+0x14`, and the record state byte at `+0x0a`
is set active.

The player path is a real downstream consumer: `0x0048f8f0` drains a player's
pending trick/event entries and calls `0x004bd980` for each one. The rail path
at `0x00490ae0` also calls it after resolving a rail node, using `-1` to select
the goal/type-14 gating path. This connects player physics and rail events to
the records created from the level TRG file.

## Per-frame consumer

`0x004bdd00` walks `DAT_0056db90` every gameplay frame. The main update at
`0x00469de0` calls it after the generic powerup/object-list update and before
the baddy/traffic list updates. For active records with a resolved `+0x14`
pointer, it computes a frame-dependent intensity from the stored delay/event
state and writes the resulting tint/intensity into the resolved runtime
object's color field. The mode-8 path uses the record delay and player byte;
the other path uses the frame triangle. This is a proven consumer of the
runtime record, but not a claim that the color word is the final renderer
material model.

`0x004bdea0` is the related trick-object line-command builder: it emits a
five-word per-frame command into the render arena from coordinates supplied by
the trick-object rendering code. Its exact map/line caller relationship is
recorded as observed; the checksum/activation/update path above is the
disk-to-runtime proof.

## Recreated flow

```text
SKWARE_T.TRG type-12 node
  -> relocated node table DAT_0056e210
  -> 0x004c8130 type-12/14 dispatch
  -> 0x004bd760, 0x18-byte RuntimeTrickObject
       +0x04 = PSX model-name checksum
       +0x08 = offline TRG node index
  -> player trick / rail event
  -> 0x004bd980 checksum match and activation
  -> 0x004bdc40 linked trick-object activation
  -> 0x004bdd00 per-frame tint/intensity consumer
```

The old cross-build symbol dump names this family `CTrickOb` and exposes
names such as `TrickOb_Init`, `TrickOb_ObjectTrickStatus`, `TrickOb_TrickedOnItem`,
and `TrickOb_Update`. Those names corroborate the role of the PC functions,
but the PC addresses and field offsets above come from the PC executable's
constructor and consumers.

## Confidence and limits

- `confirmed`: type-12/14 dispatch, `0x18` allocation, constructor/list path,
  node index/checksum storage, matched TRG-to-PSX checksum examples, player and
  rail activation calls, resolved-object storage, and per-frame update.
- `observed`: tint field writes, delay/player gating, and the related line
  command builder.
- `open`: complete trick scoring semantics, every type-12 variable payload
  variant, and the semantic interpretation of the `TRICKS.BIN` table-class and
  descriptor fields.
