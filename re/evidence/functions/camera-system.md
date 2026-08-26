# Camera system and rendered-frame boundary

Status: normal-mode input/player/camera/update ordering, fixed-point camera math, camera-to-view matrix ordering, shake composition, and the gameplay render/present ordering are established; projection scale semantics and non-default modes remain partial

Build: THPS2 PC PE32/i386, SHA-256 `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

Primary runtime captures: `build/debug/camera-live.jsonl` (`camera-live`, 240 camera observations), `build/debug/camera-present-validation2.jsonl` (present-clock/effect validation), `build/debug/camera-render-handoff.jsonl` (live world/view/object handoff), `build/debug/camera-turn-calibration.jsonl` (present-clocked controlled motion), `build/debug/camera-input-motion2.jsonl` (paired input/player/camera motion), and `build/debug/camera-zoom-probe.jsonl` (dedicated camera-action experiment)

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
            -> player prephysics camera-point producer 0x004cd750
                -> Camera_PointSelect 0x00411fc0
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

The present-clock relationship was also checked with the camera worktree's
structured trace. `tony-frame-clock render_present` used `0x004d0ca4` as the
clock, then a Warehouse run recorded 1,000 accepted `Camera_Update` hits at
`0x0040f850`. The first clock value in that level segment had 11 startup
camera calls; after those calls, every subsequent clock step had exactly one
camera update. The accepted camera records were all level `12`, mode `1`, and
`camera + 0x510` advanced from `0` through `999`. A separate 500-hit effect
capture at `0x0040c370` covered the same level segment and retained the same
camera/update ordering. This is stronger evidence for using the Flip boundary
as the native frame clock than the message-pump sampler: the first 11 camera
calls are startup work, while the settled path is one camera update per
present-clock step.

Evidence files: `build/debug/camera-present-validation2.jsonl`, with 1,000
camera records and 500 effect-boundary records; the capture was intentionally
bounded after the observations were collected. The first-frame multiplicity
is therefore characterized rather than hidden: frame `4300` contains the 11
startup camera calls, and frames `4301..5289` contain one camera record each.

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

### Simulation timing contract

The two timing stages are separate from the confirmed present clock:

```text
0x004f7ce0  message pump
0x004f5ff0  millisecond source -> public 60-Hz tick
0x00469de0  gameplay/object/camera update
0x0046a0f0  render preparation
    -> 0x00468b30  three-sample camera-rate producer
