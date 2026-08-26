# Skater position write path

Status: hybrid matching assembly; subsystem analysis in progress

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
`0x00466090`. It tries combinations of proposed and current coordinates before
committing the selected XYZ values and restoring the global guard. Exact role
names for the two callees and the query result field remain inferred.

The matching module reconstructs the 59-byte prologue/direct-commit path and
preserves the collision-resolution core from module offset `0x3b`. The complete
534-byte module and full PE remain byte-identical.
