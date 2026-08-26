# Runtime helper: parser stream reset

Status: observed

`0x004fa8f0` now has an exact VC6 reconstruction. It clears the active
stream buffer, derives the current dimension table values, resets the parser
cursor and mode fields, and restores the stream state to its initial mode.

The buffer clearing loops, table lookups, state writes, register preservation,
and return sequence match under `vc6-coff-text`.
