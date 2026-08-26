# Runtime helper: parser resource finalization

Status: observed

`0x004fa650` now has an exact VC6 reconstruction. It validates the parser
record type, releases each optional resource through the owner vtable, frees
the header, clears the parser pointer, and returns the original type-derived
status code.

The type checks, release order, vtable calls, pointer clearing, and status
calculation match under `vc6-coff-text`.
