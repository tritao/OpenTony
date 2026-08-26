# Runtime helper cluster: DirectInput keyboard and mouse creation

Status: observed

The compact DirectInput initializers are now exact VC6 reconstructions:

- `0x004ec500` stores the keyboard parent, queries the device interface,
  installs the data format, and acquires it with the original error lines.
- `0x004ec790` performs the corresponding mouse setup, then invokes the
  existing mouse-mode helpers before returning the combined status.

The `__thiscall` cleanup, vtable offsets, GUID/data-format pointers, error
diagnostics, and helper calls match under `vc6-coff-text`.
