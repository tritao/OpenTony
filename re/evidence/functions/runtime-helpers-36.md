# Runtime helper cluster: media path construction

Status: observed

`0x004e7500` is now split and byte-exact. It concatenates the configured
media root, separator, and requested relative path into its 256-byte local
buffer, then opens the resulting path through the original helper.

The `repne scasb` length scans, overlap-safe copy layout, stack offsets, and
open call match under `vc6-coff-text`.
