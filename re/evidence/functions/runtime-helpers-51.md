# Runtime helper: fixed-point endpoint measurement

Status: observed

`0x004f3f30` now has an exact VC6 reconstruction. The helper samples both
three-component fixed-point endpoints relative to a reference, invokes the
shared measurement routine for each endpoint, stores the resulting bounds and
flags, and returns the clamped quarter-scale minimum.

The signed shifts, 16-bit absolute state writes, calls, and bound clamp match
under `vc6-coff-text`.
