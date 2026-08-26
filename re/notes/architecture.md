# Architecture

Status: native level-side vertical slice established; full engine pending.

The current C++ dependency direction is deliberately one-way:

```text
PRE/PSX bytes
    -> bounded asset readers/catalogs
    -> LevelRuntime (TRG + scene + collision + resource bindings)
    -> LevelFrameScheduler/FixedStepDriver (input history, tick ordering)
    -> player state/response
-> render snapshot -> camera/renderer/audio observers
```

`LevelRenderSnapshot` carries the confirmed object/model submission data plus
raw pickup visual, motion, and lifecycle words. It is still an observation
boundary: camera transforms, texture upload, visibility/fog, animation
sampling, and final presentation remain backend policy until their retail
callers are identified.

`src/trg/` owns script decoding and stateful gameplay-facing mutations.
`src/assets/` owns file formats and asset-derived geometry. `src/runtime/`
contains cross-subsystem frame/input contracts and the recovered position
commit behavior. `PsxPositionCollisionProbe` is the current adapter from the
asset-derived collision world into that commit path. The renderer and physics
response must consume these interfaces rather than enter the TRG decoder or
reinterpret file offsets.

The retail loop evidence is in
[`re/evidence/functions/game-loop.md`](../evidence/functions/game-loop.md):
message/timing work, input polling/action history, gameplay callback, timing
and presentation. The native scheduler currently models the input-history,
level-tick, and downstream-observer portion; platform message pumping,
player response, and presentation remain unimplemented. See
`re/notes/cpp-recreation-roadmap.md` for the prioritized completion path.
