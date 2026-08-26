# Runtime helper cluster: DirectInput enumeration callbacks

Status: observed

The callback bodies behind the enumeration wrappers are now split and matched
exactly:

- `0x004ec100` copies an attached-device descriptor into the next joystick
  slot, increments the count up to fifteen entries, and marks the joystick
  presence byte when the device type nibble is four.
- `0x004ec160` performs the same descriptor copy for force-feedback devices,
  using the adjacent presence byte.
- `0x004ec6e0` initializes the mouse object, calls the state reset and geometry
  helpers, and returns `this` with the original vtable and default width.

The callbacks preserve VC6's `rep movsd`/`rep movsb` copy loop and calling
convention; all three use `vc6-coff-text` exact matching.
