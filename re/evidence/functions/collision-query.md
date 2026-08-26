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