0x004d0ca4  present / Flip
```

`0x004f5ff0` (`Game_AdvanceTime`, semantic name) calls the timer source
`0x00502ccb`, stores the previous 32-bit millisecond sample at
`DAT_029d836c`, and computes the signed integer step
`((now - previous) * 0x3c) / 1000`. A positive step advances
`DAT_0056e31c`. The separate simulation-time accumulator
`DAT_0056e320` advances only when `DAT_00561c04 == 0` and
`DAT_0056a8e0 == 0`; there is no visible maximum-delta clamp in this path.
The native reference exposes this as `advance_simulation_clock_ms`.

`0x00468b30` (`Camera_TimingProducer`, semantic name) consumes the simulation
time accumulator after gameplay/camera update. It stores a three-entry ring of
time differences at `DAT_0056868c`, sums the entries, replaces a zero sum with
`1`, computes the inverse-rate diagnostic `0xb4 / sum`, caps the sum at `15`,
and derives the Q8 camera/physics rate as `(capped_sum << 8) / 6`. A guarded
branch divides that rate by four, and `DAT_0056a948` applies the integer
`*125/100` slow-rate branch. The squared rate and accumulated progress are
updated with the same signed shifts. Because this helper runs in render
preparation after `Camera_Update`, its `DAT_0056865c` result is consumed by the
following camera update. The native `camera_timing.hpp` models this producer
and keeps the rate as an explicit input to the camera smoothing stage.

Static confidence: high for integer order, guards, ring window, and producer /
consumer ordering. Runtime confidence: the camera probe now records all of
these raw timing globals at update entry, and `tony-camera-timing-probe`
records the post-producer state; a trace showing the rate used by a camera
update does not equal the preceding producer result would falsify the claimed
one-update latency or indicate another writer between the two boundaries.

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
- Before the mode table is dispatched, adjusts `camera + 0x40c` from camera/input state and writes its low word to the active viewport record at `DAT_00563a38 + 0x0e`. The raw disassembly at `0x0040fa89..0x0040fc00` shows the exact control shape: a guarded assignment from `DAT_00524aa4`, decrement/increment flags at `DAT_0056b008`/`DAT_0056b018`, a reset-to-`0x100` flag at `DAT_0056aff8`, and a signed delta/timer pair at `+0x410/+0x414`. This ordering applies to the non-default mode paths as well; it is not part of the normal-follow common tail.
- The same pre-dispatch block also exposes the remaining raw framing-input seam. Unless `DAT_0055fa30` is nonzero, `DAT_0056b244` restores `DAT_00524a40/44/48` from `DAT_0055f9a4/10/78`; flags `DAT_0056b174`/`b184` change camera `+0x5b4` by `-10/+10`; and the `x/y/z` flag pairs at `b1e4/b1f4`, `b204/b214`, and `b1a4/b1b4` change the three framing words. The adjustment is `8` when `DAT_0056b254 == 0` and `0x20` for any nonzero byte, because the retail sequence is `neg byte; sbb reg,reg; and 0x18; add 8`; Y is masked with `0xfff` afterward. This is now represented as `CameraFramingInputControlRaw`, but its user-facing meaning is deliberately not promoted to FOV or camera-axis terminology.
- Calls `0x00410610`, `Camera_SmoothAndValidate 0x0040e090`, and `Camera_BuildLookAngles 0x004c9770` for camera/vector preparation.
- Dispatches through a jump table using `camera + 0x504`; the observed table covers mode values through `0x19`.
- Handles death-camera mode through `0x00410c90`, applies effect/shake vectors, increments `camera + 0x510`, and performs a final smoothing/position pass.

The same raw-disassembly pass resolves an important producer boundary that is
not visible from the ordinary decompiler output. In the common continuation
at `0x0040ff2b`, when the linked tripod is present, the camera stores mode
`0x19` (`25`) if `tripod + 0x3110 > 0x64` and `tripod + 0x30b8 == 1`.
This is a camera-owned mode transition in addition to the previously recorded
store from `Skater_PhysicsDispatcher 0x0049db80`; the latter remains a second
gameplay producer, not proof that the camera should read the entire physics
dispatcher.

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
target table at `0x00410378`. The currently proven mapping is:

| `+0x504` value | dispatch target | observed/static role |
|---:|---:|---|
| `1` | `0x0040ff2b` | normal follow continuation; Warehouse runtime mode |
| `2` | `0x00410006` → `0x004113f0` | separate camera-mode handler; exact semantic label pending |
| `3..22` | `0x00410027` | unsupported/default diagnostic path in this build’s table |
| `23` | `0x0041000f` → `0x00410f70` | separate point/sequence-style handler; exact mode name pending |
| `24` | `0x0041001b` → `Camera_DeathMode 0x00410c90` | death-camera handler |
| `25` | `0x0040feef` | alternate follow/effect path; calls `0x00410610` and smoothing |

The table has 25 byte entries, so mode `25` is a real dispatch value rather
than padding. The dedicated `camera-zoom-probe` recorded two valid runtime
intervals for it. Its alternate handler performs a normal follow/smoothing
pass, may apply the transformed tripod-local offset guarded by camera `+0x4a9`,
then calls `0x00410610` again before the common commit tail. The mode-25
`(0,-0x1000,0)` offset is therefore a producer input within that handler, not
evidence that mode `25` is an alias for mode `1`.

### `0x0040feef` — mode-25 alternate-follow handler

Static behavior from the raw dispatch target is:

- call `Camera_FollowTarget 0x00410610`;
- call `Camera_SmoothAndValidate 0x0040e090` with the alternate call-site
  constant `0x2c7`;
- if a linked tripod has physics state other than `1`, behavior flag `+0x2f64`
  equal to zero, and follow-offset word `+0x3110 < 0`, write camera mode `1`;
- if the tripod has `+0x3110 > 0x64` and physics state `1`, retain/write mode
  `25`;
- when camera byte `+0x4a9` is set, transform local `(0,0,-0x1000)` by the
  tripod short basis at `+0x2e58` through `0x004e85a0`, store the result in
  camera `+0x610..+0x618`, shift that vector left by `0xc` through
  `0x004cad00` into `+0x5c4`, copy the unshifted result into `+0x5b8`, then
  call `0x00410610` again;
- before the alternate vector branch, update camera `+0x5ec` and `+0x5f0`
  from the linked tripod scalar at `+0x50` when tripod `+0x31ec` is nonzero
  and camera `+0x436` is nonzero. A nonnegative scalar increments `+0x5f0`, a
  negative scalar decrements it, and `(-tripod_scalar >> 12) * camera+0x436`
  is accumulated into `+0x5ec`; while `+0x5f0 > 0`, that accumulator is added
  to camera anchor Y (`+0x3c4`). Separately, nonzero camera `+0x434` adjusts
  shared angle global `DAT_00524a94` by `-s16(+0x434)` for nonpositive scalar,
  or by `-(s16(+0x434) >> 1)` for positive scalar, then masks it to 12 bits;
- join the common camera tail at `0x00410064`, which ultimately commits through
  `0x0040be70`.

Static callers/callees: caller `0x0040fed0` via the six-target jump table;
callees `0x00410610`, `0x0040e090`, `0x004e85a0`, `0x004cad00`, and the common
commit path. Runtime evidence: `camera-zoom-probe.jsonl` recorded intervals
at frames `4108..4204` (97 camera observations) and `4292..4389` (98
observations), with mode `1` before, between, and after. Confidence is high
for dispatch and branch predicates, medium for the transformed-vector
producer because the `+0x4a9` branch has not yet been live-sampled. A runtime
mode-25 trace that bypasses the second follow call or uses a different local
vector would falsify the remaining producer details.

One mode-25 producer is visible at the gameplay boundary in
`Skater_PhysicsDispatcher 0x0049db80`. In the physics-state `0` path, when the
player has the `+0x2dd4`/`+0x3018` conditions, it sets camera `+0x504` to
`0x19` when `player +0x3110 > 1000` and either
`player +0x3130 > 0x5000` or the elapsed tick condition is below
`DAT_0056865c * 6 >> 8`. The same path emits the associated `0x14` event.
This is a producer boundary, not a request to reimplement the surrounding
physics dispatcher inside the camera class.

The direct camera-side producer is stronger evidence for the update contract:
the `0x0040ff2b` branch compares the linked tripod's `+0x3110` and
`+0x30b8` fields immediately before the mode-dispatch continuation. The
current C++ model should therefore expose this as an injectable transition
input until the runtime target mapping is promoted; it should not silently
derive mode `25` from a renamed generic `tripod_state` field.

Runtime validation from `camera-zoom-probe.jsonl` recorded mode-25 intervals
at frames `4108..4204` (97 observations) and `4292..4389` (98 observations),
with mode `1` before, between, and after them. The camera remained attached to
the live Warehouse player and returned to mode `1` without entering the point
or death handlers. Confidence is high that mode `25` is a valid alternate
follow transition and medium for the exact gameplay meaning of its producer;
a falsifier would be a trace showing the same `0x19` store used for a
non-camera event or a mode-25 update that bypasses the recovered follow offset
branch.

The mode-23 and mode-24 targets do not return through the normal
`Camera_SmoothAndValidate` tail. Both handlers jump to `0x00410357`, which
calls `Camera_CommitViewportEffects 0x0040be70` and then returns from
`Camera_Update`. The native `update_camera` contract therefore exposes an
explicit `CameraModeInputRaw` boundary for these handlers and skips normal
history/smoothing/shake when the required mode producer inputs are present.

Their transform interpolation weights are distinct from the normal
simulation-delta smoothing weight. Static PE32 arithmetic is:

```text
death: floor(tick * 0x1000 / 0x1e)
point: floor(tick * 0x1000 / 0x82)
```

The binary computes these using unsigned reciprocal multiplies
`0x88888889`/`>>4` and `0xfc0fc0fd`/`>>7`, respectively. The native helpers
`camera_death_transform_weight_q12` and
`camera_point_transform_weight_q12` preserve those operations before feeding
the shared Q12 quaternion interpolator at `0x004a9bf0`.

### `0x0040e090` — `Camera_SmoothAndValidate`

Exact static behavior:

- Copies `+0x448..+0x454` into `+0x470..+0x47c` at entry.
- Handles the no-tripod branch separately.
- In the normal tripod branch, reads tripod physics state at `tripod + 0x30b8`, maintains history at `+0x620..+0x634`, applies interpolation/collision helpers, and writes final camera position to `+0x08/+0x0c/+0x10`.
- The distance-history sample is now statically resolved through `0x004ca8f0`: it reads a separate tripod vector at `tripod + 0x4c`, shifts it to Q4 to measure length, clamps lengths above `100` to the Q16 vector `(100,0,0)`, and derives the history sample through `0x004f5f90`/`0x004f53b0`. This is distinct from the follow offset at `tripod + 0x310c`; the later collision/effect transform remains a separate injected producer.
- Copies target/transform vectors between `+0x45c..+0x468`, `+0x448..+0x454`, and the previous block depending on `+0x510` and mode conditions.

Static callers/callees: called twice from paths inside `0x0040f850` in the recovered raw disassembly; calls vector/collision helpers including `0x0040c370`, `0x0040e060`, `0x004a9bf0`, and fixed-point math helpers.

Runtime evidence: the settled Warehouse camera path hit this helper approximately once per camera update in the earlier bounded probe; the current camera trace’s changing position and transform blocks are consistent with its entry/exit copies.

Confidence: inferred smoothing/final-position helper, observed field writes.

Possible falsifier: a mode-specific call could use the same helper for a non-follow camera; compare its writes in death/camera-point mode.

### Other camera helpers

| Address | Current name | Static evidence | Confidence |
|---|---|---|---|
| `0x0040c370` | `Camera_ApplyEffects` | Reads tripod physics state and camera effect counters/short fields; participates in shake, death/follow, and smoothing paths. The `tony-camera-effects-probe` now records its guard globals, tripod gates, shared vertical effect, and raw effect fields. | static boundary; runtime hit count pending |
| `0x00410c90` | `Camera_DeathMode` | Requires a tripod, latches its position at tick `+0x570 == 0`, interpolates toward the selected death position for ticks `0..30`, then writes mode `1`; also interpolates the death transform through the shared Q12 helpers. | medium; position contract is statically exact, transform producer remains partial |
| `0x00410f70` | `Camera_PointMode` | Initializes the point-sequence state, builds a Q12 orientation sequence, interpolates position over duration `0x82` using `+0x55c`, optionally advances by six ticks after the late flag, and returns to mode `1` after the duration. | medium for the fixed-point position sequence; point-table producer remains partial |
| `0x00411fc0` | `Camera_PointSelect` | Chooses the nearest registered camera point, sets `+0x504` to `1`/`2`, links `+0x3dc`, and writes selected point coordinates. | inferred |
| `0x00411f30` | `Camera_RegisterPoint` | Appends a point ID to `DAT_0055fa58`, with a maximum of `0x46` entries. | observed |
| `0x0040bd40` | `Camera_Shake` | Selects shake parameter sets by `EShakeType` and applies them to camera effect objects. | inferred |

### Gameplay-owned camera-point producer: `0x004cd750 -> 0x00411fc0`

The point selector is called from the player prephysics path at `0x004cd750`,
before the per-frame camera vtable update. It receives the player/object in
`ECX`, forms a candidate position from:

```text
candidate = player.position + player.camera_offset[+0x310c] * 0x6e
```

It compares that candidate against the registered point IDs at
`DAT_0055fa58`, using the runtime count at `DAT_0055fae4`, and resolves the
selected point through the point registry. The selected registry flags at
`DAT_00524cb8[index]` choose the camera handoff:

```text
flags & 0x400 (without 0x800): mode 2,
    target valid = 1, secondary link = player,
    primary tripod link = 0, anchor source flag = 0,
    anchor target = selected point

flags & 0x800: mode 1,
    target valid = 1, secondary link = player,
    primary tripod link = player, anchor source flag = 1,
    camera action variant = resolved point record word 1
```

This is the missing producer boundary behind the native `CameraModeInputRaw`
hooks: the camera handlers consume the resulting mode/target state, but the
point table and registry ownership belong to gameplay/player code. The new
`tony-camera-point-probe` records the player position, `+0x310c` offset,
candidate position, registered point IDs, point-state blocks, and the linked
camera's pre-call mode/target fields. It deliberately does not infer which
point was selected from a pre-call snapshot.

Confidence: high for the static call and output writes; runtime promotion is
pending a point-selection transition capture. Possible falsifier: a live hit
at `0x00411fc0` that is only table maintenance or a path whose registry flags
do not produce the documented mode/target writes.

The native side now contains the same narrow boundary as
`apply_camera_point_selection` and `build_camera_point_candidate_q16` in
`src/camera/camera_system.hpp`: point-table search remains an injected
gameplay producer, while the PE32 candidate arithmetic and mode-1/mode-2
camera link/anchor writes are directly testable.

The selector also has a second, render-visible tail after the mode/link
handoff. It re-reads the selected registry flags and updates the active
viewport input word at `DAT_00563a38 + 0x0c` (word 6):

```text
flags & 0x400 and flags & 0x200:
    word6 = (flags & 0xff) * -10 + 0xb2c

