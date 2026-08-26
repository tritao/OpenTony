# Runtime helper cluster: mouse event and viewport setup

Status: observed

Three additional mouse helpers are exact VC6 SP3 matches:

- `0x004ec860` creates the event used for mouse notifications, stores its
  handle, and passes it to the device vtable, logging either failure path.
- `0x004ec8c0` submits the original 0x14-byte buffered-input format to the
  mouse device and logs failures at line `0x2c0`.
- `0x004ec930` clamps the requested viewport bounds against the object's
  current dimensions while preserving the original stack/register layout.

The imported `CreateEventA` call, vtable dispatches, local input-format
structure, and clamp branch layout all match under `vc6-coff-text`.
