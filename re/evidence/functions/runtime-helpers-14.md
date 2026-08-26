# Runtime helper cluster: input scan and object reset

Status: observed

Four additional compact methods now match VC6 SP3 exactly:

- `0x004ec590` releases the object at offset `+0x108` through its two vtable
  methods, then clears that member.
- `0x004ec6b0` scans a NUL-terminated input sequence and reports whether any
  byte selects an entry with its high bit set in the object-relative table.
- `0x004ec720` is the flag-aware wrapper around `0x004ec740`, returning the
  original object after optional destruction.
- `0x004ec760` clears the eleven state words spanning offsets `+4` through
  `+0x4c` in the original order.

The vtable dispatches, `__thiscall` cleanup wrapper, and byte-level input loop
use VC6 inline assembly to preserve the original register/SIB encodings. All
four modules use `vc6-coff-text` exact matching.