flags & 0x400 and flags & 0x100:
    distance = clamp(distance(player, selected_point), 0x1c2, 0x960)
    word6 = (flags & 0xff) * 0x8552 / distance
```

The distance helper at `0x004c9590` subtracts Q16 world words, shifts each
component arithmetically by 12, sums the three 32-bit products, and passes the
sum through the retail integer-square-root helper at `0x004f53b0`. For the
`0x800` action branch, the switch on the selected point record’s action word
is also exact:

| action word | word 6 | `DAT_00524a40/44/48` | camera `+0x5b4` |
|---:|---:|---|---:|
| 0 | `0x96a` | `0xf5, 0xd7, -0x46` | `0` |
| 1 | `0xb72` | `0xaf, 0x32, -0x11` | `0` |
| 2 | `0xd52` | `0x122, 0x96, -0x6c` | `0` |
| 4 | `0x96a` | `0xf5, 0xd7, -0x46` | `0x800` |
| 5 | `0xb72` | `0xc3, 0x32, -0x11` | `0x800` |
| 6 | `0xd52` | `0x122, 0x96, -0x6c` | `0x800` |

The native point-selection result now exposes these raw viewport/framing
outputs and applies the `+0x5b4` action value. This is a semantic producer
boundary, not a claim that the point table itself belongs to the camera.
Confidence: high static; runtime point-transition coverage is still missing.
A live point hit with a different registry/action mapping would falsify the
promotion and should be recorded before changing the native contract.

The death-position sub-contract is now represented by
`advance_camera_death_position`. Static dataflow gives:

```text
if tick == 0:
    start = tripod.position
    camera.position = start

if tick < 31:
    delta = (start - death_target) / 30
    camera.position = start - delta * tick
    tick += 1
else:
    camera.mode = 1
```

The divide and multiply are component-wise signed PE32 operations; this is
not a floating-point easing curve. The same routine separately builds a Q12
transform interpolation through `0x0040e060` and `0x004a9bf0`; the native
`advance_camera_death_transform` now models that interpolation and leaves the
source/target transform producer explicit. Static callers are the
mode-dispatch target `0x0041001b` from `Camera_Update 0x0040f850`; the
position/weight fixtures cover ticks 0, 1, 30, 31, and the missing-tripod
diagnostic path. No live death-mode trace has yet been captured, so the
dynamic mode transition and transform producer remain validation items.

The point-position sub-contract is represented by
`advance_camera_point_position`:

```text
delta = (point_start - point_target) / 0x82
position = point_start - delta * point_tick
point_tick += (point_acceleration_flag ? 6 : 1)
if point_tick > 0x82: mode = 1
```

Static evidence for the duration and late increment is the instruction path at
`0x00410f70` (`cmp tick, 0x82`, then `+1` or `+6` after the global late flag).
Its orientation path calls `0x004a98c0`, `0x004a9820`, `0x004a9870`, two
`0x004a9650` compositions, `0x0040e060`, and `0x004a9bf0`. The native
`advance_camera_point_transform` models the final interpolator while keeping
the table-produced target transform explicit. Fixtures cover regular,
accelerated, completion, and dispatch ordering. The selected point-table
lookup and the initial mode-23 setup still require a runtime point/camera
trace before they should be folded into the default update path.

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
| `+0x40c` | camera-owned viewport/framing parameter; copied to viewport `+0x0e`; guarded assignment, +/- flags, reset, and timed delta are statically observed | observed; projection/FOV meaning not confirmed |
| `+0x448..+0x454` | current four-word transform/effect vector in Q12-like units | inferred |
| `+0x45c..+0x468` | target four-word transform vector in Q12-like units | inferred |
| `+0x470..+0x47c` | previous/saved four-word transform vector in Q12-like units | inferred |
| `+0x434`, `+0x436` | raw signed-short phase words consumed by the mode-25 alternate-follow scalar path | observed; semantic axis labels intentionally omitted |
| `+0x504` | camera mode/state dispatch value | observed; mode meanings incomplete |
| `+0x510` | per-update/mode tick | observed; increments once per update in mode `1` |
| `+0x570` | death-camera interpolation tick | observed |
| `+0x5d8..+0x5e4` | smoothing/history counters | observed |
| `+0x5e8` | follow-effect counter incremented on `0x00410610` entry and reset by its transition/dot-band branches | observed; not a distance value |
| `+0x5ec`, `+0x5f0` | mode-25 alternate-follow integrator/counter pair | observed; producer scalar is tripod-owned |

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
| `0x004a9650` | called by `0x00410610`; consumes two embedded four-word transform objects and writes a four-word result | not independently counted; statically reached in normal follow preparation | observed; operand convention is exact, while the final matrix row/column interpretation remains a separate question |
| `0x004a9bf0` | called by `0x0040e090`; normalizes two four-word Q12 records, chooses quaternion sign, blends by a Q12 weight, and renormalizes | statically recovered; Warehouse trace remains mode 1 but did not instrument this helper separately | observed as a normalized quaternion interpolation helper; the angular conversion is `0x004ca0a0` |
| `0x004f5f90` | x87 dot product of two three-word integer vectors, scaled by `1/4096` and truncated | called twice by `0x00410610` for follow-direction thresholds | observed; earlier “length” wording was corrected, and a non-dot use at an unexamined callsite would falsify the global semantic name |
| `0x004c9500` | converts two 12-bit look angles and a scalar into the raw follow-direction vector | called by `0x00410610`; exact sine/cosine products are represented in the native reference | observed; the caller’s downstream scale interpretation remains raw/provisional |
| `0x004f53e0` | transposes the nine prepared signed-short matrix words into the backend view-record order | called at the end of `0x0045e8e0` after the two fixed matrix multiplies | observed; downstream API row/column naming remains renderer-specific |

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

The distinction is material for a native recreation. `0x004e39a0` uses an
integer `SAR 12`, while `0x004e85a0` performs the equivalent scale through
x87 and truncates toward zero. For example, a negative dot product of `-2048`
becomes `-1` through `SAR 12`, but `0` through the x87 conversion. These are
separate operations in [camera_math.hpp](../../../src/camera/camera_math.hpp)
and must remain separate until every callsite is classified.

The camera/effect callsite is now classified more precisely: the three x87
dot products are written in the order matrix row 1, row 2, row 0. The native
reference exposes that exact cyclic result as
`camera_transform_matrix_q12_trunc`; the conventional row-0/1/2 helper remains
separate for ordinary matrix consumers. This ordering is required by the
post-smoothing camera tail and is covered by a non-diagonal fixture.

### Camera transform/effect records and shake

The raw update path gives the following embedded transform records:

```text
+0x45c..+0x468  target transform record
+0x448..+0x454  current transform record
+0x470..+0x47c  previous transform record
```

`Camera_SmoothAndValidate 0x0040e090` copies current to previous at entry and
copies target into current during the startup/short-history path. The embedded
object begins at `+0x444`: `+0x444` is its vtable and the payload at
`+0x448..+0x454` is four signed Q12 words. This is strongly quaternion-like,
not a fifth matrix column: `0x004a9820`, `0x004a9870`, and `0x004a98c0` build
the X/Y/Z half-angle sine/cosine records, and `0x004a9650` composes them with
the following exact result (where `first` is the third stack operand and
`second` the second):

```text
out.x = SAR12(second.x*first.w + second.w*first.x
              + second.z*first.y - second.y*first.z)
out.y = SAR12(second.x*first.z + second.y*first.w
              + second.w*first.y - second.z*first.x)
out.z = SAR12(second.z*first.w + second.y*first.x
              + second.w*first.z - second.x*first.y)
out.w = SAR12(second.w*first.w - second.y*first.y
              - second.x*first.x - second.z*first.z)
