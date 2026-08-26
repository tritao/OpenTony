# Runtime helper: z-buffer descriptor append

Status: observed

`0x004f7980` now has an exact VC6 reconstruction. It bounds-checks the active
descriptor count, copies an eight-word z-buffer record into the renderer list,
logs the descriptor value, and increments the list count.

The stride arithmetic, bulk copy, call target, and `ret 8` cleanup match under
`vc6-coff-text`.
