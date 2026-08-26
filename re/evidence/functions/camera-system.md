# Camera system and rendered-frame boundary

Status: camera update ownership, fixed-point camera math, and the gameplay render/present ordering are established; projection semantics and non-default modes remain partial

Build: THPS2 PC PE32/i386, SHA-256 `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

Primary runtime capture: `build/debug/camera-live.jsonl` (`camera-live`, 240 camera observations)

## Result

The strongest current C++ recreation ordering is:

```text
Game_MainLoop 0x0041c2d0
    -> frontend/session callback
    -> Front_LaunchGameLevel 0x004544a0
    -> level loader 0x004524a0
    -> Game_LevelLoop 0x0046a3a0
        -> message/timing/input
        -> Game_GameplayUpdate 0x00469de0
            -> object scheduler 0x00480ff0 / 0x00480fa0
            -> camera vtable update 0x0040f850
                -> Camera_SmoothAndValidate 0x0040e090
                -> mode-specific camera handler
        -> render preparation 0x0046a0f0
            -> timing helper 0x00468b30
            -> Render_World 0x00467c90
                -> Render_SetViewProjection 0x0045e8e0
                -> skater/scene submission 0x0045f530
                -> object/model list 0x00460a90
        -> 0x0046a1a0
            -> double-buffer/render-now transition 0x0042ffc0
            -> 0x0042fd20 -> 0x004d0c30
            -> Render_Present 0x004d0ca4
                -> DirectDraw surface vtable slot +0x2c
                -> `ddraw_surface7_Flip@12`
