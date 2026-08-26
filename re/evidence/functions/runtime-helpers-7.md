# Runtime helper cluster: mode stubs

Status: observed

The split range around `0x004e4be0` adds three exact VC6 SP3 leaves:

- `0x004e4be0` and `0x004e4ce0` are empty `void` stubs.
- `0x004e4c30` returns an 8-bit zero using the original `xor al,al`
  instruction form.

The adjacent `0x004e4bf0` mode setter is semantically reconstructed but stays
raw because the compiler emits `dec`-flag branching that the straightforward
C++ forms do not reproduce exactly. The larger callback and state-update
routines in this range are also split for later matching.
