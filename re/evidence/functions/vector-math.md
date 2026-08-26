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
