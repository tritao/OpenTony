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

`Math_Vector3Add` at `0x004ca9f0` also has a matching C++ reconstruction. The
pinned Visual C++ 6.0 SP3 compiler emits all 48 retail bytes, including seven
alignment NOPs, from `match/cpp/Math_Vector3Add.cpp` with `/O2 /GX- /GR-`.
Ghidra recovered the three in-place additions but modeled the function as
`void`; exact recompilation establishes that returning `*this` accounts for the
retail `mov eax, ecx` without changing the observed mutations.
The split manifest retains the reviewed NASM module as the full-image rebuild
oracle and records the C++ source and `vc6-coff-text` matching strategy.

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

The compact signed-16-bit vector family begins nearby:

- `0x004cad60–0x004cad71` masks all three words with `0x0fff`.
- `0x004cad80–0x004cadbd` zeros each word when its signed value is in the
  inclusive range `[-1, 1]`.
- `0x004cae10–0x004cae2f` adds three source words into a destination in place.
- `0x004cae30–0x004cae4f` performs the corresponding subtraction.

Ghidra classified the first two ranges as safe proposals, and they were
accepted together through the tracked-only transactional batch. The add range
was instruction-aligned but marked `unknown-adjacent`; Ghidra missed the
adjacent subtract function. Those two boundaries were therefore split
explicitly from their observed instruction/return ranges instead of bypassing
the safe-proposal policy. All four assembly modules exactly match their retail
bytes; padding remains in separate raw fragments.
