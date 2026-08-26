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

The next three routines extend the same fixed-layout vector operator family:

- `0x004cab10–0x004cab47` subtracts an arithmetic-right-shifted copy of each
  component from itself. Three byte-sized shift counts come from the stack
  argument. Nine NOP bytes pad the module through `0x004cab50`.
- `0x004cab50–0x004cab7e` compares all three components and returns one only
  when every pair is equal. Two NOPs pad it through `0x004cab80`.
- `0x004cab80–0x004cabae` is the logical inverse, returning one when any pair
  differs. Two NOPs pad it through `0x004cabb0`.

The equality routines retain the retail short conditional branches and their
exact Boolean return-path encodings. All three complete modules match the
retail bytes.

Two out-of-place operators follow the comparison helpers:

- `0x004cabb0–0x004cabe1` adds the components of two input vectors and writes
  the three results through a distinct output pointer. Fifteen NOP bytes pad
  the module through `0x004cabf0`.
- `0x004cabf0–0x004cac21` performs the corresponding subtraction and has the
  same register, output, and padding layout through `0x004cac30`.

Both 64-byte modules reproduce the retail instruction encodings and padding.

Five scalar/shift out-of-place operators occupy the next aligned ranges:

- `0x004cac30–0x004cac60` multiplies a vector by a scalar into an output.
- `0x004cac60–0x004cac90` implements the commuted scalar/vector argument form.
- `0x004cac90–0x004cacd0` divides a vector by a scalar into an output.
- `0x004cacd0–0x004cad00` arithmetic-right-shifts a vector into an output.
- `0x004cad00–0x004cad30` left-shifts a vector into an output.

The first, second, fourth, and fifth modules are 48 bytes; the divide module is
64 bytes. Each range includes its original NOP padding and matches the retail
bytes exactly.

The safe Ghidra proposal at `0x004cad30–0x004cad53` identifies a distinct
out-of-place unary operator. It negates each signed component from the `ECX`
input vector and writes the results through the stack-supplied output pointer.
Padding begins at `0x004cad53` and remains a separate raw fragment. The
35-byte matching-assembly function reproduces the retail bytes exactly.
