# Runtime helper: parser token decoder

Status: observed

`0x004fac60` now has an exact VC6 reconstruction. It refills look-ahead,
decodes literal and back-reference tokens, updates the parser's token and
length statistics, and flushes completed output ranges.

The refill guards, token-table updates, hash refreshes, flush helper calls,
loop transitions, and mode-dependent return paths match under
`vc6-coff-text`.