```

Every product/add/subtract is a 32-bit PE32 operation before `SAR 12`. The
native reference exposes this as `TransformQ12` and
`multiply_transform_q12`. `0x004a9910` then converts a payload object to nine
signed short matrix words for the render/effect scratch. Its fixed-point
products and output locations are now statically recoverable. With
`q11(a,b) = s16((a*b) >> 11)` and `one = 0x1000`, its row-major output words
are:

```text
m0 = one - q11(z,z) - q11(y,y)
m1 = q11(z,w) + q11(y,x)
m2 = q11(z,x) - q11(y,w)
m3 = q11(y,x) - q11(z,w)
m4 = one - q11(x,x) - q11(z,z)
m5 = q11(x,w) + q11(z,y)
m6 = q11(y,w) + q11(z,x)
m7 = q11(z,y) - q11(x,w)
m8 = one - q11(x,x) - q11(y,y)
```

The native reference exposes this as `transform_to_matrix_q12`. The inverse
helper `Camera_MatrixToTransformQ12 0x004a9a00` is also recovered. Its
positive-trace path computes the root and reciprocal scale through the x87
integer-square-root helper; its alternate path selects the largest diagonal
using the embedded lookup order `[1,2,0]`. Both paths use 32-bit products and
`SAR12` at the payload boundary. Matrix quantization is observable: converting
the Q12 half-turn X basis to shorts and back produces `0x0ffe` rather than the
original `0x0fff` component, so the native implementation must preserve the
short matrix round trip instead of assuming a lossless quaternion conversion.

The remaining visual calibration is now narrower than a generic matrix-basis
search. A controlled basis-object check is still useful for world-axis
handedness, screen-axis signs, and clip/depth conventions, but the
row/column/storage convention itself is promoted by the dynamic render trace.
Both payload/matrix conversion directions and the backend-record transpose are
encoded in [camera_math.hpp](../../../src/camera/camera_math.hpp).

`0x004a9bf0` is the orientation interpolation helper used by
`Camera_SmoothAndValidate`. Its static contract is:

```text
normalize(q1), normalize(q2)
dot = SAR12(q1 dot q2)
if dot < 0: negate q1 and dot
choose the near-linear or angular blend weights
out = (weight1*q1 + weight2*q2) >> 12
normalize(out)
```

The normalization path is represented by `normalize_transform_q12`. The
angular fallback calls `0x004ca0a0`, which converts the Q12 dot product to a
12-bit turn angle using x87 `acos(dot / 4096) * 4096 / (2*pi)` before the sine
weights are formed. That operation is represented by
`acos_ratio_to_angle`; the remaining camera gap is applying this helper in the
full mode/state update, not recovering the interpolation math itself.

`Camera_CommitViewportEffects 0x0040be70` is the last camera-side commit before
the view/render path. In the normal tripod path it writes the projected camera
screen position to the active viewport record, copies the current transform
record into camera-side cached words at `+0x650..+0x65c`, and derives the
screen/effect deltas at `+0x660..+0x668`. In the special tripod state `5` path
it uses the effect-adjusted position and restores the cached transform into
the payload beginning at `+0x448`. The word at `+0x444` is the vtable/start of
an embedded effect/transform object; it is not itself a fifth transform word.
It also commits the look target with global post-render offsets before calling
the viewport/effect helpers. This makes `+0x63c..+0x668` render-side effect
state, not a second canonical camera position.

`Camera_Shake 0x0040bd40` selects one of three global parameter sets for
`EShakeType` values `0`, `1`, and `2`:

```text
type 0: globals 0x55f792 / 0x55f796
type 1: globals 0x55f788 / 0x55f78c
type 2: globals 0x55f990 / 0x55f994
common: decay word 0x55f9bc, decay byte 0x55f9be,
        phase/angle dword 0x55f750, phase word 0x55f754
```

The selected values are stored at `+0x4f2`, `+0x4f6`, `+0x4f8`, `+0x4fa`,
`+0x4fc`, and `+0x500`. `Camera_Update` rotates the three signed shake axes
with the global 12-bit phase, composes those rotations into the current
transform, then decays each axis toward zero using its byte rate. The phase
inputs are the camera phase at `+0x500`, the low and high 16-bit halves of the
angle dword at `+0x4fc` and `+0x4fe`, each multiplied by global
`DAT_0056a8d4` and masked to 12 bits. The raw composition order is:

```text
current = Z(z_phase * shake_z) * X(x_phase * shake_x)
          * Y(y_phase * shake_y) * current
```

Each axis angle is formed by truncating `sin(phase) * axis / 4096` before it
is passed to the half-angle rotation constructor. The transform products use
the recovered 32-bit-wrapped Q12 operation at `0x004a9650`. If an axis
crosses the pre-effect sign during decay, the raw path clears it. The native
reference exposes this as `apply_camera_shake`; the global phase multiplier is
kept as an explicit hook input until its producer is promoted from the runtime
global set.

`Camera_AngleDelta12 0x004103b0` independently confirms shortest-turn angle
arithmetic: it masks both inputs to 12 bits, returns `(second - first)`, and
normalizes across the `0x000/0xfff` seam. That helper should be used anywhere
the recreation interpolates camera yaw/pitch rather than subtracting unsigned
shorts directly.

### Normal follow preparation

`Camera_FollowTarget 0x00410610` is the normal mode-1 preparation routine, not
the final smoothing step. Its static contract is now sufficiently clear to
keep as a separate native stage:

```text
anchor delta = camera + 0x3c0 - camera + 0x3b0
follow offset = tripod + 0x310c..+0x3114
mode 25 offset = (0, -0x1000, 0)
dot/angle helpers -> target orientation/effect records
history vectors -> +0x5b8 and +0x5c4
```

It computes look angles from the anchor delta, builds a Q12 direction using
the fixed-point vector helpers, and feeds an interpolation/transform chain
ending at `0x004a9650`; the resulting four words are written into the target
transform payload at `+0x45c..+0x468`. It also maintains the transition bytes
at `+0x418` and `+0x5d4`, the distance/preparation counters at `+0x5e8` and
`+0x60c`, and the Q12 vector records at `+0x5b8/+0x5c4`.

The normal-follow basis tail is now explicit. After the history recurrence,
the routine shifts the history vector right by 12, narrows it to signed
shorts, negates the signed-short tripod follow offset, and computes:

```text
side   = saturating_s16(history_q4 × (-follow_offset_s16))
up     = saturating_s16((-follow_offset_s16) × side)
basis  = [ side.x, -offset.x, up.x,
           side.y, -offset.y, up.y,
           side.z, -offset.z, up.z ]
target = basis_to_transform(basis) * Y(camera +0x5b4)
```

`0x004e2f80` performs each cross product with 32-bit `IMUL`/subtract and
clamps the result to `[-32768,32767]`; `0x004a9a00` converts the resulting
short basis; and the final `0x004a9650` composition occurs before
`Camera_SmoothAndValidate`. The native reference exposes these stages as
`cross_product_s16`, `build_follow_basis_matrix_q12`, and
`build_follow_target_transform_q12`. The tripod offset and `+0x5b4` angle are
also captured by the runtime camera probe so the remaining producer-side
scale can be compared directly.

The transition state machine is now promoted at the raw-input boundary. The
static contract is:

```text
vertical_metric = abs(dot((0, 0x1000, 0), follow_offset))
near_vertical   = vertical_metric < 0xdf4
transition      =
    (near_vertical && (tripod_state == 2 || tripod_state == 8
                       || camera + 0x418 != 0))
    || (camera + 0x5d4 != 0 && tripod_state == 1)

if transition:
    camera + 0x418 = 1
    preparation_counter (+0x60c) += 1
    direction/history vector (+0x610) = direction for counts < 4
    direction/history vector (+0x610) = (0, 0x1000000, 0) at count >= 4
    camera + 0x5d4 = 1
    camera + 0x5e8 = 0
else:
    if ((abs(direction_dot) & 0xfffff000) < 0x7d0001
        && direction_state_result > 4):
        preparation_counter = 0
        camera + 0x610 = direction
    else:
        preparation_counter = 0
    camera + 0x5d4 = 0

history_b = history_a
history_a = (anchor_target == mirrored_anchor)
    ? history_b
    : (history_b + mode_vector) >> 1
