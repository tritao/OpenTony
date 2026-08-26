# Runtime helper cluster: second pass

Status: observed

The next instruction-safe `.text` spans were split into standalone modules
and checked against the original executable. The following leaf routines have
matching VC6 SP3 C++ reconstructions under `/O2 /GX- /GR-`:

- `0x004da3e0` links two nodes by writing the forward and back pointers.
- `0x004da580` returns the object field at offset `0xc`.
- `0x004dae60` stores a value at object offset `0x1c`.
- `0x004db3e0` clears the global entry count at `0x6a0664`.
- `0x004dc0b0` returns the fixed address `0x55fe10`.
- `0x004de000` clears the two global object pointers at `0x6a3a88` and
  `0x6a18a8`.
- `0x004de150` reports whether the global mode is 5, 8, or 9.
- `0x004de570` stores the current value in the global at `0x6a3a90`.
- `0x004deff0` resets the two global object records and related state flags.
- `0x004df280` zero-initializes the fixed-size object record.

Additional boundaries in the ranges around `0x004da3e0`, `0x004dab20`,
`0x004db1a0`, `0x004dc090`, `0x004de000`, `0x004de270`, `0x004de570`,
`0x004deb00`, and `0x004df1d0` remain raw modules. Several small global and
string helpers were decompiled for later matching, but were not promoted when
their VC6 register/branch encodings differed from the original bytes.
