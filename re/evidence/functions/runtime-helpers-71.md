# DirectX import thunks

Status: observed

The retail import thunks at `0x004f88dc` (`DirectDrawEnumerateExA`) and
`0x004f88e2` (`DirectInputCreateA`) now have exact VC6 reconstructions. Each
forwards directly through its recorded IAT slot.

Both six-byte indirect jumps match under `vc6-coff-text`.
