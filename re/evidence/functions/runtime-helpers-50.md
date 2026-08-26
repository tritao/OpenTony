# Runtime helper cluster: sound reactivation, trigonometry, and vector scaling

Status: observed

Four compact helpers are now exact VC6 reconstructions:

- `0x004f3940` scans the initialized sound-slot table and reactivates slots
  whose active bit is set.
- `0x004f3980` and `0x004f39b0` mask packed angles to 12 bits, evaluate the
  original x87 cosine/sine path, and tail-dispatch through the shared integer
  conversion helper.
- `0x004f40d0` shifts two three-component fixed-point vectors down by 12 bits,
  clears the output mask, and calls the shared axis-normalization helper.

The absolute global accesses, x87 operations, call targets, and control flow
match under `vc6-coff-text`.
