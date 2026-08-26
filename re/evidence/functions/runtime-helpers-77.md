# Runtime helper: parser session reset

Status: observed

`0x004fa180` now has an exact VC6 reconstruction. It validates the parser
session, resets output cursors and mode fields, normalizes the record type,
clears the active resource pointer, and invokes the two parser reset helpers.

The guard branches, type normalization, state writes, call targets, and error
return match under `vc6-coff-text`.
