# Runtime helper cluster: binary output buffering

Status: observed

Two parser-buffer helpers are now exact VC6 reconstructions:

- `0x004fa5d0` appends a big-endian 16-bit value to the active output buffer.
- `0x004fa600` flushes bounded pending bytes into the caller's output and
  resets the parser source pointer when drained.

The byte ordering, bounded copy, cursor updates, and cleanup paths match under
`vc6-coff-text`.
