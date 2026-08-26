# Vector math helpers

Status: observed

The retail `.text` bytes at `0x004ca9f0–0x004caa19` add three signed
32-bit components from the stack argument vector into the vector passed in
`ECX`, return the destination in `EAX`, and use `ret 4`. Seven NOP bytes pad
the function through `0x004caa20`.

The adjacent routine at `0x004caa20–0x004caa49` has the same calling
convention and data flow but subtracts all three source components. Seven NOP
bytes pad it through the next function at `0x004caa50`.

Both matching-assembly modules reproduce their original bytes exactly before
participating in the full-image rebuild.

Four more adjacent leaf helpers use the same three-component layout:

- `0x004caa50–0x004caa72` multiplies every component by the signed scalar
  referenced by its stack argument, followed by 14 NOP bytes.
- `0x004caa80–0x004caaa4` divides every component by the signed scalar
  referenced by its stack argument, followed by 12 NOP bytes.
- `0x004caab0–0x004caad7` arithmetic-right-shifts every component by the count
  referenced by its stack argument, followed by nine NOP bytes.
- `0x004caae0–0x004cab07` left-shifts every component by the same form of shift
  count, followed by nine NOP bytes.

Their explicit module ranges extend through the padding to `0x004caa80`,
`0x004caab0`, `0x004caae0`, and `0x004cab10`, respectively. All four assembled
modules are byte-identical to the retail ranges.
