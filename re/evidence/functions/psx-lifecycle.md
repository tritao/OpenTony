# PSX region ownership and teardown

Status: confirmed region-slot cleanup, material-usage decrement, environment
detach, and level teardown call site
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004b1bb0`, `0x004b2120`, `0x004b3090`, `0x004b32f0`,
`0x00466030`, `0x0046a250`, `0x004544a0`

The common PSX loader has an explicit inverse path. It is not sufficient to
drop the raw file buffer: the runtime clears relocated tables, decrements
shared material references, detaches environment lists, and resets blockmap
state before reusing a slot.

## Region clear operation

`0x004b32f0(region, cleanup_flags, release_tables)` is the central clear
operation. It validates a region index in the 20-entry slot range and requires
that the slot have a non-empty name and usable type. Its behavior splits by
the region type byte at `DAT_0056d432[region * 0x44]`:

```text
type 2 region
    -> free raw/final buffer at DAT_0056d440[region * 0x11]
    -> clear raw pointer and region name

ordinary PSX region
    -> 0x004b2120 decrement model checksum/material usage
    -> optional region destructor through the relocated object-table vtable
    -> clear object table, animation table, model/material metadata, and name
    -> 0x004b3990 find attached environment-list index
    -> 0x00466030 clear the corresponding blockmap grid/cache
    -> 0x004b3090 detach environment-list membership
    -> free raw/final buffer
```

The decompiler exposes the exact published arrays cleared by the ordinary
branch:

```text
DAT_0056d438[region * 0x11]  object-record table
DAT_0056d43c[region * 0x11]  model-pointer table
DAT_0056d440[region * 0x11]  raw/final PSX buffer
DAT_0056d444[region * 0x11]  animation table
DAT_0056d44c[region * 0x11]  model/material metadata
DAT_0056d454/458              region-side auxiliary pointers
DAT_0056d428[region * 0x44]  bounded region name
DAT_0056d432/433              region type/state bytes
DAT_0056d463/464              texture/runtime state bytes
```

`cleanup_flags != 0` additionally calls the adjacent material/texture cleanup
helpers after the slot tables are cleared. The exact graphics-device release
performed by those helpers remains a lower-level texture-manager question; the
ownership edge is confirmed at this boundary.

## Shared material lifetime

`0x004b2120(region)` walks the region's model checksum references. For each
checksum entry it locates the shared scene-material record and decrements the
16-bit usage count at material `+0x10`. This is the inverse of the parse-time
material allocation and face-reference relocation documented in
[asset-loading.md](asset-loading.md).

The consequence for a recreation is important: a material record is shared by
faces/models/regions and cannot be freed merely because one model's raw file
buffer is going away. The loader's checksum table and the teardown usage pass
form a reference-counted ownership layer.

## Environment-list and blockmap cleanup

`0x004b3090(environment_list_index)` detaches one of the two environment-list
links recorded through `DAT_00537c80`. It updates `DAT_0056db28`, the head of
the attached environment object list, and removes the detached region's tail
link. This is the list later traversed by the renderer and collision code.

`0x00466030(blockmap_index)` clears the active flag and 400 pointer words in
the corresponding `0x660`-byte blockmap-cell storage. This is the inverse of
the type-10 blockmap relocation in [blockmap-collision.md](blockmap-collision.md):
the cell pointer table is transient runtime state and is explicitly reset when
its region is released.

## Level lifecycle call site

`0x0046a250` is the level teardown routine used after a level session. It
flushes gameplay/audio/graphics subsystems, clears the active environment
region for the relevant mode through `0x004b32f0`, releases level-owned
resources, and resets the session state. The level-launch path in
`0x004544a0` also clears the previous custom-park region before building the
next one.

The load/consume/clear lifecycle is therefore:

```text
file open/read
  -> PSX parse/finalize/attach
  -> DAT_0056d438/43c/440 published tables
  -> renderer, collision, trigger, player consumers
  -> 0x0046a250 level teardown
  -> 0x004b32f0 region clear
  -> material usage decrement + list/blockmap/table/raw-buffer cleanup
```

## Confidence and limits

- `confirmed`: 20-slot free-slot scan, central clear operation, ordinary/type-2
  split, material usage decrement, table/name reset, environment detach,
  blockmap reset, and level teardown caller.
- `observed`: optional cleanup flag path and the exact list of auxiliary slot
  words cleared.
- `open`: the complete internal behavior of the final graphics/material cleanup
  helpers and the destructor implementation behind the relocated table vtable.
