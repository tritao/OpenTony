# Runtime helper: timer accumulation

Status: observed

`0x004f5ff0` now has an exact VC6 reconstruction. It samples the multimedia
timer, initializes the previous-tick state, converts elapsed milliseconds to
the game tick scale, and advances the active clocks unless either pause gate is
set.

The integer multiply/divide sequence, absolute timer globals, branch layout,
and state update match under `vc6-coff-text`.
