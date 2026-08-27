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

## Short-basis boundary fixture

The compact [collision-query-init-boundaries fixture](../fixtures/collision-query-init-boundaries.json)
records a controlled retail run of `0x004624d0` at build
`f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669`. At the
entry breakpoint, the probe replaced the second endpoint with the first
endpoint plus each selected Q4 delta, then captured `q+0x44`, the nine signed
shorts at `q+0x48`, `q+0x89`, and the query generation stamp at return. The
injected endpoint words and both basis/scratch globals were restored after
every case; the retail-produced query outputs were left intact for the
caller.

These observations discriminate the competing constructions:

- Proportional `(1,2,3)` and `(2,4,6)` deltas produce the identical basis
  `[3885,0,-1295; -693,3461,-2077; 1094,2189,3282]`; only the integer total
  length changes from `3` to `7`. This confirms normalization through the
  shared integer-magnitude path, not an unnormalized cross product.
- Sign changes alter the expected signed/rounded short components. The
  asymmetric cases preserve the same horizontal magnitudes while exposing
  one-unit truncation differences such as `1094/-1095` and `-2077/2076`.
- Horizontal-zero is a dedicated branch, including the exact zero vector:
  `(0,0,0)` and `(0,+1,0)` both produce
  `[4096,0,0; 0,0,-4096; 0,4096,0]` with direction flag `1`; `(0,-1,0)`
  flips the final 2x2 signs and clears the flag. This rules out normalizing a
  zero vector or reusing the prior basis.
- The horizontal X-axis and signed-short boundary cases produce the same
  axis basis with the sign determined by the signed post-shift component,
  including the asymmetric `-32768` case.

The runtime result confirms the decompiled integer construction and its
dedicated horizontal-degenerate branch. It does not by itself assign final
native matrix names or prove behavior outside the tested signed-short and
small-delta boundaries.