mode_vector = history_a
```

The `direction_state_result` value is kept as an injected producer result:
`0x004ca8f0` also mutates renderer scratch state, so treating it as a camera
enum would overstate what the disassembly proves. The tripod `+0x2f64` flag
resets `+0x418`, `+0x5d8`, and `+0x5d4`; `+0x2ddc` gates the transition-side
tripod notification. The native reference keeps `+0x5d8` separate from the
follow preparation counter at `+0x60c`.

Evidence ledger for this promotion:

| address | exact behavior | callers/callees | runtime evidence | confidence / falsifier |
|---|---|---|---|---|
| `0x00410610` | Computes the two absolute dot thresholds, applies tripod states `1/2/3/8`, updates `+0x418`, `+0x5d4`, `+0x5e8`, `+0x60c`, `+0x610`, then applies the anchor-equality history recurrence. | Called by `0x0040f850`; calls `0x004f5f90`, `0x004c9500`, vector add/shift/equality helpers, and the follow transform tail. | Warehouse probe: 240 mode-1 camera hits, one accepted update per post-startup render tick. Branch-specific producer values were not captured, so transition flags remain static-only. | High for the integer branch contract; runtime mode coverage is high, branch coverage is low. A trace with tripod states `2/8/1` or a producer-reset event could falsify the input mapping, not the observed instruction sequence. |

The recovered native fixture now exercises three independent raw cases: the
canonical follow basis, a long-history saturating cross product, and a
negative history value that distinguishes x86 arithmetic `SAR` from
truncating division. A two-update fixture also verifies that history is
advanced and the target transform is rebuilt before the startup current-copy
stage consumes it. These checks are intentionally based on literal basis
records rather than only comparing a helper to itself.

### Post-smoothing camera position stage

`Camera_SmoothAndValidate 0x0040e090` has a separate tail after transform
smoothing. It converts the current four-word transform at `+0x444` through
`0x004a9910`, calls `0x004e85a0` to transform a camera-local offset, and then
writes:

```text
camera.position = camera.anchor_target + transformed_local_offset
camera + 0x4c..0x54 = a second transformed effect vector
```

The position write is therefore not a direct tripod copy and must remain a
distinct native stage. The local-offset producer, collision-dependent
branches, and second effect vector are not yet promoted into the default C++
contract; they should enter through an explicit position/effect hook until
their gameplay inputs are captured. The C++ reference now exposes that
boundary as `CameraPositionStageInput` → `transform_position_stage` →
`apply_position_stage`; it uses the exact cyclic row order of `0x004e85a0`
and writes the transformed local offset relative to `anchor_target`. This is
the main remaining normal-mode camera gap after the follow basis recovery:
the transform and normal-path input shapes are recovered, but the exact
collision/effect producer logic is still outside the camera contract.

The base producer is now statically identified. Before the later branch logic,
`0x0040e090` derives the first input as:

```text
local_offset = (0, 0, -(camera + 0x5d0)) << 12
effect_input = (0, DAT_00524A48 << 12, 0)
```

`0x0040f850` updates `DAT_00524A48` and then seeds the shared effect globals
`DAT_0055F948/4C/50` as `(0, DAT_00524A48 << 12, 0)`. The smoothing helper
can add transition/collision terms to the local Z word and can modify the
effect magnitude, so this is a verified base stage rather than the complete
branching producer. The native reference exposes it as
`CameraPositionProducerRaw` → `build_base_position_stage_input`.

The live tail probe closes the transform-side part of that gap. In a headless
Warehouse run, a raw breakpoint at `0x004e85a0` filtered to the two
`Camera_SmoothAndValidate` return sites produced a repeated pair on each
settled camera pass:

```text
return 0x0040ecb8: vector = (0, 0, -distance_q16)
return 0x0040ecee: vector = (0, -effect_q16, 0)
```

The first call writes the local-offset result used by the subsequent
`anchor_target + result` position assignment; the second writes the camera
effect block at `+0x4c..+0x54`. For example, the stationary/early Warehouse
path supplied `-806912` for the first Z word and `-573440` for the second Y
word, while the first vector's Z word changed continuously as the camera
distance changed. The nine matrix words were signed 16-bit Q12 values and
changed with camera rotation, confirming that this is a live camera transform
tail rather than a one-time setup conversion.

A bounded capture made the base relation explicit. The eight observed
position/effect pairs had `camera +0x60c`, `+0x5d8`, and `+0x5e0` all zero;
`camera +0x5d0` took the sequence
`197, 299, 354, 386, 406, 420, 427, 430`; and every first vector was exactly
`(0, 0, -distance * 0x1000)`. `DAT_00524a48` remained `-140`, and every
second vector was exactly `(0, -573440, 0)`. This validates the base producer
in a live mode-1 level-entry path while also showing that the transition
additions were inactive in that capture.

The non-startup distance recurrence is also recovered statically. When the
tripod physics state is `0` or `4`, the function refreshes the six-word history
at `+0x620..+0x634`; otherwise it consumes the existing history. It sums those
words, computes

```text
step = ((((sum / 0xcc) * 0x3c) / 2) * 0xe10) / 0x149e >> 12
distance = (distance + step * 4 + DAT_00524A98) >> 1
```

with signed integer division, 32-bit multiply/add behavior, and arithmetic
right shifts. The result is stored at `+0x61c` and `+0x5d0`, respectively.
The native reference exposes the value-level contract as
`camera_distance_smoothing_step_q4` and `advance_camera_distance_q4`; the
history refresh is now also represented by
`advance_camera_distance_smoothing`: for physics states `0` and `4`, it shifts
`+0x620..+0x634`, accepts the scalar result of the opaque vector/collision
chain, stores `sample << 6` at `+0x620`, and returns the exact updated history,
step, and distance. Other physics states retain the existing six words.
`advance_camera_smoothing_stage` then feeds the recovered distance step into
the effect-envelope state while preserving the unresolved `0x0040c370`
transform producer as an explicit special-branch result. The C++
`CameraStateRaw` now carries the corresponding `+0x5d0`, `+0x61c`,
`+0x620..+0x634`, and `+0x5d8..+0x5e4` words, with stateful adapters for the
distance and effect stages; the shared vertical-effect word remains an
explicit reference input.

The native reference also contains `camera_distance_sample_from_q16`, which
models the resolved `tripod + 0x4c` sample producer. It preserves the retail
Q4 length check, the `100`-unit `(100,0,0)` clamp, and the Q16 dot/square-root
sample. The remaining `0x004e85a0` collision/effect transform is intentionally
still a hook boundary because it depends on gameplay/world collision state.

The orientation smoothing tail is now promoted as well. After its temporary
effect branches, `0x0040e090` copies `camera +0x458` (target transform) and
`camera +0x46c` (previous transform) into temporary transform objects, derives

```text
weight_q12 = SAR((DAT_0056865C * 222), 8)
current_transform = slerp(previous_transform, target_transform, weight_q12)
```

using `0x004a9bf0`, then for the first twelve update ticks copies the target
transform directly into the current transform. The entry copy from current to
`+0x470..+0x47c` is repeated at the tail only for those startup ticks; on a
steady-state tick the pre-smoothing transform remains in the previous block
until the next update enters. The native reference models this exact state
timing with `smooth_camera_transform_q12` and exposes the raw Q8 delta through
`CameraUpdateHooks::smoothing_delta_q8`; the default fixture value is the
observed runtime constant `0x100`.

The normal vertical-effect envelope is separately modeled. In the branch that
does not enter the special effect-transform path, `+0x5e0` is initialized from
the distance step and capped at `0x1e`; a squared phase contributes

```text
DAT_0055F94C += (counter_c * phase * phase * 8000) / 0x1e
```

with 32-bit multiply wrap and signed division. The alternate steady branch
uses the equivalent maximum phase constant `0x5fb40 = 49 * 8000` and advances
`+0x5e4`. The native reference exposes this as
`advance_camera_effect_ramp`; it reports the special branch rather than
guessing the unresolved collision/effect transform producer at
`0x0040c370`.

Static callers/callees: `0x0040ecb8` and `0x0040ecee` are the two callsites in
`0x0040e090`; both call `0x004e85a0`, whose three outputs are written in
matrix-row order `1,2,0`. Runtime hit frequency: one position/effect pair per
observed settled camera update; the camera entry probe independently recorded
one update per rendered frame after the 11-call level-entry startup. The C++
position-stage hook now matches the recovered transform and anchor writes, but
the code that chooses distance/effect magnitudes remains a gameplay/effect
producer boundary.

The native update ordering now exposes that boundary in the same place as the
retail routine: `CameraUpdateHooks::prepare_smoothing_stage` runs after the
entry current-to-previous copy and before orientation interpolation; the
returned base local/effect vectors are applied after the current transform is
available. A special-effect result suppresses the generic base application and
is handed to `commit_smoothing_stage`, preserving `0x0040c370` as a producer
boundary rather than silently applying the wrong transform. The fixture covers
this ordering with a nonzero distance, fixed-point effect vector, and a
steady-state interpolation tick.

Confidence: high for the two tail callsites, raw input scales, output ordering,
and position/effect destinations; medium for the semantic source of the two
input vectors because collision and effect branches can replace them.
Possible falsifier: a mode-23/24/25 or split-screen trace that reaches
`0x004e85a0` through another caller and uses a different vector layout; those
callers should be probed separately before making the hook inputs universal.

### Live view/projection and actor handoff validation

The previously static handoff is now covered by a live Warehouse capture.
`camera-render-handoff.jsonl` recorded 3,500 accepted calls to
`Render_SetViewProjection 0x0045e8e0`. The world-render caller at
`0x00467d0c` contributed 2,771 of them over frame-clock values `2214..4984`;
those records contained camera `0x05f40ac8`, view input `0x00533fa8`, and the
active viewport record `0x0055f7c8`. The view input had the expected
`640x480` rectangle, depth `30000`, vertical scale `3413`, and camera viewport
word `12`. The camera angle and look-target fields were readable at this
callsite, so this is a live camera-to-view setup observation rather than a
menu-only hit.

The same capture recorded 3,500 accepted calls to
`Render_SubmitActor 0x0045f530`. The five world-traversal callsites
`0x00467d52`, `0x00467d64`, `0x00467e6f`, `0x004681e8`, and `0x004681f4`
each produced roughly 554/555 observations over frames `2214..2768`.
Critically, the `0x00467e6f` path passed the live player pointer
`0x05f39530` on 554 observations; its submission record had stable raw type
and flag fields while the camera/view state continued to update. This
promotes the player submission path from static inference to a measured
world-render boundary. The other four pointers are scene/object candidates;
their runtime ownership remains provisional.

The capture also separates frontend and level rendering: menu callers
`0x004a476d`/`0x0045a660` occur at earlier frame ranges, while the
`0x00467d0c` view callsite and the five `0x00467xxx` actor callers occur in
the level segment. The remaining uncertainty is the exact matrix
row/column/handedness convention after `Render_SetViewProjection`; the live
handoff confirms consumption and ordering, not a conventional FOV or clip
matrix name.

The follow-up `camera-turn-calibration` capture used the same present callsite
as its frame clock while holding a Warehouse level and sending a controlled
left/right turn-and-move sequence. It recorded 8,301 contiguous
`render_present` events (the trace was stopped deliberately, so its footer is
`complete:false`), 700 camera observations at frames `1605..2294`, and 4,824
world view/projection observations at frames `1605..6428`. All 690 distinct
camera frames had a same-frame `0x00467d0c` world-view record. Across the
camera observations, position changed on all 700 records, anchor changed on
692, and orientation changed on 561; all remained in mode `1` with raw
viewport value `12`. The corresponding world matrix pair had 4,731 distinct
signatures while the steady view-input record remained
`640x480`, depth `20512`, vertical scale `3413`, and viewport value `12`.

This promotes the dynamic relationship

```text
Camera_Update 0x0040f850
    -> camera position/anchor/orientation changes
    -> Render_SetViewProjection 0x0045e8e0 via 0x00467d0c
    -> changing Q12 matrix payload
    -> Render_SubmitActor 0x0045f530
    -> Render_Present 0x004d0ca4
