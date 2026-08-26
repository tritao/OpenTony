# Runtime helper cluster: profile utilities

Status: observed

Three small helpers are now exact VC6 reconstructions:

- `0x004f6550` forwards a millisecond delay to the imported `Sleep` API.
- `0x004f6cd0` writes and flushes an INI profile value.
- `0x004f6d00` returns the two persisted profile coordinates.

The imported-call indirections, profile literals, absolute state loads, and
cleanup sequences match under `vc6-coff-text`.
