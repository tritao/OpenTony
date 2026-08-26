# Runtime helper cluster: input accessors

Status: observed

The safe proposal range around `0x004e4190`–`0x004e42ba` is split. Four
leaf routines have exact VC6 SP3 C++ reconstructions:

- `0x004e4230` sign-extends the byte at object-relative address
  `0x52f3f0`.
- `0x004e4240` returns zero.
- `0x004e4250` indexes the integer table at `0x52eff0`.
- `0x004e4260` searches the fixed table beginning at offset `0x6c`, returns
  the matching slot index, and returns `-1` after offset `0xec`.

The neighboring event-loop and callback wrappers remain split raw while their
external-call/register encodings are being matched.