```

to high confidence for normal Warehouse gameplay. It does not yet identify
which internal gameplay field implements every camera behavior outside the
normal follow path, but the paired input/player trace now closes the
normal-mode timing chain. In `camera-input-motion2.jsonl`, the input sampler
at the post-poll gameplay boundary, the physics/player-diff probe, and
`Camera_Update` all record the same present-clock frame. The event ordering is
stable:

```text
Render_Present frame N
    -> input/action state
    -> physics/player writes
    -> Camera_Update
    -> Render_SetViewProjection / world traversal
Render_Present frame N+1
```

The controlled phases were idle `5760..6249`, Left `6250..6340`, idle
`6341..6365`, Right `6366..6455`, then idle. Left was action mask `0x8000`
with held DirectInput key `203`; Right was `0x2000` with key `205`. The Left
phase changed player position fields at `+0x08/+0x10` and the physics response
fields at `+0x4c/+0x54`; the Right phase changed the same position fields and
the full response triplet at `+0x4c/+0x50/+0x54`. Camera position, anchor, and
angle fields changed on those same frames, and the world view matrix changed
before the following present. This is high-confidence evidence for the
normal-mode input/player/camera/render ordering, not a claim that the camera
reads the keyboard directly.

### Dedicated camera-action experiment and `+0x40c`

The runtime configuration maps the keyboard camera action to `O`. A separate
Warehouse capture (`camera-zoom-probe.jsonl`) recorded 1,800 input samples
and 1,800 camera updates, including two held-camera phases. The action sampler
recorded `O` as DirectInput key `24` with action mask `0x0100` on frames
`3585..3674` and `3735..3795`; idle frames before, between, and after those
phases had mask `0`. Throughout all 1,800 camera records, including both
held-camera phases, camera `+0x40c` remained raw `12`. Camera mode remained
`1` during the held phases while position, anchor, and orientation continued
to update.

This falsifies the convenient interpretation that `+0x40c` is directly
changed by the normal camera key, and weakens its promotion as a FOV/zoom
scalar. The strongest current label is a viewport/framing selector or a
camera parameter copied from another producer. It remains part of the raw
projection contract until an aspect/display-mode or camera-state experiment
changes it and the resulting basis scale is measured.

The direction helper’s raw output is not a conventional normalized float
vector. For angles `a=first`, `b=second` and scalar `s`, it writes:

```text
out.x = -((sin(a)*s >> 12) * sin(b))
out.y =   sin(a)*s
out.z = -((cos(a)*s >> 12) * cos(b))
```

All sine/cosine values are Q12 integer results and the caller preserves this
mixed intermediate scale into the dot/threshold path. The native reference
keeps that behavior in `direction_from_angles_raw`.

The routine has two important mode/state seams that a faithful C++ camera
must preserve:

- camera mode `25` uses the hard-coded offset `(0,-0x1000,0)` rather than the
  tripod follow offset;
- when the tripod’s state/offset conditions are not eligible, it resets the
  follow transition state and uses the history vectors rather than applying
  the normal follow interpolation.

The current Warehouse runtime stayed on mode `1`, so these branches are
static evidence only. A falsifier would be a mode-1 trace where the target
transform changes without this routine or where the mode-25 branch is reached
with a different offset.

### `0x004113f0` — `Camera_Mode2`

Static recovery gives mode `2` a separate target-preparation contract; it is
not just an alias for the normal mode-1 continuation. The handler:

- derives an anchor delta from camera `+0x3c0` and `+0x3b0`;
- selects the tripod offset at `tripod + 0x310c..+0x3114`, or the fallback
  `(0,-0x1000,0)` when the tripod link is null;
- builds look angles and a raw direction vector, then conditionally seeds
  camera `+0x610` from that direction when the dot/length threshold branch is
  satisfied;
- copies smoothing vector `+0x5b8` to `+0x5c4`, applies the same external-
  history gate and vector interpolation family used by the follow path, and
  copies the resulting vector back to `+0x610`;
- converts that vector and the selected offset into the signed-short basis,
  converts the basis to a Q12 transform, composes the Y half-angle from
  camera `+0x5b4`, and writes the target transform at `+0x45c..+0x468`;
- clears the transition byte at `+0x5d4` before returning.

The native C++ reference now exposes this as `prepare_mode2_target` and keeps
mode `2` out of the mode-1 history path. Its inputs include the raw view-state
result from `0x004ca8f0` because that helper also updates renderer scratch
state. This remains static-only for the retail runtime: the bounded Warehouse
run never entered mode `2`, and a forced-mode probe could not acquire a
responsive WineDbg connection before the isolated session was stopped. Keep
the semantic mode label provisional until a runtime trace records the
selected offset, `+0x610`, smoothing vectors, and target-transform writes
together. A runtime mode-2 path that bypasses the `+0x5b8/+0x5c4` history or
uses a different fallback offset would falsify this promotion.

Promotion audit for these records:

| address | exact behavior | runtime/static evidence | confidence / falsifier |
|---|---|---|---|
| `0x0040be70` | commits screen/effect offsets, cached transform words, and look-target viewport state | static callers from constructor/update; reached in the camera path before world setup | provisional semantic boundary; a runtime write trace showing the renderer bypasses these records would falsify the handoff claim |
| `0x0040bd40` | selects shake parameter sets and forwards them to effect objects | decompiler + raw field writes; no controlled shake activation yet | observed behavior, effect meaning inferred; activate each shake type and compare the three axes |
| `0x004103b0` | shortest signed 12-bit angle delta | exact branch structure in raw disassembly | observed; only a caller using reversed argument order would change the API naming |
| `0x004e85a0` | x87 Q12 matrix conversion with truncation toward zero | exact signed-short loads, `1/4096` scale, and shared `FISTP` helper | observed; unusual overflow inputs could still require explicit x87 precision emulation |

## View/projection and world handoff

`Render_World 0x00467c90` receives a scene/viewport argument and an integer viewport index. It:

- selects `DAT_0055fa3c` and the per-viewport record at `DAT_00560fd4 + 0x88 + index * 0x8000`;
- calls `Render_SetViewProjection 0x0045e8e0` with the viewport record and current render state;
- calls `0x0045f530` for actor/skater submission; the adjacent render traversal reaches `0x00460a90` for the object/model list;
- is called by the render-preparation function `0x00468520` from `0x0046a0f0`.

`Render_SetViewProjection 0x0045e8e0` stores the active viewport and camera/view record in globals, consumes short/fixed-point viewport fields, reads matrix-like blocks at view-record `+0x34` and `+0x54`, calls `0x004e39a0`, and prepares transform arrays around `DAT_005620c0`/`DAT_005620e8`. The first matrix block is read as nine signed shorts at `+0x34..+0x44`; the second is read at `+0x54..+0x64`. `0x004d14d0` later converts short fixed-point model values for the backend.

The final `0x004f53e0` call is a literal 3x3 transpose from the prepared
matrix scratch into the second view-record block. This fixes the engine’s
internal matrix ordering even though the eventual renderer API should still
keep handedness and clip-space naming as explicit contracts.

The five-iteration view-to-object handoff is now represented by
`prepare_view_records_q12` in `src/camera/camera_math.hpp`. Starting at basis
word `1`, each iteration advances four words and sends the slices
`[base-1..base+1]` and `[base+11..base+13]` through
`Fixed_MatrixMultiplyQ12 0x004e39a0`. The first result is written to the
fifteen-short record at `DAT_005620e8`; the second is written to
`DAT_005620c0`. An identity-matrix fixture checks all five triplets and keeps
the signed-short narrowing visible. This closes the camera-to-prepared-object
record contract, while material/object traversal and the later geometry packet
remain separate boundaries.

### Dynamic matrix-convention validation

The paired `camera-input-motion2.jsonl` trace closes the previously open
row/column question for the normal gameplay path. It contains 1,091
`Render_SetViewProjection` observations from caller `0x00467d0c`, covering
frame-clock values `5760..6850`. For every observation, the nine signed shorts
in the view record at `+0x54` equal:

```text
transpose_matrix_q12(transform_to_matrix_q12(camera.current_transform))
```

The comparison uses the camera snapshot at the same frame-clock value; the
camera breakpoint is at function entry, so the `+0x34` block represents the
post-update matrix in the adjacent render/update phase. The one-frame probe
offset is visible in the trace rather than being hidden in the comparison.
For example, at frame `6000` the entry transform was
`[473,2366,1485,-2958]`. `0x004a9910` produces:

```text
[ 287,-1599, 3760, 2691, 2911, 1031, -3076, 2399, 1254 ]
```

and the backend-order `+0x54` record is:

```text
[ 287, 2691,-3076,-1599, 2911, 2399, 3760, 1031, 1254 ]
```

which is the exact 3x3 transpose. The same relation holds across all 1,091
normal gameplay view records, including the controlled Left and Right motion
phases. This promotes the storage convention with high confidence. A
remaining falsifier is a non-default mode or alternate viewport that uses a
different basis; world-axis handedness and final clip/depth signs still need
a controlled basis-object submission.

The render setup also derives viewport center/scale shorts from the active
viewport record and global scale values. It writes camera `+0x40c` into the
active viewport record at `+0x0e`, but the current evidence does not identify
that value as a perspective FOV. A faithful renderer should retain the raw
viewport record and only promote a projection parameter after a controlled
zoom/aspect experiment.

The projection search can now distinguish the two nearby raw fields. The
active viewport input word at `+0x0c` (word `6`) is directly consumed by
`Render_SetViewProjection` when deriving `viewport[7]`, and the static
`Camera_PointSelect 0x00411fc0` path writes this field with camera-point
values `0x96a`, `0xb72`, and `0xd52` (and a distance-derived value in the
interpolated branch). By contrast, the dedicated `O` action experiment
recorded action mask `0x0100` while camera `+0x40c` and viewport input word
`7` remained raw `12`. Therefore word `6` is the stronger vertical
projection/framing input candidate; camera `+0x40c`/word `7` remains a raw
selector or secondary viewport parameter and must not be labelled FOV.

Static viewport contract recovered from `0x0045e8e0`:

```text
viewport[0..3]  = rectangle edges (shorts)
viewport[4]     = signed depth/axis term (negated into the Q12 basis)
viewport[5]     = runtime viewport/state selector (overwritten from 0x563a3c)
viewport[6]     = vertical scale input
viewport[7]     = derived vertical projection scale
viewport[8]     = derived horizontal center
viewport[9]     = derived vertical center
```

The two display-dimension globals are `DAT_029da394` and `DAT_029da398`.
The two fixed-point viewport scale globals are `DAT_00563a6c` and
`DAT_00563a70`. Before matrix preparation, the function computes:

```text
viewport[7] = (((viewport[0] - viewport[2]) << 11) & 0xfffff000)
              / ((viewport[6] << 12) / DAT_00563a70)