```

The camera update is therefore gameplay-object processing, not a renderer helper that happens to be called late in the frame. It runs before the world traversal that consumes the camera state.

## Camera_Update boundary and ABI audit

The reviewed raw body owns `0x0040f850..0x00410376`; `0x00410377` is the
following alignment byte and the dispatch table begins at `0x00410378`. The
entry is reached through the camera/object vtable with `ECX` holding the
camera object. It has no stack parameters, returns `void`, and ends in a plain
`ret`, so its tracked ABI is `__thiscall void Camera_Update()`.

The static caller is the indirect camera/object hook at `0x00480fa0`: its
second vtable call is at `0x00480fc4`, with return address `0x00480fc7` in the
camera update body. Ghidra cannot represent that incoming reference as a
direct call because the target is selected through a vtable. The runtime
probe observed the same return address for all 240 accepted Warehouse hits.

The update body directly reads or writes these additional camera fields. The
names below are storage labels where the instruction sequence does not prove a
semantic name; widths are taken from the operand encoding.

| Offset | Access/width in `Camera_Update` | Conservative interpretation |
|---:|---|---|
| `+0x0c8` | read `u16` | constructor/level-selected raw word |
| `+0x3a4` | read `u32` | primary tripod/player link |
| `+0x3ac` | read `u8` | anchor-copy control flag |
| `+0x3b0..+0x3b8` | read/write 3 `u32` | mirrored anchor words |
| `+0x3bc` | read `u8` | tripod-position copy control flag |
| `+0x3c0..+0x3c8` | read/write 3 `u32` | primary anchor/target words |
| `+0x3dc` | read `u32` | secondary target/tripod link |
| `+0x3e0` | read `u8` | secondary-target control flag |
| `+0x3e4..+0x3ec` | read/write 3 `u32` | secondary anchor words |
| `+0x3f0..+0x3f8` | read 3 `u32` | look-at target words |
| `+0x40c` | read/write `u32`; publish low `u16` | raw viewport/framing parameter |
| `+0x410` | read/write `u16` | raw transition/count word |
| `+0x414` | read `u32` | raw viewport adjustment word |
| `+0x434`, `+0x436` | read `s16` | camera-dependent raw angle/offset words |
| `+0x444` | address passed as an embedded vector destination | current transform object header |
| `+0x448..+0x454` | read/write 4 `u32` | current Q12-like transform words |
| `+0x45c..+0x468` | read 4 `u32` | target Q12-like transform words |
| `+0x470..+0x47c` | read/write 4 `u32` | previous Q12-like transform words |
| `+0x4a9` | read/write `u8` | raw pending/state flag |
| `+0x4f2..+0x4f6` | read/write 3 `s16` | effect/shake offsets |
| `+0x4f8..+0x4fa` | read 3 `u8` | effect control bytes |
| `+0x4fc..+0x500` | read 3 `s16` | effect phase/offset words |
| `+0x504` | read/write `u32` | raw dispatch mode/state |
| `+0x510` | read/write `u32` | update/mode tick |
| `+0x5b4` | read/write `i32` | raw update accumulator |
| `+0x5b8..+0x5c0` | read/write 3 `u32` | mode-produced vector words |
| `+0x5c4..+0x5cc` | write 3 `u32` | mode-produced vector words |
| `+0x5ec`, `+0x5f0` | read/write `i32` | smoothing adjustment accumulators |
| `+0x610..+0x618` | read/write 3 `u32` | mode-produced vector words |
| `+0x61c` | read `u32` | smoothing/state-stage counter |

The `+0x444` header is observed separately in the constructor as the
embedded vector vtable `0x005184d8`; this update body uses its address as the
destination object for the effect/vector helper. The three vector blocks
remain raw integer storage in the recovered type because their complete
object semantics are not established here.

Matching status for this unit is deliberately conservative: the exact body is
isolated as `match/modules/text/text_0040f850.asm` with manifest status `raw`,
and its module comparison is byte-identical. No C++ or assembly promotion is
claimed for the exception-bearing body while the mode-specific semantics and
helper-owned fields remain incomplete.

## The real present boundary

Address: `0x004d0ca4`

Name class: semantic name, promoted from a concrete imported API callsite. `Render_Present` is not an embedded/original symbol.

Exact observed behavior:

- The callsite performs an indirect call through the DirectDraw surface object at `DAT_029da3ac`, vtable offset `+0x2c`.
- The recovered slot target is the imported `ddraw_surface7_Flip@12` operation.
- The game-owned wrapper immediately above it is `0x004d0c30`.
- The preceding buffer-state path is `0x0042ffc0` → `0x0042fd20`; its embedded diagnostics include `pDoubleBuffer->RenderNow`, `pDoubleBuffer->Rendering`, and `Both Buffers Set to Flip`.

Runtime validation from separate headless runs:

- Menu run: 120 Flip hits in 10.627298436 seconds; the convenient message-pump callsite `0x004f7ce0` hit 769 times in the same interval.
- Warehouse gameplay run: 120 Flip hits through the same `0x0042ffc0` transition.
- The Flip hit count is the displayed-frame boundary for this build. `0x004f7ce0` is only a Windows message-pump/timing sampling point and must not be used as the renderer frame clock.

Confidence: confirmed as the game-owned present boundary for the recorded DirectDraw build.

Possible falsifier: a different display mode or a future backend may bypass this DirectDraw surface; in that case the replacement must still be the last game-owned call immediately before the backend’s actual present operation.

## Loop relationship

`Game_MainLoop 0x0041c2d0` is the outer shell/session loop. Its static body owns a message pump, timing, frontend/session work, and a virtual callback at `(*piVar3 + 4)`. It does not contain a direct static call to `Game_LevelLoop` or to the DirectDraw Flip site.

`Game_LevelLoop 0x0046a3a0` is entered by the level-launch path after `0x004544a0` and `0x0046a8d0` finish loading a level. Its own loop performs the active level’s input, gameplay/object update, camera update, render preparation, and then the buffer/present path. The two addresses are different ownership layers, not two competing per-frame samplers:

- `Game_MainLoop` wraps frontend/session modes and delegates through a callback object.
- `Game_LevelLoop` owns the active level session while that callback is active.
- For recreation, use the level loop as the level runtime owner and the Flip callsite as the rendered-frame clock. Do not add a second tick merely because the outer loop also pumps messages.

Static level-loop ordering from `0x0046a3a0` is:

```text
0x004f7ce0  message/timing tick
0x004f5ff0  timing helper
0x004dae00  input/device preparation
0x004699f0  input update
0x00469de0  gameplay/object update
0x004e02d0  object/level support
0x0042fed0  render/display preparation
0x0046a0f0  render stage
0x0046a1a0  post-render buffer/present stage
0x0046a250  session/UI cleanup or transition work
```

## Camera construction and ownership

### `0x004691e0` — camera creation site

Exact observed behavior:

- Creates one or two skater/player objects depending on the runtime mode.
- Allocates `0x674` bytes for each camera.
- Calls `0x0040b650(camera, tripod, 0)`.
- Sets `camera + 0x504` to `1`.
- Stores the camera at `player + 0x29b0`.
- In two-player mode, links the player objects through `player + 0x29bc`.

Static callers/callees: called from the player/camera setup path; callee `Camera_Constructor 0x0040b650`.

Runtime evidence: Warehouse capture had `player = 0x05f39530`, `camera = 0x05f40ac8`, and the stable relationship `*(player + 0x29b0) == camera`. Absolute allocations are not stable across runs.

Confidence: confirmed for allocation size and player-to-camera ownership; semantic names for the containing C++ classes remain provisional.

Possible falsifier: a mode-specific allocator may use another camera size or owner field; repeat the allocation experiment in two-player and cutscene modes before generalizing.

### `0x0040b650` — `Camera_Constructor`

Exact static behavior:

- Installs vtable pointer `0x005184b8`.
- Stores its tripod argument at `camera + 0x3a4` and again at `camera + 0x3dc`.
- Initializes `camera + 0x3e0` to `1`.
- Initializes multiple embedded vector-like blocks with homogeneous value `0x1000`.
- Copies tripod position words from `tripod + 0x08/+0x0c/+0x10` into camera target/anchor storage.
- Uses `tripod + 0x2ccc` to choose the pad number and contains the source strings `camera.cpp` and `Bad tripod passed to camera`.

Static callers/callees: called by `0x004691e0`; calls base/vector initialization helpers including `0x004117f0` and `0x0040be70`.

Confidence: observed for layout initialization and links; individual vector names are provisional.

Possible falsifier: a camera subclass could reinterpret one of the embedded vector blocks in a special mode.

## Per-frame camera update

### `0x0040f850` — `Camera_Update`

Name class: provisional semantic name. The address is not a trustworthy decompiler function boundary in the generated Ghidra function table; it is promoted from the runtime vtable and raw disassembly.

Exact observed behavior:

- Uses `ECX` as `this` and begins by reading camera state, including `+0x3ac`, `+0x3bc`, `+0x3c0..+0x3c8`, `+0x3dc`, `+0x3e0`, and `+0x3f0..+0x3f8`.
- Mirrors anchor/target values when the corresponding flags are set and may copy positions from the primary or secondary tripod.
- Updates global camera/framing values at `DAT_00524a40/44/48`.
- Adjusts `camera + 0x40c` from camera/input state and writes its low word to the active viewport record at `DAT_00563a38 + 0x0e`.
- Calls `0x00410610`, `Camera_SmoothAndValidate 0x0040e090`, and `Camera_BuildLookAngles 0x004c9770` for camera/vector preparation.
- Dispatches through a jump table using `camera + 0x504`; the bounds check admits raw mode values `1..25`.
- Handles death-camera mode through `0x00410c90`, applies effect/shake vectors, increments `camera + 0x510`, and performs a final smoothing/position pass.

Static callers/callees:

- The method is reached from the object scheduler path `0x00469de0` → `0x00480ff0` → `0x00480fa0`, where an object vtable slot invokes the camera method. The runtime return address was `0x00480fc7`.
- Important callees include `0x0040e090`, `0x00410610`, `0x004c9770`, `0x00410c90`, `0x00410f70`, and `0x004113f0`.

Runtime experiment:

- `tony-camera-probe 240` broke at `0x0040f850` and recorded 240 accepted observations in Warehouse.
- All 240 had level `12`, camera vtable `0x005184b8`, camera `0x05f40ac8`, tripod `0x05f39530`, secondary target `0x05f39530`, caller `0x00480fc7`, and mode `1`.
- `camera + 0x510` ran from `0` through `239`, increasing exactly once per accepted camera hit.
- There were 11 startup camera calls before the first `Render_World` frame-clock tick; after that startup phase, the capture had one camera update per world-render clock step.
- `camera + 0x40c` was `12` for all 240 observations in this mode. This is a viewport/zoom input candidate, not a confirmed FOV value.
- Camera position, anchor target, and transform vectors changed continuously after startup. The four-word transform candidates had fourth components near `0x1000`, and their Q12 interpretations produced homogeneous values near `1.0`.

Hit frequency: 240 observations in the bounded capture; 229 of the post-startup observations had distinct world-render clock ticks, with one camera hit per tick. The initial 11 hits are level-entry/render-startup work, not evidence of an 11x camera rate.

Confidence: high for a per-frame gameplay camera update dispatcher; medium for the exact semantic role of each mode and vector block.

Possible falsifiers: a cutscene/death/two-player run could route through a different vtable entry or mode branch. Capture those modes before treating mode `1` behavior as universal.

### Mode dispatch table (static, semantic labels intentionally incomplete)

At `0x0040fed0`, the dispatcher computes `mode - 1`, bounds-checks it against
`0x18`, indexes byte table `0x00410390`, and jumps through the six-entry
target table at `0x00410378`. The exact address-based mapping is:

| `+0x504` value | dispatch target | observed/static role |
|---:|---:|---|
| `1` | `0x0040ff2b` | normal follow continuation; Warehouse runtime mode |
| `2` | `0x00410006` → `0x004113f0` | separate handler; semantic label pending |
| `3..22` | `0x00410027` | unsupported/default diagnostic path in this build’s table |
| `23` | `0x0041000f` → `0x00410f70` | separate handler; semantic label pending |
| `24` | `0x0041001b` → `Camera_DeathMode 0x00410c90` | death-camera handler |
| `25` | `0x0040feef` | alternate follow/effect path; calls `0x00410610` and smoothing |

Values `0` and `26+` take the out-of-range default path. This is useful for a
faithful recreation: preserve the raw table and its default diagnostic entry
instead of collapsing every non-default state into a semantic enum. The
special entries at `23..25` are real table bytes, although their runtime mode
coverage is not yet captured.

### `0x0040e090` — `Camera_SmoothAndValidate`

Exact static behavior:

- Copies `+0x448..+0x454` into `+0x470..+0x47c` at entry.
- Handles the no-tripod branch separately.
- In the normal tripod branch, reads tripod physics state at `tripod + 0x30b8`, maintains history at `+0x620..+0x634`, applies interpolation/collision helpers, and writes final camera position to `+0x08/+0x0c/+0x10`.
- Copies target/transform vectors between `+0x45c..+0x468`, `+0x448..+0x454`, and the previous block depending on `+0x510` and mode conditions.

Static callers/callees: called twice from paths inside `0x0040f850` in the recovered raw disassembly; calls vector/collision helpers including `0x0040c370`, `0x0040e060`, `0x004a9bf0`, and fixed-point math helpers.

Runtime evidence: the settled Warehouse camera path hit this helper approximately once per camera update in the earlier bounded probe; the current camera trace’s changing position and transform blocks are consistent with its entry/exit copies.

Confidence: inferred smoothing/final-position helper, observed field writes.

Possible falsifier: a mode-specific call could use the same helper for a non-follow camera; compare its writes in death/camera-point mode.

### Other camera helpers

| Address | Current name | Static evidence | Confidence |
|---|---|---|---|
| `0x0040c370` | `Camera_ApplyEffects` | Reads tripod physics state and camera effect counters/short fields; participates in shake, death/follow, and smoothing paths. | inferred |
| `0x00410c90` | `Camera_DeathMode` | Asserts on missing/non-skater tripod; copies tripod position and interpolates over `+0x570` for roughly `0x1f` ticks. | inferred |
| `0x00411fc0` | `Camera_PointSelect` | Chooses the nearest registered camera point, sets `+0x504` to `1`/`2`, links `+0x3dc`, and writes selected point coordinates. | inferred |
| `0x00411f30` | `Camera_RegisterPoint` | Appends a point ID to `DAT_0055fa58`, with a maximum of `0x46` entries. | observed |
| `0x0040bd40` | `Camera_Shake` | Selects shake parameter sets by `EShakeType` and applies them to camera effect objects. | inferred |

## Camera fields currently safe to expose in C++

The evidence-backed layout is recorded separately in [re/types/camera.yml](../../types/camera.yml). The critical distinction is to preserve raw integer storage until the scale is proven:

| Offset | Current interpretation | Confidence |
|---|---|---|
| `+0x00` | camera vtable; observed value `0x005184b8` | confirmed |
| `+0x08..+0x10` | final camera position in Q16.16 world words | inferred from player/tripod representation and identical fixed-point arithmetic |
| `+0x14..+0x18` | three signed 16-bit look-angle fields; first two are masked to 12 bits and the third is zeroed by `0x004c9770` | observed |
| `+0x3a4` | primary tripod/player link | confirmed |
| `+0x3ac`, `+0x3bc` | anchor/target control flags | observed |
| `+0x3c0..+0x3c8` | primary anchor/target in Q16.16 world words | inferred |
| `+0x3dc` | secondary target/tripod link | observed |
| `+0x3e0` | secondary target-valid/control flag | observed |
| `+0x3f0..+0x3f8` | look-at target in Q16.16 world words | inferred from the `0x004c9770` call |
| `+0x40c` | viewport/framing parameter; copied to viewport `+0x0e` | observed, not FOV-confirmed |
| `+0x448..+0x454` | current four-word transform/effect vector in Q12-like units | inferred |
| `+0x45c..+0x468` | target four-word transform vector in Q12-like units | inferred |
| `+0x470..+0x47c` | previous/saved four-word transform vector in Q12-like units | inferred |
| `+0x504` | camera mode/state dispatch value | observed; mode meanings incomplete |
| `+0x510` | per-update/mode tick | observed; increments once per update in mode `1` |
| `+0x570` | death-camera interpolation tick | observed |
| `+0x5d8..+0x5e4` | smoothing/history counters | observed |

Numeric conclusion:

- Player world positions are independently supported as 16.16-style fixed values.
- Camera transform/effect vectors visibly use integer arithmetic, `>> 0xc`, `<< 0xc`, and homogeneous `0x1000` values. A Q12 representation is therefore the best current C++ representation for those vector/matrix-like values.
- Camera position and anchor storage should be represented as signed Q16.16 raw values in the first faithful implementation. The camera constructor copies tripod/player world words directly, and the camera path uses those words with the same fixed-point shifts as the player path.
- Do not convert the canonical camera state to `float` at update time. Convert only at the render boundary, reproducing the original fixed-point rounding first.

## Exact fixed-point and angle primitives

The following helpers are promoted from raw disassembly rather than inferred from
runtime magnitude:

### `0x004c9770` — `Camera_BuildLookAngles`

Callsite `0x0040fdb3` passes:

```text
output = camera + 0x14
origin = camera + 0x08
target = camera + 0x3f0
```

The helper computes `target - origin`, arithmetic-shifts each component right
by 12, and treats the result as Q4 displacement words. It then:

- computes the second output angle from `x/z`;
- computes the first output angle from `y/sqrt(x²+z²)`;
- writes the third output short as zero;
- masks the first two output shorts with `0x0fff`;
- returns the horizontal range as an integer square-root result.

The ratio passed to `0x004e8580` is `(numerator << 12) / denominator`, with
the original x86 signed/unsigned division branches preserved. `0x004e8580`
calculates `atan(ratio / 4096) * 4096 / (2*pi)`. The constants are:

```text
0x518910 = float 1/4096
0x519a08 = float 2*pi = 6.28318548
0x519a10 = double 4096/(2*pi) = 651.89862876367124
```

The shared return conversion at `0x005004f4` changes the x87 rounding mode
to round-toward-zero and uses `FISTP`; truncation is therefore part of the
observable result. `0x004f39b0` is the matching sine helper: it masks an angle
to 12 bits, evaluates `sin(angle * 2*pi / 4096) * 4096`, and truncates.

This establishes a 4096-unit turn (`0x400` = 90 degrees, `0x800` = 180
degrees). The native reference is [src/camera/camera_math.hpp](../../../src/camera/camera_math.hpp).

Promotion audit for these math helpers:

| address | static callers/callees | runtime experiment | confidence / falsifier |
|---|---|---|---|
| `0x004c9770` | caller `0x0040fdb3`; callees `0x004e8580`, `0x004f53b0`, shared x87 return `0x005004f4` | not independently counted; reached by every captured mode-1 camera update | observed; a mode that writes angles without this call would falsify universality |
| `0x004e8580` | callers include `0x004c9770`; shared x87 return `0x005004f4` | not separately counted | observed from instruction sequence and constants; a non-atan use of the same constants would falsify the semantic label |
| `0x004e39a0` | callers include `0x0045e8e0` and camera transform paths; callee `0x004e2070` | not separately counted; downstream render setup is reached before present | observed; overflow-sensitive inputs could require modeling 32-bit `IMUL` wrap explicitly |
| `0x004e85a0` | callers include camera-mode transform paths and render preparation; shared x87 return `0x005004f4` | not separately counted | observed; unusual matrix/vector inputs could expose a different conversion convention |

### `0x004e39a0` — Q12 matrix multiply

The helper sign-extends nine 16-bit matrix elements and three signed vector
words, performs three row dot products, arithmetic-shifts each sum right by
12, and stores the three integer results. In compact form:

```text
out[row] = SAR((m[row][0] * v[0]) + (m[row][1] * v[1]) + (m[row][2] * v[2]), 12)
```

`0x004e85a0` is the related conversion path: it reads the same nine signed
short matrix elements, performs the dot products through x87, multiplies by
`0x519908` (`1/4096`), and converts through the same truncating helper. This
is strong evidence that view/model transform matrices are signed Q12, not
ordinary float matrices.

## View/projection and world handoff

`Render_World 0x00467c90` receives a scene/viewport argument and an integer viewport index. It:

- selects `DAT_0055fa3c` and the per-viewport record at `DAT_00560fd4 + 0x88 + index * 0x8000`;
- calls `Render_SetViewProjection 0x0045e8e0` with the viewport record and current render state;
- calls `0x0045f530` for actor/skater submission; the adjacent render traversal reaches `0x00460a90` for the object/model list;
- is called by the render-preparation function `0x00468520` from `0x0046a0f0`.

`Render_SetViewProjection 0x0045e8e0` stores the active viewport and camera/view record in globals, consumes short/fixed-point viewport fields, reads matrix-like blocks at view-record `+0x34` and `+0x54`, calls `0x004e39a0`, and prepares transform arrays around `DAT_005620c0`/`DAT_005620e8`. The first matrix block is read as nine signed shorts at `+0x34..+0x44`; the second is read at `+0x54..+0x64`. `0x004d14d0` later converts short fixed-point model values for the backend.

The render setup also derives viewport center/scale shorts from the active
viewport record and global scale values. It writes camera `+0x40c` into the
active viewport record at `+0x0e`, but the current evidence does not identify
that value as a perspective FOV. A faithful renderer should retain the raw
viewport record and only promote a projection parameter after a controlled
zoom/aspect experiment.

### One actor submission path

`Render_World` passes the active scene/player pointers through
`Render_SubmitActor 0x0045f530` (twice in the normal one-player path, for the
two actor/material submissions visible in the static body). The submission
function:

- copies the prepared short transform records from `DAT_005620c0` and
  `DAT_005620e8` into the fixed-point scratch region at `DAT_006a3e80`;
- invokes `Fixed_MatrixMultiplyQ12 0x004e39a0` while preparing transformed
  values;
- reaches `0x004d11d0` with an indexed geometry record and two zero flags;
- `0x004d11d0` converts the signed short geometry words and the prepared
  Q12-like values through `0x518910 = 1/4096` before continuing into the
  backend-facing path.

This is enough to define the camera-to-object handoff for a native vertical
slice: camera update writes raw Q16 position/angle state, view setup writes
Q12 matrix/viewport state, actor submission consumes the transformed scratch
records, and the renderer later presents. It is not a claim that
`0x004d11d0` is the final hardware draw call.

`Render_SubmitActor 0x0045f530` has static callers in `Render_World
0x00467c90` and the neighboring render-stage branches. The current successful
runtime capture stopped at the camera/view boundary and did not separately
count this helper; its promotion is therefore static/inferred, not a claim of
a measured per-frame hit rate. The falsifier would be a runtime path that
submits the active actor through another game-owned entry without passing
through this function.

The static path proves the view/projection handoff, but not yet the exact matrix convention, FOV, near/far clip, or handedness. Those must be recovered before matching visual output exactly.

## Minimal faithful C++ contract

The first native implementation should preserve the original update/render boundary:

```cpp
using Raw = std::int32_t;

