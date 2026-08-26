# Runtime helper: D3D device record append

Status: observed

`0x004f78f0` now has an exact VC6 reconstruction. It bounds-checks the device
record list, copies the fixed descriptor, allocates and copies the device name,
logs the resulting pointer, and increments the list count.

The record stride, bulk-copy loops, allocation call, diagnostic call, and
`ret 0x10` cleanup match under `vc6-coff-text`.