viewport[8] = (viewport[0] + viewport[2]) >> 1
viewport[9] = (viewport[1] + viewport[3]) >> 1
```

The assembly uses zero-extended 16-bit inputs, 32-bit wraparound for the
subtract/shift, then a signed `idiv` for `viewport[7]`; this is not a floating
point width or a guessed aspect ratio. The subsequent basis state can be
represented as five eight-short blocks, beginning at `DAT_00563a90`:

```text
A = [ 0,       0,       -4096, selector, 0,        0,        4096, -depth ]
B = [ 0,       x_axis,  x_edge, 0,       0,       -x_axis,  x_edge, 0      ]
C = [ y_axis,  0,       y_edge, 0,      -y_axis,  0,        y_edge, 0      ]
D = [ 0,       v_axis,  v_edge, 0,       0,       -v_axis,  v_edge, 0      ]
E = [ h_axis,  0,       h_edge, 0,      -h_axis,  0,        h_edge, 0      ]
```

Here `selector` is the runtime value copied into viewport field 5, and the
normalization uses the original integer-square-root helper (`FSQRT` followed
by x87 truncation). For the horizontal/vertical basis terms, the unscaled
edge token is:

```text
extent = right * 0x1fffff + left       (32-bit wrap)
edge_token = extent << 11              (32-bit wrap)
x_axis = viewport[7] * scale_x /
         sqrt((edge_token >> 12)^2 + ((viewport[7] * scale_x) >> 12)^2)
x_edge = edge_token / the same divisor
y_axis/y_edge = the same construction using scale_y
```

The remaining terms use `SAR((top - bottom), 1)` and `SAR((left - right), 1)`
as the vertical/horizontal half-extents. This is now encoded in
`src/camera/camera_math.hpp` as `build_viewport_projection`; it is a raw
reference contract, not a claim that the result is a conventional FOV matrix.

It also normalizes the viewport rectangle against the display dimensions,
using `<< 9 / DAT_029da394` horizontally and `* 0xf0 / DAT_029da398`
vertically, then passes those four shorts to `0x004e87f0`. That helper writes
the `0xe3000000` render-state header, copies the four shorts, and replaces a
zero width or height with `1`; this is now modeled by
`normalize_viewport_record` in `src/camera/camera_math.hpp`. The Q12 basis
blocks at `DAT_00563a90..0x563ade` are built from normalized half-extents,
`viewport[6]`, `viewport[7]`, and `viewport[4]`; each row is consumed by
`Fixed_MatrixMultiplyQ12`. This establishes the integer projection setup and
where aspect/zoom enters, but not a conventional camera FOV scalar.

### Controlled projection perturbation

The first perturbation attempt was invalid because its bounded view sample
completed in the front end before the Warehouse geometry probe became live.
The probe was then changed to accept only a valid level, player, and camera;
menu calls are rejected without consuming the observation limit.

The fresh `camera-projection6` run provides the positive experiment:

| observation | accepted count | runtime evidence |
| --- | ---: | --- |
| `Render_SetViewProjection 0x0045e8e0` | 1,200 | every mutation record had level `12`, player `0x05f39530`, and camera `0x05f40ac8` |
| geometry handoff `0x004d11d0` | 1,200 | level `12`, same player/camera, 19 packets per full observed render frame |
| `Camera_Update 0x0040f850` | 500 | same live Warehouse camera path |
| `Render_Present 0x004d0ca4` | 16,595 | present clock remained live while the mutation ran |

The normal live input contained:

```text
viewport input words = (640, 480, 0, 0, 10, 20512, 3413, 12, 320, 240, 0, 0, 320, 480)
```

The probe alternated word `6` between `3413` and `1706` at function entry.
It produced 600 records at each value. The prepared view blocks were stable
across the 19 geometry packets belonging to a view setup, and the alternation
changed those blocks in the expected repeated pattern. For example, adjacent
live frames showed these first nine signed shorts in `DAT_005620c0`:

```text
word6 = 1706: (4, -3437, 2228, 3781, 283, 1543, -3779, 282, 1556)
word6 = 3413: (5, -2606, 3169, 3155, 491, 2574, -3149, 490, 2585)
```

The corresponding `DAT_005620e8` block changed as well. The effect reversed
on every subsequent pair while the camera continued to move, so this is
strong runtime evidence that input word `6` is consumed by projection/view
preparation and reaches object submission. It does not by itself prove that
the value is a conventional FOV angle: camera motion changes between the
alternating calls, and the exact screen/depth interpretation still needs a
stationary basis-object or viewport-producer experiment. The strongest
current semantic name is therefore `vertical_projection_input`, with high
confidence for dataflow and medium confidence for its user-facing meaning.

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
0x00467c90` and the neighboring render-stage branches. The focused
`camera-render-handoff` capture now separately counted this helper in the
live level path; the five `0x00467xxx` callers above account for the measured
world traversal. The exact object-list ownership remains provisional, and the
falsifier would be a runtime path that submits the active actor through
another game-owned entry without passing through this function.

The targeted `tony-actor-probe` now records the stack argument entering this
function, its raw object prefix, and the five fields visibly consumed by the
submission routine (`+0x04`, `+0x1a`, `+0x1f`, `+0x24`, and `+0x30`). It keeps
the object/model ownership provisional until a gameplay run captures those
records alongside `Render_World` and the present clock.

