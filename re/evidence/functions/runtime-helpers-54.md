# Runtime helper: oriented box transform

Status: observed

`0x004f5540` now has an exact VC6 reconstruction. It stages the nine signed
words of a box, applies the shared orientation routine to each axis triplet,
and writes the transformed words back in the original ordering.

The 16-bit stack temporaries, absolute state exchanges, call targets, and
cleanup sequence match under `vc6-coff-text`.
