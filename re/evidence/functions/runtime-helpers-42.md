# Runtime helper cluster: MMX stream packing

Status: observed

The packed-data helpers are now exact VC6 reconstructions:

- `0x004ef1a0` applies the original MMX shifts/addition and writes packed
  16-bit samples for each source vector.
- `0x004ef210` fills an aligned destination with repeated 64-bit MMX values,
  handling 2/4/6-byte alignment and short tails exactly.

The nested frame setup, MMX register sequence, alignment branches, explicit
NOP layout, and loop counters match under `vc6-coff-text`.
