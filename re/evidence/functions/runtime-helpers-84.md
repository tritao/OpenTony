# Runtime helper: parser conversion wrapper

Status: observed

`0x004fbb20` now has an exact VC6 reconstruction. It forwards the two
conversion arguments to the shared helper and preserves the original stack
cleanup and return sequence.

The argument loads, call target, cleanup, and epilogue match under
`vc6-coff-text`.
