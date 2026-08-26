# Runtime helper cluster: effect creation and timing

Status: observed

The effect-device helpers are now exact VC6 reconstructions:

- `0x004ed7d0` creates the effect through the parent vtable, reports failures,
  and records the effect-valid flag.
- `0x004ed950` normalizes the requested rate, computes the scaled timing
  value through the x87 helper, invokes effect setup, and enables it.

The `__thiscall` cleanup, vtable offsets, fixed diagnostics, x87 scale load,
and `ret 0x10` calling convention match under `vc6-coff-text`.
