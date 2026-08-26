# Runtime helper cluster: keyboard update and linked insertion

Status: observed

Four more compact routines are exact VC6 SP3 matches:

- `0x004ec5c0` acquires the keyboard, reads a 0x100-byte state buffer, retries
  after the lost-input error, and reports failures through the original logger.
- `0x004e9070` and `0x004e90d0` wrap the shared allocation/cleanup helpers for
  the two input modes and update the global mode state to `3` and `4`.
- `0x004ef320` inserts an input node into the selected linked-list chain,
  updates the aggregate count, and sets the low flag for the special path.

The retry/error branches, absolute mode globals, and linked-list register
layout are preserved under `vc6-coff-text` exact matching.