struct Q16Vec3 { Raw x, y, z; };

struct Q12Vec4 { Raw x, y, z, w; };

struct LookAngles12 {
    std::uint16_t first, second, third;
};

struct CameraStateRaw {
    Q16Vec3 position;
    Q16Vec3 anchor_target;
    Q16Vec3 look_target;
    LookAngles12 angles;
    Q12Vec4 current_transform;
    Q12Vec4 target_transform;
    Q12Vec4 previous_transform;
    std::uint32_t mode;
    std::uint32_t update_tick;
    std::int32_t viewport_parameter_raw;
};

struct CameraLinks {
    std::uint32_t tripod_link;           // PE32 pointer representation
    std::uint32_t secondary_target_link; // PE32 pointer representation
};

void CameraSystem::update(CameraStateRaw& camera,
                          const CameraTarget& tripod,
                          const CameraInput& input,
                          FixedDelta dt) {
    mirror_or_rebuild_anchors(camera, tripod);
    update_mode_dispatch(camera, tripod, input, dt);
    smooth_and_validate(camera, tripod, dt);
    apply_effects_and_shake(camera, dt);
    ++camera.update_tick;
}

void Renderer::render_world(const CameraState& camera, const Scene& scene) {
    auto view = build_view_from_raw_fixed(camera);
    auto projection = build_projection_from_viewport_state(camera);
    submit_world(scene, view, projection);
}

