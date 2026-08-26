# Runtime helper cluster: alignment stubs

Status: observed

The manually verified alignment-separated leaves around `0x004e8a10` and
`0x004e8b90` are now split and matched under VC6 SP3:

- `0x004e8a10` and `0x004e8a70` are empty `void` stubs.
- `0x004e8a20`, `0x004e8a50`, `0x004e8a60`, `0x004e8a80`, `0x004e8a90`, and
  `0x004e8aa0` return zero.
- `0x004e8a30` returns its second stack argument, while `0x004e8a40`
  returns its first argument.
- `0x004e8b90`, `0x004e8ba0`, `0x004e8bc0`, `0x004e8bd0`, and `0x004e8be0`
  return zero; `0x004e8bb0`, `0x004e8bf0`, `0x004e8c40`, `0x004e8c50`,
  `0x004e8c60`, and `0x004e8c70` are empty `void` stubs.
- `0x004e8c80` returns a zero `short`, preserving the original `xor ax,ax`
  encoding.

Every promoted source matches the original function bytes; trailing NOPs are
the VC6 COFF section-alignment suffix accepted by the comparator.

The byte-pattern audit also found and matched zero-return leaves at
`0x004e87b0`, `0x004e89b0`, `0x004e89c0`, `0x004e89d0`, `0x004e89e0`,
`0x004e8af0`, `0x004e8b40`, `0x004e8b50`, and `0x004e8b60`.
