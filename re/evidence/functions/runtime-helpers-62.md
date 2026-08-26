# Runtime helper: custom-level filename normalization

Status: observed

`0x004f6400` now has an exact VC6 reconstruction. It composes the configured
level directory, optional custom-level prefix, and caller name into the local
path buffer, normalizes the extension, logs the delete operation, and removes
the resulting file.

The bulk string-copy loops, optional-prefix branches, absolute literals, and
call sequence match under `vc6-coff-text`.
