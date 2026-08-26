# Runtime helper: parser look-ahead refill

Status: observed

`0x004fab00` now has an exact VC6 reconstruction. It compacts the input
window when look-ahead is exhausted, adjusts buffered token offsets, invokes
the refill helper, and updates the parser's two-byte look-ahead hash.

The compaction loops, offset normalization, refill call, threshold branches,
state writes, and loop-back path match under `vc6-coff-text`.
