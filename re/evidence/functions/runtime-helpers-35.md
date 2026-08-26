# Runtime helper cluster: movie resource teardown

Status: observed

`0x004e6da0` is now split and byte-exact. It closes the Bink stream through
the original import thunk, releases the PCM interface through its vtable,
reports fatal release failures, clears both movie buffer arrays, and resets
the playback flag.

The import call, virtual call, fixed diagnostics, loop counts, and global
stores match under `vc6-coff-text`.
