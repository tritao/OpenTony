# Runtime helper: transformed collision record

Status: observed

`0x004f5ca0` now has an exact VC6 reconstruction. It loads the two packed
records into shared collision state, runs the three orientation helpers,
writes the transformed record, then derives the translated extent words from
the original reference record.

The 16-bit state transfers, absolute globals, call targets, signed subtraction,
and output layout match under `vc6-coff-text`.
