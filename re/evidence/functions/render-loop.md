# Renderer / camera reconnaissance

Status: Session G stop condition met; gameplay render path bounded at object submission and present

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

Image: PE32/i386 `THawk2.exe`, image base `0x00400000`

## Result

The strongest normal-frame boundary is the game-owned callsite `0x004d0ca4`:

```text
mov eax, ds:0x029da3ac
push eax
mov edx, [eax]
call [edx+0x2c]       ; IDirectDrawSurface7::Flip
```

At runtime, `DAT_029da3ac` pointed to `0x02fc6f68`, whose vtable was
`0x78dcc720 <ddraw_surface7_vtbl>` and whose slot `+0x2c` was
`0x78dbef20 <ddraw_surface7_Flip@12>`. This is therefore a real displayed-frame
boundary, rather than the convenient message-pump sampler at `0x004f7ce0`.

The game-owned wrapper immediately containing the call is `0x004d0c30`.
It performs the final renderer/device work, calls the surface vtable slot with
`(0, 1)`, and handles `DDERR_WASSTILLDRAWING` (`0x887601c2`) and recovery.
For a frame clock, prefer the exact callsite `0x004d0ca4` if the tracer wants
the boundary itself; use `0x004d0c30` if a function-level hook is more practical.

## Timed runtime comparison

Session `render-loop5` used this same binary build in the isolated headless
Wine profile. With the game on the main menu, two software breakpoints were
armed together: `0x004f7ce0` incremented `$pump` and continued, while
`0x004d0ca4` incremented `$flip` and stopped at `$flip == 120`. The wall-clock
markers were Unix nanoseconds
`1787700862163910572` and `1787700872791209008`, an elapsed interval of
10.627298436 seconds.

The result was:

```text
present/Flip callsite 0x004d0ca4: 120
message-pump callsite 0x004f7ce0: 769
```

This is the direct comparison requested for the old sampler: it can run many
times between displayed frames, whereas the Flip callsite terminated the
same bounded 120-present window. A separate Warehouse gameplay capture also
reached 120 Flip hits through the inner level path (`0x0042ffc0`), confirming
that the boundary remains active outside the frontend.

## Supported ordering

The observed/static ordering is:

```text
outer shell timing/input
    ↓
frontend callback / level launcher
    ↓
Game_LevelLoop 0x0046a3a0
    ↓
message pump 0x004f7ce0 and timing/input/object updates
    ↓
double-buffer preparation 0x0042fed0
    ↓
camera update 0x0040f850
    ↓
view/world render 0x00467c90
    ↓
view setup 0x0045e8e0
    ↓
object list 0x00460a90
    ↓
object/model submission 0x004604f0
    ↓
part submission 0x00461b10
    ↓
polygon/backend transform 0x004d14d0
    ↓
buffer commit/finalization 0x0042ffc0
    ↓
render command processing 0x004d3160
    ↓
present wrapper 0x004d0c30
    ↓
IDirectDrawSurface7::Flip callsite 0x004d0ca4
```

The exact order of animation/UI helpers around the camera call is less
important than the stable boundary: in `Game_LevelLoop`, `0x0046a0f0` calls
`0x00468520`, which prepares the view and reaches `0x00467c90`; the loop then
calls `0x0046a1a0`, whose path reaches `0x0042ffc0` and the final present.

## Bounded backend anchor

The backend was followed only far enough to separate submission from display:

- `0x0042fed0` (source string `H:\TonyHawk\Pc2\db.cpp`) synchronizes the
  current double-buffer state, checks `Still_Rendering_after_drawsync` and
  `Still_RenderNow_Set`, clears/reinitializes the command-list storage through
  `0x004d3b00`, and updates the render timestamp/state.
- `0x004d14d0` is the game-owned geometry submission/transform stage. It
  converts the fixed-point model data into float working values and queues
  backend work.
- `0x004d3160` walks the linked render-command list, changes D3D state through
  the device vtable, and dispatches command types. It is downstream of object
  submission but upstream of presentation.
- `0x004d0c30` performs the final device/surface work and invokes the
  DirectDraw `Flip` slot at `0x004d0ca4`.

`0x004d0bc0` is called at the beginning of both the inner commit path and the
outer frontend frame tail with the current render target/state arguments. It
is retained as a frame-target setup helper, not promoted as a literal clear
operation because the available evidence does not isolate its exact surface
operation. This keeps the backend conclusion at the useful boundary without
reconstructing DirectDraw internals.

## Promoted functions and callsites

### `0x004d0ca4` — `Render_PresentFlipCallsite`

