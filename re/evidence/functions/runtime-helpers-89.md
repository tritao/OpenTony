# Runtime helper: bitstream finalizer

Status: observed

`0x004fbe40` now has an exact VC6 reconstruction. It emits the fixed end
marker, handles split and unsplit bit-buffer boundaries, updates output
alignment, and invokes the encoder finalization helper for each record phase.

The repeated bit emission paths, absolute table loads, alignment arithmetic,
helper calls, and final state transition match under `vc6-coff-text`.
