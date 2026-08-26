# Runtime helper: fixed-point box construction

Status: observed

`0x004f4130` now has an exact VC6 reconstruction. It converts the high and
low halves of a fixed-point box into integer and sub-voxel bounds, applies the
original two-unit padding, and mirrors each axis when the corresponding flip
bit is set.

The stack layout, signed shifts, byte-mask tests, and endpoint writes match
under `vc6-coff-text`.