Name class: semantic name, high-confidence. The external runtime symbol
`ddraw_surface7_Flip@12` is implementation/import evidence, not an original
game symbol.

Evidence:

- Static behavior: indirect call through `DAT_029da3ac` vtable slot `+0x2c`.
- Static caller: `0x004d0c30`; the outer frame path reaches it from
  `0x00430510`, and the level path reaches it from `0x0042ffc0`.
- Runtime identity: the object and vtable resolved to Wine's DirectDraw
  Surface7 object and `Flip` implementation.
- Runtime frequency: 120 hits during a menu capture, matching 120 hits at
  `0x00430510` and `0x004d0c30`; a separate Warehouse level capture produced
  120 hits while `0x0042ffc0` was the active inner-loop path.
- Interpretation: one normal displayed frame per hit in both frontend and
  Warehouse gameplay captures.

Confidence: high for the normal display-frame boundary.

Possible falsifier: a different runtime mode may present through a separate
`Blt`/`Flip` callsite, or a device-reset/loading path may produce extra flips.
The candidate should be re-counted for any mode that uses a different surface
or presentation policy.

### `0x004d0c30` — `Render_Present`

Name class: semantic name, high-confidence; source evidence points to the
Direct3D renderer rather than an embedded original function name.

Evidence:

- Static behavior: performs final renderer/device work, optionally invokes a
  surface method on `DAT_029da3b0`, then calls `DAT_029da3ac` vtable `+0x2c`
  with `(0, 1)` at `0x004d0ca4`.
- Static callers: the outer frame tail in `0x00430510` and the inner level
  commit path in `0x0042ffc0`.
- Runtime frequency: 120 menu hits and 120 Warehouse-level hits in the
  corresponding captures.
- Relevant evidence: the surface object/vtable resolved to
  `ddraw_surface7_vtbl` and `ddraw_surface7_Flip@12`.

Confidence: high as the game-owned present wrapper.

Possible falsifier: a caller that invokes the wrapper for synchronization
without presenting, although the observed vtable call and one-per-frame count
make that unlikely for the normal path.

### `0x0046a3a0` — `Game_LevelLoop`

Name class: existing cross-build/semantic name, supported by the earlier
loop evidence and this session's call graph.

Evidence:

- Static behavior: loops while `DAT_0056a8d0 == 0`, performs message/input,
  timing, object/game updates, buffer preparation, render preparation, and
  the post-render commit path.
- Static callees in the render tail: `0x0042fed0`, `0x0046a0f0`,
  `0x0046a1a0`, and `0x0046a250`; the `0x0046a0f0` path reaches the camera and
  world-render functions below.
- Runtime: directly invoked after loading Warehouse (level index `12`), then
  the inner level path reached `0x0042ffc0` and the exact Flip callsite.

Confidence: high for the active gameplay/session loop.

Possible falsifier: a level mode that delegates rendering to another loop;
the normal Warehouse path renders directly through this loop.

### `0x0041c2d0` — `Game_MainLoop`

Name class: existing cross-build/semantic name, high-confidence as the outer
shell loop.

Evidence:

- Static behavior: initializes input, performs shell timing/subsystem work,
  invokes a virtual callback at `(*piVar3 + 4)`, tests the callback/session
  result, then reaches `0x00430510` for the outer frame's render/present tail.
- Relationship: frontend launch code at `0x004544a0` enters `0x0046a8d0`,
  which calls `0x0046a3a0` while the selected level is active. Thus the level
  loop is nested under the frontend callback/launcher path, not a wholly
  unrelated second runtime.
- Runtime: `0x00430510` and its `0x004d0c30`/`0x004d0ca4` path produced the
  menu-frame count; active Warehouse gameplay produced the corresponding
  inner-loop count.

Answer to the loop question: `Game_LevelLoop` renders and presents directly
through `0x0042ffc0`; `0x0041c2d0` owns the broader shell/frontend loop and
presents its own frontend frames after its callback. They are nested runtime
contexts during level launch, with a shared backend present boundary.

Confidence: high for this relationship.

Possible falsifier: a launch mode that calls the level loop outside the
`0x0041c2d0` callback; that would change nesting, but not the observed render
tail or Flip identity.

### `0x0040f850` — `Camera_Update`

Name class: provisional semantic name. The method is a camera-vtable entry;
`camera.cpp` is embedded as a source-path string, but no original function name
was recovered for this exact address.

Evidence:

- Static behavior: method uses `ECX` as a camera object, copies camera vectors,
  reads the tripod/player pointer at `this+0x3a4`, calls camera/trigonometric
  helpers, updates camera position/orientation state, and dispatches by the
  camera mode/state at `this+0x504`.
