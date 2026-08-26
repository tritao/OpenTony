# Runtime helper cluster: transform axis normalization and loads

Status: observed

Three transform-state helpers are now exact VC6 reconstructions:

- `0x004f4050` normalizes X/Y/Z axis ordering, toggles the corresponding
  permutation bits, and records signed extents.
- `0x004f4fe0` loads the full transform and signed translation into shared
  math globals.
- `0x004f56c0` performs the same fixed-point load and tail-dispatches to the
  shared transform evaluator.

The swap operations, mask updates, absolute stores, and tail jump match under
`vc6-coff-text`.
