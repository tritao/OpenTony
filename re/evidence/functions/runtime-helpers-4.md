# Runtime helper cluster: state stubs

Status: observed

The next instruction-safe range adds exact VC6 SP3 reconstructions for the
following leaves:

- `0x004e8b80` and `0x004e8c00`, `void` stubs containing only `ret`.
- `0x004e8c10`, `0x004e8c20`, and `0x004e8c30`, zero-return stubs.
- `0x004e8c90`, `0x004e8ca0`, `0x004e8cb0`, and `0x004e8cc0`, empty `void`
  stubs.
- `0x004e8cf0`, which clears the byte state at `0x6a74bc`, stores state `2`
  at `0x6a74c0`, and returns zero.

The neighboring `0x004e8cd0` and `0x004e8d10` routines have been decompiled
and split, but remain raw because VC6 chooses a different register sequence
for the fixed-address stores and output pointers.
