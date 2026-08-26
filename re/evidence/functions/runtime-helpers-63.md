# Runtime helper: custom-level save path

Status: observed

`0x004f62c0` now has an exact VC6 reconstruction. It composes the save path
with the optional custom-level prefix, invokes the profile/logging and file
save helpers, and reports the original error path when the file cannot be
opened.

The bulk-copy loops, branch layout, absolute error state, and call sequence
match under `vc6-coff-text`.
