# Runtime helper cluster: joystick and mouse acquisition

Status: observed

Four acquisition wrappers now match VC6 SP3 exactly:

- `0x004ed380` acquires the joystick at `+0x58`, logs failure at line `0x53e`,
  and marks byte `+0xec` on success.
- `0x004ed3c0` unacquires the same device, logs line `0x54f`, and clears the
  acquired byte.
- `0x004ecbf0` acquires the mouse at `+0x28`, resets its state through
  `0x004ec760`, logs line `0x375` on failure, and sets byte `+0x50`.
- `0x004ecc30` performs the corresponding mouse unacquire, logs line `0x387`,
  and clears byte `+0x50`.

The vtable dispatches, state reset calls, diagnostic callbacks, and original
success/failure register paths are preserved under `vc6-coff-text` matching.
