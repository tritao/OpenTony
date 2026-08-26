# Runtime helper: parser resource-stream initializer

Status: observed

`0x004fa710` now has an exact VC6 reconstruction. It allocates the parser
resource stream, clones the template record and its variable-sized buffers,
initializes the derived offsets and embedded pointers, and routes failed
initialization through the parser resource finalizer.

The allocation calls, copy loops, state writes, cleanup call, error returns,
and success epilogue match under `vc6-coff-text`.
