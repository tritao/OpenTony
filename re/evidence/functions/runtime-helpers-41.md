# Runtime helper cluster: joystick auto-center and effect defaults

Status: observed

The next joystick/effect helpers are now exact VC6 reconstructions:

- `0x004ed250` builds the 20-byte auto-center property block, submits it
  through the device vtable, and preserves the original diagnostic path.
- `0x004ed870` initializes the effect state block, linked substructures,
  timing defaults, and shared calibration values.

The `__thiscall` cleanup, property fields, fixed constants, and global loads
match under `vc6-coff-text`.
