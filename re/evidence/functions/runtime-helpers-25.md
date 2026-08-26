# Runtime helper cluster: input capture teardown

Status: observed

`0x004e4a60` is now split and byte-exact. It tears down the four input objects
at `0x006a43e0`, `0x006a3f20`, `0x006a42e8`, and `0x006a4500`, then transfers
control through the original indirect `ReleaseCapture` import thunk at
`0x005182b0`.

Ghidra could not recover the final jump-table target, so the fixed indirect
jump is retained in VC6 inline assembly; the complete 46-byte module matches
under `vc6-coff-text`.
