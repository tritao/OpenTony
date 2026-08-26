# Runtime helper cluster: DirectInput enumeration wrappers

Status: observed

The four neighboring DirectInput wrappers are now split and matched exactly:

- `0x004ec000` enumerates attached joysticks with callback `0x004ec100` and
  mode `4`, logging failures at source line `0x97`.
- `0x004ec040` enumerates force-feedback joysticks with callback `0x004ec160`
  and mode `0x101`, logging failures at line `0xaa`.
- `0x004ec080` enumerates attached devices with callback `0x004ec100` and
  mode `0`, logging failures at line `0xbf`.
- `0x004ec0c0` performs the generic device enumeration with mode `0`, logging
  failures at line `0xd3`.

Each wrapper preserves the original vtable call, error-path callback, and
success/failure return bytes under `vc6-coff-text` matching.
