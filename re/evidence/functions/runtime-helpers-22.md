# Runtime helper cluster: device creation and effect dispatch

Status: observed

Four additional methods are exact VC6 SP3 reconstructions:

- `0x004ebf10` initializes the DirectInput object, clears device state, and
  preserves the original 16-iteration initialization loop.
- `0x004ebf90` stores the requested version/flags and calls
  `DirectInputCreateA`, reporting failures at line `0x77`.
- `0x004ed830` submits the effect parameter block through the device vtable and
  reports failures at line `0x60a`.
- `0x004e9730` dispatches through the indexed callback table, optionally
  releases the supplied object, and returns the callback result.

The imported DirectInput call, callback-table addressing, and original loop and
stack layouts are preserved under `vc6-coff-text` exact matching.
