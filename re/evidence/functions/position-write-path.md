# Skater position write path

Status: complete matching assembly; subsystem analysis in progress

The Ghidra body for `0x00496060–0x00496276` is instruction-aligned. Its only
proposal risk was `unknown-adjacent`; static bytes show that the following
`0x00496276–0x00496280` range is ten NOP padding bytes, so the function was
split explicitly without absorbing that padding.

The function uses `ECX` as the player/skater object, receives proposed X/Y/Z
words on the stack, and returns with `ret 0x0c`. The field at object offset
`0x3200` is therefore behaviorally resolved as a direct-commit gate: when it is
nonzero, the function writes those values directly to offsets `+0x08`,
`+0x0c`, and `+0x10`. The selected ordinary grounded and air-to-landing paths
take the zero/collision branch. The reviewed split has no writer for this
field, so its producer remains open outside the selected paths.

When `+0x3200` is zero, the function saves and sets global `0x00567c7c`, builds
a local collision-query structure, and repeatedly calls `0x004624d0` and
`0x00466090`. A zero result at local stack offset `0x7c` accepts the current
candidate. It tests seven combinations in this order: proposed XYZ; current X;
current Z; current Y; current Y/Z; current X/Y; and current X/Z. If none is
accepted it falls back to current XYZ. The selected position is committed and
the global guard restored. Exact role names for the two callees and result
field remain inferred.

The matching module expresses the complete 534-byte function as reviewed NASM;
it contains no `incbin`. The module and full PE remain byte-identical. The
compact [player-frame integration fixture](../fixtures/player-frame-integration.json)
records the fixed-point caller values and the native candidate ordinals used
by the two selected frame paths.
