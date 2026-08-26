# Runtime helper: parser table initialization

Status: observed

`0x004fbc80` now has an exact VC6 reconstruction. It resets parser token
tables, installs the embedded stream pointers and vtables, initializes table
sizes, and invokes the surrounding setup helpers.

The initialization stores, absolute table addresses, helper calls, stack
cleanup, and return sequence match under `vc6-coff-text`.