- Static callers: reached from the `0x0046a0f0` render-preparation path through
  the camera object/vtable; helper `0x0040e090` is called from this method and
  is not the primary entry point.
- Runtime: breakpoint hit 60 times in a settled Warehouse capture. At one
  hit, `ECX=0x05f39c7c`, `[ECX]=0x005184b8`, and `*(ECX+0x3a4)=0x05f326e4`.
  The player object at `0x05f326e4` also held the camera pointer at `+0x29b0`.
- Relevant strings: `H:\TonyHawk\Pc2\camera.cpp` at `0x00524af0` and the
  camera constructor path at `0x0040b650`.

Confidence: high for a per-frame gameplay camera update method; medium for
the exact semantic breadth of every camera mode handled by the method.

Possible falsifier: the vtable entry could be a mode-specific camera process
that is not called in every runtime mode. It should be rechecked in menus and
alternate camera modes before treating it as universal.

### Camera object model

The minimal runtime model is:

```text
player/root global: DAT_0056a858
camera object:      *(player + 0x29b0)
tripod/player link: camera + 0x3a4
position:           camera + 0x08, +0x0c, +0x10
view/target inputs: camera + 0x3c0 .. +0x3c8
orientation state:  camera +0x448 .. +0x468,
                   copied/processed through +0x470 .. +0x47c
```

The position and transform state are integer/fixed-point oriented: the camera
helpers use shifts, shorts, and fixed-point trigonometric operations. Do not
interpret these fields as IEEE floats without a separate conversion proof.
The runtime example above is allocation-dependent; the pointer relationships
and offsets are the evidence that should be reused.

`0x0045e8e0` is the per-viewport view/projection setup reached from the world
path. It consumes the viewport record at `DAT_0055f7c8 + index*0xa4`, stores
matrix-like fixed-point arrays around `DAT_005620c0`/`DAT_005620e8`, and sets
the active renderer viewport. An explicit FOV field was not isolated in this
session; projection parameters remain unresolved.

### `0x00467c90` — `Render_ViewWorld`

Name class: provisional semantic name.

Evidence:

- Static behavior: accepts the current viewport/view inputs, sets the active
  view globals, calls `0x0045e8e0` for view/projection setup, renders scene and
  skater-related structures through `0x0045f530`, temporarily adjusts the
  player object for a render pass, then calls `0x00460a90` on the main object
  list and additional scene render helpers.
- Static caller: `0x00468520`, reached from `0x0046a0f0` in the level loop's
  render preparation.
- Runtime frequency: 30 hits in a Warehouse capture; the object-list renderer
  below hit 27 times in the same bounded experiment, consistent with state or
  transition frames that skip object traversal.
- Relevant source family: downstream M3D callees and the fixed-point viewport
  setup in `0x0045e8e0`.

Confidence: high for a viewport/world-render entry point; medium for whether
it is the single root of every scene-render pass.

Possible falsifier: a separate level-specific world traversal bypassing this
function; search alternate `0x0045e8e0` callers if another mode requires it.

### `0x00460a90` — `Render_ObjectList`

Name class: provisional semantic name.

Evidence:

- Static behavior: receives a linked object-list head, tests object flags,
  sets per-object render globals, calls `0x004604f0`, and follows the next
  object pointer at `object+0x20` until null.
- Static caller: `0x00467c90` at its main scene-list render branch.
- Runtime: `DAT_0056a960` was the observed list head; its value was the current
  player/root object in the captured Warehouse state, and the linked-list
  linkage was observed at `+0x20`. Breakpoint hit count was 27 in the bounded
  run.

Confidence: high for a game-owned linked object-list traversal; medium for the
complete meaning of every list head and object flag.

Possible falsifier: `DAT_0056a960` may be a mode-specific list rather than the
whole scene; verify another level or camera viewport before generalizing it.

### `0x004604f0` — `Render_SubmitObject` / `M3D_RenderSuperitem`

Name class: provisional semantic name; the embedded source path supports the
M3D family but not the exact original function name.

Evidence:

- Static behavior: source/assert string references `H:\TonyHawk\Pc2\m3d.cpp`;
  reads object flags, model number/type, position, orientation fields, and
  model/geometry pointers; transforms each model part and calls `0x00461b10`.
- Static caller: `0x00460a90` at `0x00460c67`.
- Runtime: first observed object argument was `0x05f326e4`, the current
  player/root object. Its vtable was `0x00518dd8`; position-like fields were
  present at `+0x08`, `+0x0c`, `+0x10`, and orientation fields at
  `+0x118` onward. The call returned to `0x00460c6c`.
