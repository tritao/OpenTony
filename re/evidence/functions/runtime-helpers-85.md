# Runtime helper: stream checksum update

Status: observed

`0x004fbb50` now has an exact VC6 reconstruction. It updates the split
checksum state over bounded chunks, handles aligned 16-byte blocks and tail
bytes, reduces both accumulators modulo `0xfff1`, and returns the packed
checksum value.

The unrolled byte accumulators, chunk loop, modulo reductions, null/empty
guards, and register cleanup match under `vc6-coff-text`.
