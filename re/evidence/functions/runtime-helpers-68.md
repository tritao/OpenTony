# Runtime helper: D3D capability dump

Status: observed

`0x004f77f0` now has an exact VC6 reconstruction. It logs the selected device
capabilities, texture limits, and texture-capability bits through the original
diagnostic formatter.

The deferred stack cleanup, field offsets, capability masks, and call sequence
match under `vc6-coff-text`.
