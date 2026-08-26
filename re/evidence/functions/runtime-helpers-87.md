# Runtime helpers: parser setup hooks

Status: observed

`0x004fbd00` and `0x004fbd10` now have exact VC6 reconstructions. The first
is the empty setup hook used by parser initialization; the second clears the
token and histogram tables and resets their associated counters.

Both return sequences, table-clear loops, state writes, and register cleanup
match under `vc6-coff-text`.
