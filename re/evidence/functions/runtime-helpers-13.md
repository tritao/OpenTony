# Runtime helper cluster: object cleanup and construction

Status: observed

The six newly split lifecycle routines are exact VC6 SP3 reconstructions:

- `0x004ebf50` and `0x004ec4c0` call their base cleanup, conditionally free
  the object when the low flag bit is set, and return `this` with the original
  `__thiscall` cleanup convention.
- `0x004ebfe0` releases the secondary object at offset `+4` through its vtable
  and clears that member.
- `0x004ec4a0` installs vtable `0x00519a1c`, clears the byte at `+0x10c`, and
  clears the word at `+0x108`.
- `0x004ec4e0` installs the same vtable and tail-jumps to `0x004ec590` when
  the `+0x108` member is present.
- `0x004ec830` releases the object at offset `+0x28` through both of its
  vtable operations and clears the member.

Fixed-address calls, vtable dispatches, and constructor/destructor calling
conventions are preserved with VC6 inline assembly; all six modules match
under `vc6-coff-text`.
