# Renderer

Status: raw camera/presentation handoff is available; a platform renderer is
still missing.

The native boundary now has three separate layers:

- `trg::LevelRenderSnapshot` carries PSX model faces, raw vertices, UVs,
  texture references, object placement, and trigger-driven visibility/lifecycle
  state.
- `runtime::GameplayPresentation` copies the live player/session state and
  owns one persistent `camera::CameraRuntime`.
- `camera::CameraRuntime` runs the recovered value-only camera update and
  returns the raw viewport commit. The normal level path starts in mode `1`;
  camera state remains fixed-point and no host-unit conversion is hidden in
  this boundary.

The camera facts used by the native model are the retail constructor/allocation
at `0x0040b650`/`0x004691e0`, player link at `+0x29b0`, update at
`0x0040f850`, smoothing/validation at `0x0040e090`, and the render-preparation
order beginning at `0x0046a0f0`. The retail present boundary is
`0x004d0ca4` (`DirectDraw::Flip` through the wrapper at `0x004d0c30`). These
facts define the handoff order, not a claim that DirectDraw behavior has been
reimplemented.

Still required for a visible faithful level:

- convert raw PSX palettes/textures into backend resources with the retail
  CLUT/page/texture rules;
- implement camera view/projection and viewport conventions at the recovered
  fixed-point boundary;
- submit static and factory-created models with depth, ordering, culling,
  transparency, fog, and animation policy; and
- add a window, device/input loop, frame pacing, screenshot capture, and
  deterministic render snapshots for comparison.

Record observations with build identity, addresses, and evidence confidence. Prefer links into `re/evidence/` for claims that should survive refactors.
