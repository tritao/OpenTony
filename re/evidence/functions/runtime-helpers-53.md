# Runtime helper cluster: transform caches and square root

Status: observed

Four compact helpers are now exact VC6 reconstructions:

- `0x004f50d0` caches three raw transform components.
- `0x004f50f0` shifts a fixed-point vector down by 12 bits into shared integer
  state.
- `0x004f5390` forwards two scalar arguments to the shared transform path.
- `0x004f53b0` clamps non-positive inputs to zero and computes the fixed-point
  square root through the original x87 conversion helper.

The absolute state stores, stack cleanup, x87 sequence, and call targets match
under `vc6-coff-text`.
