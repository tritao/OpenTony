# Runtime helper cluster: sound ID duration lookup

Status: observed

`0x004f3720` is now split and byte-exact. It validates the sound system,
indexes the bank from the packed identifier, queries the channel vtable,
preserves the fatal diagnostic path, and converts the returned duration with
the original 64-bit x87/scaled fixed-point sequence.

The bank indexing arithmetic, temporary stack storage, scale constants, and
failure tail match under `vc6-coff-text`.
