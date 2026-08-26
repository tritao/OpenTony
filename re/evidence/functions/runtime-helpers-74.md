# Runtime helper: standard renderer parser wrapper

Status: observed

`0x004f9e00` now has an exact VC6 reconstruction. It forwards the caller's
configuration pointers through the retail parser with the fixed standard
profile `(8, 0xf, 8, 0)`.

The argument order, constants, call target, and stack cleanup match under
`vc6-coff-text`.
