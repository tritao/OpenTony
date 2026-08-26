# Runtime helper cluster

Status: observed

The raw `.text` span after the vector operators has been split at Ghidra's
instruction-safe function boundaries. The following leaf routines have
matching VC6 SP3 C++ reconstructions under `/O2 /GX- /GR-`:

- `0x004cb320` clears the two queue cursors at `0x56e338` and `0x56e438`.
- `0x004cb380` reads one two-word queue record, advances the read cursor
  modulo 30, and writes both output values.
- `0x004cb420` and `0x004cb4b0` initialize two pairs of state globals to `-1`.
- `0x004cbc90` clears the two skater-state pointers used by the teardown path.
- `0x004cbf70` flushes a pending four-byte queue value and resets its count to
  `0x20`.
- `0x004ce280` is an empty `__thiscall` method with one stack argument; VC6
  emits the retail `ret 4`.
- `0x004ce290` returns the byte flag at object offset `0xe4` as an integer.
- `0x004d0a20` counts set bits in an unsigned word.
- `0x004d0a40` counts trailing zero bits, returning zero for a zero input.

The nearby routines at `0x004cb330`, `0x004cb360`, `0x004cb580`, and
`0x004cde70` have been decompiled and split but are not marked `cpp`: their
current high-level forms are semantically correct, while VC6 chooses a
different register or branch encoding for the fixed-address global accesses.