- Runtime frequency: 27 hits in the bounded Warehouse run.

Confidence: high for one object/model submission stage; medium for whether
the first object is always the skater rather than simply the first list item.

Possible falsifier: the object may be a generic superitem wrapper whose model
parts are not the visible skater in every level. A second object capture would
separate those cases.

### `0x00461b10` and `0x004d14d0` — part/polygon submission

`0x00461b10` is a small, high-confidence bridge: it combines the part flags
with renderer state and calls `0x004d14d0(param_1, flags)`.

Evidence for `0x00461b10`:

- Static caller/callee: called by `0x004604f0` once per submitted model part;
  calls `0x004d14d0` after adding the renderer's global material/visibility
  flags.
- Runtime: its downstream live `0x004d14d0` hit received geometry data and
  flags `0x800`, confirming that this is the object-to-backend bridge rather
  than a bookkeeping-only helper. It was not independently counted, so its
  exact per-frame hit count is not promoted here.
- Confidence: high for the part-submission bridge.
- Possible falsifier: if a later caller bypasses this helper for a primitive
  class, the path describes model-part submission but not every draw type.

`0x004d14d0` is a provisional `Render_SubmitPolygon`/backend-transform name.
It is the last game-owned transform/draw stage observed before command-buffer
processing, not the DirectDraw present itself.

Evidence for `0x004d14d0`:

- Static behavior: consumes geometry/face data and draw flags, reads the
  active viewport/clip state at `DAT_00563a38`, converts fixed-point/16-bit
  values through renderer constants into floats, transforms the data, and
  calls lower-level renderer routines including `0x004d29e0` and
  `0x004d18b0`.
- Static caller: `0x00461b10`; the object path is therefore
  `0x004604f0 -> 0x00461b10 -> 0x004d14d0`.
- Runtime: at a live breakpoint, the first argument was geometry data at
  `0x05aed6ec`, the second argument was flags `0x800`, and the data began
  `0x002a0048 0x001d001d 0x00a0b000 ...`. The optimized/hand-written path
  did not preserve a useful symbolic backtrace, but the static caller chain
  is unambiguous.
- Relevant source family: `H:\TonyHawk\Pc2\Pc\d3dfunc.cpp` is present in the
  binary's source strings for the backend family; the object stage contains
  the `m3d.cpp` source string.

Confidence: high for a game-owned geometry submission/transform stage;
medium for the exact primitive type represented by its first argument.

Possible falsifier: a later command-list stage may defer the actual draw until
`0x004d3160`; in that case `0x004d14d0` is the submission boundary, not the
hardware execution point. It remains downstream of object transforms and
upstream of the final Flip, which is the useful distinction for this session.

## Loop relationship and frame-clock comparison

`0x004f7ce0` is the Windows message-pump callsite inside `Game_LevelLoop`.
It was useful for sampling gameplay state but is not a rendered-frame
boundary: the loop can pump messages and perform updates before a frame is
committed, and menu/transition work can call it independently of a completed
present.

The replacement candidate is `0x004d0ca4` (or wrapper `0x004d0c30`). In the
captured menu interval, the message-pump/outer-loop and Flip counts happened
to match at 120, but the identity proof comes from the vtable call to
`IDirectDrawSurface7::Flip`, and the same exact Flip callsite continued at
120 hits during Warehouse gameplay. This makes the Flip callsite the
trustworthy rendered-frame clock for the normal path.

## Naming and evidence boundaries

Embedded/original evidence consists of the source paths
`camera.cpp`, `m3d.cpp`, and `Pc\d3dfunc.cpp`, plus the runtime DirectDraw
implementation symbol. `Game_MainLoop` and `Game_LevelLoop` are existing
cross-build/semantic names supported by the earlier loop evidence. Names such
as `Camera_Update`, `Render_ViewWorld`, `Render_ObjectList`,
`Render_SubmitObject`, and `Render_SubmitPolygon` are deliberately marked
provisional/semantic; they should not be promoted to original symbols without
stronger source or cross-build evidence.

This session records only the runtime object pointer relationship needed to
connect scene traversal to rendering. It does not assign ownership of that
object to the disk asset format or reverse the loader, leaving that boundary
to Session E.

## Remaining falsifiers / bounded follow-up

- Count `0x004d0ca4` in any mode that uses a different display surface or
  resize/device-reset path; those may legitimately have extra presentation
  calls.
- Sample a second level/object to distinguish the main player object from a
  generic first scene-list item.
- Isolate the projection/FOV field if a later camera task needs it; it is not
  required for the frame boundary or world-render entry point.