The new raw `tony-geometry-probe` closes that downstream observation without
claiming a renderer function name. Its acceptance predicate requires a live
player-owned camera, so menu geometry is rejected. The bounded
`camera-geometry-level` capture recorded 500 accepted calls to `0x004d11d0` at
frames `4413..4439`; every record was level `12`, player `0x05f39530`, and
camera `0x05f40ac8`. The caller was consistently `0x0046192a`, with 19
geometry packets per full observed frame. The two trailing call arguments were
stable raw resource/pipeline values (`0x02adfbdc` and usually `0x00568a00`),
while the first geometry pointer varied across the scene records and repeated
across frames. The prepared view blocks at `0x005620c0` and `0x005620e8`
contained 15 signed shorts each, were stable within a frame, and changed as
the camera/view state changed. The `0x006a3e80` scratch block was populated at
the same boundary.

This promotes the following narrower contract:

```text
Camera_Update
    -> Render_SetViewProjection
    -> Render_SubmitActor / scene traversal
    -> 0x0046192a -> 0x004d11d0 geometry handoff
    -> backend-facing conversion
    -> Render_Present
```

It does not promote `0x004d11d0` as the final draw call, and it does not prove
that the `0x0046192a` packets are the player model: the live packet pointers
were scene-geometry records. A player-specific actor trace remains required
for object ownership, but the camera-to-scene geometry transform boundary is
now runtime-supported.

A bounded `camera-actor` attempt armed `render_present`, the camera, view, and
actor probes, but the synthetic frontend path stalled before level entry. Its
trace contains zero accepted camera/view/actor observations and remains a
negative experiment, not evidence that the actor path is bypassed. The later
`camera-render-handoff` run reused the proven level-entry sequence and supplies
the positive live world/object observations above.

The static path proves the view/projection handoff, but not yet the exact matrix convention, FOV, near/far clip, or handedness. Those must be recovered before matching visual output exactly.

## Minimal faithful C++ contract

The value-level portion is now implemented in
[camera_math.hpp](../../../src/camera/camera_math.hpp) and
[camera_system.hpp](../../../src/camera/camera_system.hpp). `CameraStateRaw`
preserves the recovered PE32/Q16/Q12 fields, `prepare_follow_target` keeps the
mode-25 and dot/angle branches explicit, `update_camera_history` models the
observed half-step recurrence, and `commit_viewport_effects` preserves the
special tripod-state `5` behavior. The `CameraUpdateHooks` are intentional:
anchor reconstruction, gameplay-state transitions, and the final follow
transform producer are still owned by opaque runtime helpers, so the native
reference leaves them injectable rather than silently replacing them with
floating-point behavior.

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

1. Validate the renderer’s consumption of the recovered `0x004a9910` matrix row/column and sign convention with controlled X/Y/Z basis inputs; both payload/matrix conversion directions, the follow cross-product basis, and the Q12 transform composition are now statically encoded.
2. Isolate the remaining projection producers. The gated perturbation now proves that viewport input word `6` changes prepared view/object packets. Camera `+0x40c` still needs a producer trace that exercises its guarded assignment, increment/decrement flags, reset, and timed delta updates; the newly decoded framing globals and rotation/axis controls likewise need a gameplay run that exercises their flags. Their relationship to viewport word `7` must remain separate from the proven word-6 dataflow.
3. Enumerate the `+0x504` mode values and transitions in normal follow, camera-point, death, replay, menu, and two-player paths.
4. Reproduce the original fixed-point multiply, divide, shift, saturation, and trigonometric lookup behavior. Ordinary floating-point math will drift in camera smoothing and orientation.
5. Use the now-established input/player/camera frame contract for stationary projection calibration: vary one camera/viewport input at a time and compare known basis-object coordinates, clipping, depth, and present-to-present output. The live word-6 perturbation already closes the dataflow part of this gate.
6. Recover the remaining scene/object transform handoff only far enough to validate one visible object; leave asset disk-format ownership to the asset-runtime session.
7. Validate viewport selection and present behavior in split-screen or alternate modes, where one gameplay update may feed multiple viewport renders.

For the larger faithful C++ engine, the camera work is only one of four
runtime contracts that must meet at the same frame IDs:

```text
simulation clock/input
    -> player/object state
    -> camera state
    -> scene snapshot and render packets
    -> fixed-point view/projection conversion
    -> material/texture/backend submission
    -> present
```

The remaining engineering gates are therefore:

1. **Clock and ownership.** Implement one authoritative simulation/update
   clock, keep `Game_LevelLoop` ordering distinct from the `Flip` clock, and
   define pause, menus, level transitions, and split-screen behavior. The
   native driver must be compared against traces keyed to the confirmed
   present boundary, not to message-pump frequency.
2. **Camera completion.** Connect the recovered mode-1 follow snapshot and
   transform interpolation to the remaining gameplay-owned anchor/collision
   producer, then add point mode, death mode, runtime shake parameter sets, and
   projection calibration as separate replayable stages. The normal shake
   composition and decay order is already encoded; its live parameter
   producers still need validation.
3. **Scene/render handoff.** Bind runtime scene objects to asset-backed model
   instances, reproduce traversal/order and object visibility, and validate one
   actor and one static object from camera state through transformed vertices
   to a backend draw packet. The player `Render_SubmitActor` path is now
   sampled live; exact object-list ownership and the downstream geometry
   packet remain open.
4. **Renderer fidelity.** Recover the exact matrix convention, viewport
   normalization, clipping/depth behavior, texture/palette conversion,
   material flags, transparency, draw ordering, and only then select a native
   graphics API. DirectDraw itself is an output boundary, not the engine
   semantics to reproduce.
5. **Gameplay producers.** Close the already identified physics/collision,
   animation, trigger/script, and runtime-object lifecycle seams. Camera
   parity is not visually meaningful if the player pose, object transforms, or
   collision response diverge before submission.
6. **Non-render subsystems.** Add UI/menu state, audio/music, save/profile
   behavior, and platform/input details for a whole-game recreation. These can
   follow the first playable vertical slice, but they must not be mixed into
   the camera’s simulation contract.
7. **Parity harness.** Build a deterministic retail/native trace comparator
   for input, simulation tick, camera raw fields, viewport records, object
   packets, and present count. Every promoted semantic should have a fixture,
   a confidence level, and a falsifier; screenshots alone cannot identify
   whether a mismatch came from timing, camera state, or rasterization.

The next highest-value probe is now a stationary basis-object calibration
using the established frame fixture. Hold the player and camera still, vary
word `6`, camera `+0x40c`, and the display/aspect state one at a time, and
record `Render_SetViewProjection`, transformed geometry, clipping, and
`Render_Present`. This should resolve screen-axis signs, depth convention,
and the remaining `+0x40c` producer without expanding into the whole
DirectDraw backend.

## Open questions and falsifiers

- `0x0040f850` is recovered from a runtime vtable and raw disassembly, but Ghidra’s function table does not provide an ordinary function record for its exception-heavy prologue.
- The raw `tony-view-probe`, `tony-view-perturb`, and `tony-actor-probe` are
  implemented and unit-tested. `camera-projection6` reached live Warehouse
  rendering and recorded 1,200 gated word-6 mutations alongside 1,200
  geometry submissions. The experiment proves the word-6 dataflow, but a
  stationary basis-object run is still required for final screen/depth
  semantics and for the independent `+0x40c` producer.
- `+0x40c` remained raw `12` through the dedicated `O` camera-action phases (`0x0100` / key `24`). Static code nevertheless shows direct camera-side control through `DAT_00524AA4`, `DAT_0056B008`, `DAT_0056B018`, `DAT_0056AFF8`, and the `+0x410/+0x414` timer/delta pair. The native `CameraViewportParameterControlRaw` now preserves the exact restore/decrement/increment/reset/timer ordering, and the camera probe records the raw control globals at update entry. A run that changes actual projection scale without changing `+0x40c` would falsify its current viewport/framing interpretation and move the projection search to another viewport/global field; a run that exercises those flags would close the producer side of this field.
- The default Warehouse motion capture remained in mode `1`, but the dedicated camera-action capture also observed valid mode-25 intervals. Modes `2`, `23`, and `24` remain unobserved in a live level transition; mode `25` is statically mapped to the alternate-follow handler and dynamically observed, but its transformed-offset branch is not yet live-sampled.
- The camera object’s four-word embedded payload is strongly quaternion-like from the half-angle constructors, composition helper, and recovered matrix inverse. The dynamic `+0x34`/`+0x54` comparison now establishes the renderer record transpose; a controlled basis-object submission remains for world-axis handedness, screen-axis signs, and clip/depth behavior.
- `0x0041c2d0` may run other shell/session modes without entering `Game_LevelLoop`; a callback trace in menu and level modes would strengthen the loop relationship.
