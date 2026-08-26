# Runtime helper cluster: input flag accessors

Status: observed

`0x004e41b0` is an exact VC6 SP3 reconstruction of the indexed input-flag
test. It masks the caller-supplied index to one byte, reads the table at
`0x006a43e4`, and returns its high bit. The source uses VC6 inline assembly so
the stack-frame-free byte operations and `shr al, 7` match the original body.

`0x004e42b0` is an exact VC6 SP3 reconstruction of the adjacent input-state
setter; it stores its argument at `0x0054ace0`.

Both modules are promoted to `status: cpp` with `vc6-coff-text` matching.
