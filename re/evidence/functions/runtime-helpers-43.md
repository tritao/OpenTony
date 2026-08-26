# Runtime helper cluster: flagged float interpolation

Status: observed

`0x004ef820` is now split and byte-exact. It computes the interpolation
factor from the source/target scalar fields, updates the vector and optional
position fields selected by the flag bits, and preserves the original x87
stack cleanup path.

The comparison status test, x87 arithmetic order, field offsets, and flag
branches match under `vc6-coff-text`.
