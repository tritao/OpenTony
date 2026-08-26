# Runtime helper cluster: PKR archive unload

Status: observed

`0x004e9b80` is now split and byte-exact. It compares the requested archive
name with the loaded name, reports mismatches, copies the current archive
path, frees both PKR buffers, and clears their ownership globals.

The pairwise name comparison, copy loops, fixed diagnostics, allocator calls,
and global clears match under `vc6-coff-text`.
