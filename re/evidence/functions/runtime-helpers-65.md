# Runtime helper: shutdown sequence

Status: observed

`0x004f86f0` now has an exact VC6 reconstruction. It emits the shutdown log
message, tears down the renderer/input subsystems in order, and invokes the
final shutdown gate.

The call order, literal address, stack cleanup, and return sequence match under
`vc6-coff-text`.