void Renderer::present() {
    // The frame clock belongs here, immediately before the backend Flip.
    backend.flip();
}
```

The names in this contract are reconstruction interfaces, not claims that the original binary used the same C++ class names. Mode dispatch, fixed-point rounding, camera-point transitions, death-camera interpolation, and projection conversion must be implemented as separate testable pieces.

## What remains before visual-faithful recreation

The camera boundary is now usable, but these items still matter for pixel/behavior fidelity:

1. Recover the exact four-word transform layout, including row/column order and sign conventions; Q16.16 world words, Q12 matrix words, and 12-bit angles are now established.
2. Isolate the projection parameter represented by viewport record `+0x0e` / camera `+0x40c`; do not call it FOV until a controlled zoom/camera-input experiment proves that.
3. Enumerate the `+0x504` mode values and transitions in normal follow, camera-point, death, replay, menu, and two-player paths.
4. Reproduce the original fixed-point multiply, divide, shift, saturation, and trigonometric lookup behavior. Ordinary floating-point math will drift in camera smoothing and orientation.
5. Capture a controlled turn/move trace with the present clock enabled, then compare camera target, position, orientation, and viewport fields against the same frame IDs.
6. Recover the remaining scene/object transform handoff only far enough to validate one visible object; leave asset disk-format ownership to the asset-runtime session.
7. Validate viewport selection and present behavior in split-screen or alternate modes, where one gameplay update may feed multiple viewport renders.

The next highest-value probe is a two-phase trace using `render_present` as the clock: stationary Warehouse, then controlled left/right camera movement. It should record only the camera fields above plus the viewport record and one object submission pointer. That will resolve the remaining scale/projection questions without expanding into the full DirectDraw backend.

## Open questions and falsifiers

- `0x0040f850` is recovered from a runtime vtable and raw disassembly, but Ghidra’s function table does not provide an ordinary function record for its exception-heavy prologue.
- The raw `tony-view-probe` is implemented and unit-tested, but the two bounded headless retries in this pass did not reach level entry after synthetic frontend input; they produced no dynamic projection samples. Projection claims in this document are therefore static-only until a controlled level-entry run succeeds.
- `+0x40c` is a strong viewport/framing candidate; a run that changes camera zoom without changing it would falsify the current label.
- The current Warehouse capture remained in mode `1`, so it does not identify all mode constants or cutscene behavior.
- The camera object’s embedded vector blocks expose Q12-like arithmetic, but the exact C++ type and whether all four words are a matrix row, quaternion-like vector, or effect basis remain provisional.
- `0x0041c2d0` may run other shell/session modes without entering `Game_LevelLoop`; a callback trace in menu and level modes would strengthen the loop relationship.
