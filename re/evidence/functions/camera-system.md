# Camera system and rendered-frame boundary

Status: normal follow-camera ordering and the game-owned present boundary are
confirmed for the recorded PC PE32 build. Non-default camera modes, dynamic
axis calibration, and the platform renderer remain incomplete.

Build: THPS2 PC PE32/i386, SHA-256
`f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`.

## Confirmed ordering

The active level path is:

```text
Game_LevelLoop 0x0046a3a0
  -> gameplay/object update 0x00469de0
  -> camera update 0x0040f850
     -> smoothing/validation 0x0040e090
     -> mode-specific handler
  -> render preparation 0x0046a0f0
     -> world traversal 0x00467c90
     -> view/projection 0x0045e8e0
     -> skater/scene submit 0x0045f530
     -> object/model list 0x00460a90
  -> buffer/present path 0x0042ffc0 -> 0x0042fd20 -> 0x004d0c30
  -> DirectDraw Flip callsite 0x004d0ca4
```

The camera is therefore gameplay-side state consumed by render preparation;
it is not a late renderer helper. The present callsite is the indirect
DirectDraw surface operation at vtable offset `+0x2c`, reached through the
game-owned wrapper at `0x004d0c30`. The settled Warehouse path showed one
camera update per displayed frame after startup camera calls, so the native
renderer clock should be tied to the actual backend present boundary rather
than the `0x004f7ce0` message pump.

## Camera object facts

- Constructor: `0x0040b650`; allocation size: `0x674` at `0x004691e0`.
- Vtable: `0x005184b8`; player link: `player +0x29b0`.
- Update method: `0x0040f850`; smoothing/validation: `0x0040e090`.
- Normal-follow mode is mode `1`; mode `23` is point-camera dispatch,
  mode `24` is death-camera dispatch, and mode `25` is alternate follow.
- The normal state includes current/target/previous transforms at
  `+0x444..+0x47c`, effect fields at `+0x4f2..+0x500`, mode at `+0x504`,
  update tick at `+0x510`, and alternate-follow state at `+0x5ec/+0x5f0`.
- The low word of `+0x40c` is written to the viewport record at
  `DAT_00563a38 +0x0e`; `+0x510` increments once per camera update.

These offsets are represented as raw fixed-point/value fields in
`src/camera/camera_system.hpp`. `CameraRuntime` owns the persistent state and
`GameplayPresentation` joins it to `GameplaySession` after the gameplay step.
No host-unit conversion, texture policy, or renderer ownership is inferred at
this boundary.

## Remaining falsifiers and seams

- Calibrate the complete view/projection and viewport conversion against the
  transformed-vertex traces before choosing backend units.
- Recover non-default mode producers, camera effects, clipping, and dynamic
  framing inputs where the current C++ input records remain caller-owned.
- Reproduce model transforms, face winding, CLUT/page texture lookup,
  depth/transparency, fog, animation, HUD, and the actual window/present loop
  in a backend that consumes `LevelRenderSnapshot`.

Do not reuse these addresses for a different executable identity without a new
capture.
