# Runtime helper: z-buffer initialization

Status: observed

`0x004f7a70` now has an exact VC6 reconstruction. It clears the renderer
descriptor array, initializes the z-buffer dimensions and format, and registers
the original callback through the renderer vtable.

The stack layout, descriptor constants, vtable call, and error path match under
`vc6-coff-text`.
