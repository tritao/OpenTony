# Collision query setup and forwarding

`0x004624d0–0x004628e6` receives a query pointer as its sole stack argument.
The first six words are two XYZ endpoints. Its reviewed 120-byte prefix:

- installs `0x7fffffff` sentinels at offsets `+0x40` and `+0x8c`;
- derives fixed-point endpoint deltas by shifting each component by 12;
- clears fields at `+0x68`, `+0x80`, `+0x88`, and `+0x89`;
- installs `-1` at `+0x84`;
- branches to a special axis-aligned construction path when the squared X/Z
  delta is zero.

The remainder normalizes the horizontal delta, constructs and rotates a 3x3
fixed-point basis, publishes that basis to the collision globals, derives
component-wise endpoint bounds at offsets `+0x18–+0x2c`, calls `0x00462490`,
and stores the resulting word at `+0x8a`. The complete 1,046-byte function is
matching assembly with no `incbin`.

The nonvertical arithmetic is now closed. After the component deltas are
shifted down by 12 and narrowed to signed shorts, the initializer forms
`h = sx*sx + sz*sz` and `t = h + sy*sy` with 32-bit wrapping. Each positive
value is normalized by shifting its sign bit into place, taking the truncated
integer square root of `value << (lead_minus_one & 0x1e)`, and retaining both
the normalized root and the root shifted back by `lead_minus_one >> 1`. It
then computes, with signed x86 division truncating toward zero,
`xn = (sx << (half_h + 12))/root_h`,
`zn = (sz << (half_h + 12))/root_h`,
`v = (sy << (half_t + 12))/root_t`, and
`u3 = (root_h << 12)/(root_t << ((half_h-half_t) & 31))`.
The seed is `[0x1000,0,0; 0,u3,-v; 0,v,u3]`; columns
`[zn,0,xn]`, `[0,0x1000,0]`, and `[-xn,0,zn]` are multiplied through
`0x004e3130`. Its products/additions wrap as 32-bit values before the
arithmetic shift and signed-short saturation. The query basis is therefore
unscaled; linked-object tail scales at `+0x28/+0x2a/+0x2c` belong to the
later `0x004f5540` matrix operation.

The 12-record `collision-runtime-20260826-2` basis capture (including its
duplicate records) is reproduced by the native tests as nine unique
nonvertical cases plus the vertical branch.

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
