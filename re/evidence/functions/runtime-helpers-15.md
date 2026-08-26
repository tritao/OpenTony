# Runtime helper cluster: allocation and linked-list helpers

Status: observed

Six more compact routines match the original VC6 SP3 bodies:

- `0x004ecf50` releases the object at `+0x58`, first invoking its callback,
  then its two vtable cleanup methods, and clears the member.
- `0x004ed490` is the flag-aware wrapper around `0x004ed4b0`.
- `0x004ed4c0` conditionally dispatches the `+8` object cleanup when the
  object's `+0x1b0` flag is set.
- `0x004ef2d0` aligns and advances the `0x006a768c` arena pointer, returning
  zero on invalid alignment or overflow.
- `0x004ef300` advances the `0x006a7688` arena pointer by `0x10`, returning
  zero past its limit.
- `0x004ef380` inserts a node into the doubly linked list headed at the first
  object argument.

Absolute globals, vtable calls, and the original branch layout are retained
with VC6 inline assembly where necessary; every module is an exact
`vc6-coff-text` match.
