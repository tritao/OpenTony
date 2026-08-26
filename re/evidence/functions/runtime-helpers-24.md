# Runtime helper cluster: startup text and command dispatch

Status: observed

Three early utility routines now match VC6 SP3 exactly:

- `0x004e0dd0` selects the initial setup path based on the object state and
  updates the shared startup result.
- `0x004e0ee0` copies the startup text into the global command buffer and marks
  it ready.
- `0x004e1490` copies a command string into the global buffer and posts the
  original `WM_USER+1` notification through `PostMessageA`.

The bounded `rep movsd`/`rep movsb` copies, fixed-address handlers, and imported
message call all use `vc6-coff-text` exact matching.
