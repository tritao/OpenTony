# Runtime helper cluster: angle and menu-state helpers

Status: observed

The six newly split functions all match the original VC6 SP3 bytes:

- `0x004e8560` and `0x004e8580` are the two angle helpers. They load integer
  inputs into the x87 stack, apply `fpatan` (with the second helper's scale
  factor), multiply by the shared conversion constant, and tail-jump to
  `__ftol`.
- `0x004e8f00` invokes the menu/state helper at `0x004e7900`, forwards its
  result to `0x004e7bc0`, and returns zero.
- `0x004e8f20` maps the result of `0x004e7500` to either zero or five.
- `0x004e8f40` stores the result of `0x004e75b0` in `0x0054b304` and returns
  five for failure or zero for success.
- `0x004e8f70` releases the stored state through `0x004e7bc0` and resets the
  global to `-1`.

The FPU sequences, fixed-address relative calls, and absolute global accesses
are encoded with VC6 inline assembly where required; all six use the
`vc6-coff-text` exact-match strategy.
