# Skater position write path

Status: complete matching assembly; subsystem analysis in progress

The Ghidra body for `0x00496060–0x00496276` is instruction-aligned. Its only
proposal risk was `unknown-adjacent`; static bytes show that the following
`0x00496276–0x00496280` range is ten NOP padding bytes, so the function was
split explicitly without absorbing that padding.

The function uses `ECX` as the player/skater object, receives proposed X/Y/Z
words on the stack, and returns with `ret 0x0c`. When the field at object offset
`0x3200` is nonzero, it writes those values directly to offsets `+0x08`,
`+0x0c`, and `+0x10`.

When `+0x3200` is zero, the function saves and sets global `0x00567c7c`, builds
a local collision-query structure, and repeatedly calls `0x004624d0` and
`0x00466090`. A zero result at local stack offset `0x7c` accepts the current
candidate. It tests seven combinations in this order: proposed XYZ; current X;
current Z; current Y; current Y/Z; current X/Y; and current X/Z. If none is
accepted it falls back to current XYZ. The selected position is committed and
the global guard restored. Exact role names for the two callees and result
field remain inferred.

The matching module expresses the complete 534-byte function as reviewed NASM;
it contains no `incbin`. The module and full PE remain byte-identical.
