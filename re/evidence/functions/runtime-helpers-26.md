# Runtime helper cluster: mode file operations

Status: observed

Three immediate callees of the previously matched mode wrappers are now exact
VC6 SP3 reconstructions:

- `0x004e8f90` validates the requested flags, checks the current file state,
  performs the mode-5 operation, and records global mode `5`.
- `0x004e9000` performs the corresponding mode-6 operation and records global
  mode `6`.
- `0x004e7b60` dispatches a valid descriptor through the mode-2 callback,
  while preserving the original diagnostic paths for invalid descriptors.

The signed flag normalization, callback calls, absolute mode globals, and
error branches match under `vc6-coff-text`.
