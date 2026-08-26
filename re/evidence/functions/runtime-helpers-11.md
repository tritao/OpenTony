# Runtime helper cluster: media and object lifecycle

Status: observed

The next seven safe module boundaries were split and checked against Ghidra's
decompilation. All seven now have exact VC6 SP3 reconstructions:

- `0x004e3e50` forwards a callback-style continuation to `0x00500260`.
- `0x004e72d0` updates Bink volume and pan through the two imported callbacks,
  including the signed `0x200` volume scaling adjustment.
- `0x004e74f0` forwards `&DAT_006a72bc` to `0x004e9b80`.
- `0x004e78e0` returns the first empty slot in the ten-byte table at
  `0x006a6d10`, or `-1`.
- `0x004ebf70`, `0x004ec740`, and `0x004eccc0` initialize their respective
  vtable pointers and tail-call their cleanup paths when the corresponding
  member is nonzero.

The small direct-call and conditional-jump bodies use VC6 inline assembly for
the fixed-address control-flow encodings; each matches with the
`vc6-coff-text` strategy.
