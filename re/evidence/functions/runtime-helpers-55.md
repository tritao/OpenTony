# Runtime helper: packed collision transform

Status: observed

`0x004f5a70` now has an exact VC6 reconstruction. It loads the packed matrix
and extent words into shared collision state, runs the orientation, scale, and
translation helpers in sequence, and writes the transformed record back to its
caller-provided buffer.

The sign extensions, 16-bit temporaries, absolute state exchanges, call
targets, and output ordering match under `vc6-coff-text`.
