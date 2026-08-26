# Runtime helper cluster: input device and action state

Status: observed

Five compact routines now have exact VC6 SP3 reconstructions:

- `0x004e3ce0` squares the three signed 16-bit state components, stores the
  products, and tail-calls `0x004e2130`.
- `0x004e4650` updates the low/high action bytes and combines the current
  action mask with the stored flags.
- `0x004e83e0` returns the square root converted through the original x87
  `__ftol` path for positive input, otherwise zero.
- `0x004ec630` acquires the input device at `+0x108`, logs failures through
  the original diagnostic helper, and marks the acquired byte at `+0x10c`.
- `0x004ec670` performs the corresponding unacquire operation and clears the
  acquired byte.

The x87 sequence, absolute global accesses, vtable calls, and error-path
relative calls are preserved with VC6 inline assembly; all five use the
`vc6-coff-text` exact-match strategy.
