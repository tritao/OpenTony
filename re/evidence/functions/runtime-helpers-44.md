# Runtime helper cluster: render bounds and sentinel grid

Status: observed

The render/sound initialization helpers are now exact VC6 reconstructions:

- `0x004f2240` clamps the requested rectangle against the configured global
  bounds and clears it when the result is empty.
- `0x004f2720` resets the bounds, initializes the shared state fields, and
  fills the 64×64 grid with the original `0x801f`/`0xffff` sentinels.

The absolute global stores, boundary comparisons, nested loop branches, and
sentinel pattern match under `vc6-coff-text`.
