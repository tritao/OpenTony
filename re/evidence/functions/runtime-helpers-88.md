# Runtime helper: token bit writer

Status: observed

`0x004fbd80` now has an exact VC6 reconstruction. It appends a variable-width
token to the bit buffer, emits completed bytes, advances the output alignment,
and forwards the resulting payload record to the encoder helper.

The bit shifts, buffer writes, alignment calculation, call arguments, cleanup,
and register-preserving epilogue match under `vc6-coff-text`.
