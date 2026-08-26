# Collision query setup and forwarding

`0x004624d0–0x004628e6` receives a query pointer as its sole stack argument.
The first six words are two XYZ endpoints. Its reviewed 120-byte prefix:

- installs `0x7fffffff` sentinels at offsets `+0x40` and `+0x8c`;
- derives fixed-point endpoint deltas by shifting each component by 12;
- clears fields at `+0x68`, `+0x80`, `+0x88`, and `+0x89`;
- installs `-1` at `+0x84`;
- branches to a special axis-aligned construction path when the squared X/Y
  delta is zero.

The remainder normalizes the horizontal delta, constructs and rotates a 3x3
fixed-point basis, publishes that basis to the collision globals, derives
component-wise endpoint bounds at offsets `+0x18–+0x2c`, calls `0x00462490`,
and stores the resulting word at `+0x8a`. The complete 1,046-byte function is
matching assembly with no `incbin`.

`0x00466090–0x004660a5` is not the collision implementation. It forwards its
two arguments unchanged to `0x004660b0`, repairs the caller stack, clears EAX,
and returns. Its complete 21 bytes are matching assembly. Consequently, the
position path's acceptance field is populated through the query object by
`0x004660b0`; it is not the wrapper's return value.

`0x004660b0–0x004667d3` is the complete collision-query engine. Static control
flow shows it initializing collision masks, then walking 0x660-byte partition
descriptors rooted at `0x00567f80` until a null pointer slot. It rejects
partitions against the query bounds, handles a degenerate point query directly,
and otherwise clips/rasterizes the segment into spatial cells. The repeated
calls to `0x004f5f10` perform the integer interpolation used during clipping;
selected cell entries are dispatched through `0x004638d0`. After the partition
sentinel, `0x00463d50` finalizes the query object. Exact semantic names for the
partition and cell structures still require dynamic confirmation.

The complete 1,827-byte engine is matching assembly with no `incbin`.
