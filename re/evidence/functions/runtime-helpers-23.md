# Runtime helper cluster: startup dialog and mode dispatch

Status: observed

Two remaining early game helpers are exact VC6 SP3 matches:

- `0x004e2040` opens the startup dialog through `DialogBoxParamA` and maps its
  result to the original success/failure return constants.
- `0x004e2ef0` dispatches the three startup mode combinations to their fixed
  handlers (`0x004e3b00`, `0x004e3290`, and `0x004e39a0`).

The imported dialog call and fixed-address dispatch branches are preserved with
VC6 inline assembly under `vc6-coff-text` exact matching.
