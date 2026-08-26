# Session F — skater animation pipeline

Status: the complete minimal pipeline is confirmed by static control-flow, field-write, asset, pose-cache, and live Warehouse player evidence. A live player request/timing trace was obtained; a clean input-driven left/right turn trace remains the only harness gap.

Build: retail `THawk2.exe`, SHA-256 `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`, image base `0x00400000`.

Worktree: `/home/joao/dev/OpenTony-animation`, branch `re/animation`.

## Result

The minimal pipeline is:

```text
skater physics / steering state
    ↓
animation request wrapper (0x004903f0 / 0x00490420 / 0x00490450 / 0x00490480)
    ↓
CSuper::RunAnim (0x00480730)
    ↓
current animation and frame fields on the skater object
    ↓
CSuper animation update (0x00480950, dispatched by 0x00480fa0)
    ↓
Decomp_GetAnimTransform (0x00430920)
    ↓
hierarchy/calculation-order data used to produce bone transforms
```

This is independent of ollie state. No ollie-specific semantics were needed.

The useful retail/source identity anchors are the `CSuper` names in
`build/assets/all-pkr/files/data/EDITEDMAP.TXT`: `RunAnim`, `CycleAnim`,
`UpdateFrame`, `SetAnimOrder`, and `Decomp_GetAnimTransform`. The PC image
addresses above are the addresses used by the decompilation project for this
retail executable; the map is supporting source-name evidence, not a license
to mix addresses from another regional build.

## Selection / controlled states

`0x00492f20` is the useful controlled-state selector in the skater steering path. It reads the steering/turn inputs at the skater object, examines the current animation at `player+0xf6`, and requests animation changes through `0x00490450`:

| Recovered animation ID | Recovered name | Selector evidence |
| ---: | --- | --- |
| `0` | `SKATER_ANIM_IDLE` | `0x004903f0(0, ...)` is used when grounded push/step animation completes or the stable grounded state is restored. |
| `1`–`3` | `KICK_OFF`, `KICK_LOOP`, `KICK_TO_IDLE` | `0x0049b010` enters the push sequence with `0x00490420(1, ...)`, then handles `1 → 3 → 0`; the recovered enum names are in `SKATE2.TAG`. |
| `6` | `SKATER_ANIM_TURN_LEFT` | `0x00492f20` requests it through `0x00490450`. |
| `7` | `SKATER_ANIM_TURN_RIGHT` | `0x00492f20` requests it through `0x00490450`. |
| `9` / `10` | `CROUCH_TURN_LEFT` / `CROUCH_TURN_RIGHT` | Same selector, only when the crouched steering branch is active. |

The sign-to-left/right branch is coordinate-dependent: in the observed code, the negative and non-negative steering branches select the two IDs in opposite order depending on the crouch/board-mode branch. The ID/name mapping above is reliable; do not infer a world-axis sign from the selector alone.

There is no separate top-level `SKATER_ANIM_ROLL` name in the recovered enum. In the deliberately simple straight-ground case, movement is represented by the grounded physics/push sequence and then the stable ID `0`; the push animation is the visible transition into that state. This is why a “rolling straight” snapshot should distinguish gameplay velocity from animation identity rather than expect a unique roll ID.

The request wrappers are small and reproducible:

* `0x004903f0`: set `+0x108 = 0x10000`, then `RunAnim(anim, 0, -1, -1)`.
* `0x00490420`: set `+0x108 = 0x10000`, then `RunAnim(anim, start, -1, -1)`.
* `0x00490450`: set `+0x108 = 0x10000`, then `RunAnim(anim, start, end, -1)`.
* `0x00490480`: set `+0x108 = 0x10000`, then pass the full animation request to `RunAnim`.

`0x00480730` is therefore the animation selection/request entry point, not the per-frame advancement routine.

### Steering frame behavior

The steering selector does more than choose an animation ID. In the normal
turn branch it maps the steering magnitude to a target frame, then advances
the current frame toward that target with `0x00492ed0`:

* distances greater than 12 advance by 5 frames;
* distances greater than 3 advance by 3 frames;
* smaller distances advance by 1 frame.

The normal turn range is capped at frame `0x16` (22). The crouched turn
branches use caps `0xf` (15) or `0xc` (12), depending on direction/context.
After changing `+0xf4` directly, the selector requests
`RunAnim(turn_id, frame, frame, -1)`. Equal start/end frames intentionally
freeze the requested pose; this is why turning is not just ordinary playback
of a looping clip.

When steering returns to zero, IDs `6`/`7` request idle ID `0`. Crouched
turn IDs `9`/`10` request ID `8` over frames `0x13..0x1a`, with the alternate
endpoint byte supplied by the caller. The sign-to-left/right
mapping is coordinate/context dependent; only the ID mapping, not a world-axis
sign, should be copied into a reconstruction.

The push path at `0x0049b010` requests ID `1` (`KICK_OFF`), advances through
the push sequence, and requests ID `3` (`KICK_TO_IDLE`) and then ID `0` when
the sequence finishes. It also raises the playback rate to `0x14000` in
the observed fast push cases. Straight movement therefore has to be modeled
as gameplay velocity plus the current animation state; the recovered enum
has no top-level `SKATER_ANIM_ROLL`, and no separate straight-roll selector
was found. A stable ID `0` after pushing is a static-path conclusion for the
simple grounded branch, not a claim about every grounded state.

The complete static enum-name inventory is present in `SKATE2.TAG` under
`mech.h`; it is useful for future selector work, but the tag's source line
numbers are not a verified PC numeric enum layout. The names are:

```text
360_POP_SHOVEIT, 360_VARIAL, 360FLIP, 50_50_GRIND_IDLE, 5_0_GRIND_IDLE,
BACK_FOOT_IMPOSSIBLE, BACKSIDE_BOARDSLIDE_IDLE, BAIL_2_FALL, BAIL_NUTTING,
BENIHANA, BLUNT, BORED, BRAKE, CAR_SMASH, CROOKED_FS, CROOKEDGRIND, CROUCH,
CROUCH_TURN_LEFT, CROUCH_TURN_RIGHT, CROUCHED_TO_IN_AIR_IDLE, DROPIN,
FACESMASH, FALLBACK, FAST_PLANT, FINGERFLIP_TO_INDY, FLAIL_HIT_BACK,
FLAIL_HIT_CHEST, FOOT_PLANT, FRONT_FOOT_IMPOSSIBLE,
FRONTSIDE_BOARDSLIDE_IDLE, GETUP, HANDPLANT, HARDFLIP, HEELFLIP, IDLE,
IDLE_TO_IN_AIR_IDLE, IMPOSSIBLE, IN_AIR_IDLE, INDY, INDY_NOSEBONE,
INIT_50_50, INIT_BS_GRIND, INIT_FS_GRIND, INIT_NOSE_GRIND, INIT_TAIL_GRIND,
JAPAN_AIR, KICK_LOOP, KICK_OFF, KICK_TO_IDLE, KICKFLIP, KICKFLIP_TO_INDY,
LAND, LAND_TO_CROUCHED, LOSE, MADONNA, MELON, NOLLIE, NOSEGRIND_IDLE, OLLIE,
ROCKETAIR, SEXCHANGE, SMITH_FS, SMITHGRIND, SPECIAL1, SPECIAL2, SPECIAL3,
SPECIAL4, SPECIAL5, SPLAT, STALEFISH, STEP_OFF_BOARD, STEP_ON_BOARD, SWITCH,
SWITCH_CROUCH, TAILGRAB, TURN_LEFT, TURN_RIGHT, VICTORY, VICTORY2,
WALLRIDE_LEFT, WALLRIDE_RIGHT
```

Static callers outside this controlled subset cover crouch/air/landing,
grinds, bails, and tricks. They use IDs such as `4`, `5`, `8`, `0xe`, `0x22`,
`0x31`, `0x82`, `0x83`, `0xa2`, `0xa3`, `0xaf`, `0xb5`, `0xd8`, and `0x91`.
Those call sites prove that selection is distributed across physics/state
updates, but they are deliberately not assigned names here: several are
ollie/trick or collision semantics owned by other sessions, and the numeric
mapping is not needed to recreate the minimal idle/push/turn pipeline.

## Current animation and timing fields

The decompilation of `0x00480730` shows these writes on the same skater/`CSuper` object. The offsets are relative to the object passed in `ECX`:

| Offset | Interpretation | Evidence |
| ---: | --- | --- |
| `+0xf6` | current animation ID | First write in `RunAnim`; also read by steering, physics, and `Decomp_GetAnimTransform`. |
| `+0xf4` | discrete current frame | Initialized from the requested start frame and advanced by `0x00480950`. |
| `+0x104` | fractional frame accumulator | Low 16 bits of the 16.16 frame accumulator; reset on request and written each update. |
| `+0x108` | playback rate / frame increment | Request wrappers use `0x10000`; `0x00480950` multiplies it by the global frame scale before advancing. |
| `+0x106` | frame count for the selected animation | Loaded from the animation table entry at request time. |
| `+0xf8` | playback mode | Selects stop/range, loop, ping-pong, and reverse behavior in `0x00480950`. |
| `+0x100` | playback direction | Set to forward/reverse from the requested start/end range. |
| `+0x101` | endpoint frame | Stored by `RunAnim` and used by the mode logic. |
| `+0x102` | alternate/next endpoint | If non-negative, replaces `+0x101` when the current endpoint is reached and reverses direction; this is endpoint-swap/ping-pong state, not a blend amount. |
| `+0x107` | finished / endpoint flag | Set when the requested range is already at its endpoint and updated by the playback-mode logic. |
| `+0x114` | saved/original start frame | Written during request setup and used by reverse/endpoint transitions. |

The pose-side fields that complete the object contract are:

| Offset | Interpretation | Evidence |
| ---: | --- | --- |
| `+0x1f` | model/animation part-set selector | Indexes the global model-region and animation-table arrays. |
| `+0xe8` | calculation-order array | Allocated by `Decomp_GetAnimTransform`; contains the root-first hierarchy order. |
| `+0xec` | decompressed per-part transform cache | Allocated for the selected skeleton and returned by `Decomp_GetAnimTransform`. |
| `+0xf0` | hierarchy root index | Derived from parent links and stored while building the calculation order. |
| `+0xf2` | cached animation ID for pose decoding | Prevents re-decoding when the animation ID is unchanged. |
| `+0xf3` | cached frame for pose decoding | Prevents re-decoding when the frame is unchanged. |
| `+0x138` | animation-part tween/pose buffer | Allocated during model rebuild; filled from neighboring animation frames. |
| `+0x13c` | model-part mapping records | Stores the animation/model part correspondence used by the pose path. |
| `+0x140` | model-part descriptor/list | Set from the model resource descriptor. |
| `+0x144` | attached model/resource descriptor | Set when the model binding is first built. |
| `+0x14c` | animation-order dirty flag | Set by `SetAnimOrder`. |
| `+0x150..` | model/render part ID → animation-part order map | `SetAnimOrder(render_id, render_order)` writes one byte per model/render part. |

`0x00480950` combines `+0xf4` and `+0x104` as a 16.16 frame value, adds or subtracts `(+0x108 * DAT_0056865c) >> 8`, writes the fractional part back to `+0x104`, and writes the integer frame back to `+0xf4`. Its mode switch handles stop, loop, ping-pong, and reverse endpoint behavior. This is the reproducible time/frame advancement path.

The recovered debug/source symbol inventory gives the corresponding retail
member names: `mAnim` (`+0xf6`), `mAnimMode` (`+0xf8`), `mAnimDir`
(`+0x100`), `mFrame` (`+0xf4`), `mAnimFinished` (`+0x107`),
`mAnimSpeed` (`+0x108`), `mNumFrames` (`+0x106`), and `mFrameFrac`
(`+0x104`). The pose-cache names are also present: `mpCalculationOrder`
(`+0xe8`), `mpDecompressedFrame` (`+0xec`), `mDecompressedAnim`
(`+0xf2`), `mDecompressedFrame` (`+0xf3`), and `mpPoseBuffer` (`+0x138`).
These names come from `SKATE2.TAG` and corroborate the field roles; the PC
offset mapping is established by the decompiled reads/writes above.

The endpoint behavior is now resolved from the decompilation rather than left
as a transition guess. Before advancing, mode `0`/`2` checks whether the
current frame has reached `+0x101`. If `+0x102 < 1`, it sets `+0x107` and
stops. Otherwise it swaps `+0x101` and `+0x102` and negates `+0x100`. For
example, the crouch-turn restoration call is
`RunAnim(8, 0x13, 0x1a, 0x13, ...)`, which traverses frames 19 through 26 and
then back to 19. `+0x102` is therefore an alternate/next endpoint. Mode `1`
wraps at the selected animation's frame count; mode `3` has a separate
`+0xfa/+0xfc/+0xfe` ping-pong clock/range path; mode `4` reverses and swaps
against the saved start at `+0x114`.

`RunAnim` also has a precise invalid-ID behavior: it asserts/logs “Bad anim
sent to RunAnim”, replaces an out-of-range ID with `0x2e`, then continues
using that animation's frame count. `CycleAnim` sets mode `1`, starts at frame
zero, copies the requested direction, clears the fraction and finished flag,
and uses the selected animation's frame count. These details should be part
of a compatibility implementation, not approximated as a generic clip
player.

## Animation resource identity

The animation resource is table-backed rather than a separately proven per-player pointer:

1. `Front_LoadGame` at `0x004524a0` requests the `sk2anim` resource with `0x004b46a0("sk2anim", 1)`.
2. `0x004b46a0` looks up or loads `sk2anim.psx`, stores the loaded resource in the global resource-slot table at `DAT_0056d440 + slot*0x44`, registers it, and marks the slot loaded.
3. `RunAnim` uses the object's model/part-set byte at `+0x1f` to select the animation table at `DAT_0056d444 + part_set*0x11`, then indexes that table with `+0xf6` to obtain the selected animation's frame count and data entry.

Thus the minimal identity is `(object+0x1f model/part set, object+0xf6 animation ID)`, with the loaded `sk2anim.psx` resource resolved through the global model-region tables. No unverified “current resource pointer” field is introduced.

## Per-update and pose boundary

`0x00480fa0` is the object update dispatcher. The level loop's per-frame
`0x00469de0` reaches the active object list through `0x00480ff0`, which calls
`0x00480fa0` for each object. When an object's update flags permit animation,
`0x00480fa0` calls `0x00480950`; it then invokes two object callbacks through
the vtable. The timing routine is consequently separated from the pose/data
consumer. The base `CSuper` vtable entries observed at
`PTR_FUN_005193a8 + 0xc/+0x10` are no-op callbacks; the concrete pose work is
performed by the model-side rebuild path below rather than being inferred
from those callback slots.

`0x00430920` is the recovered `Decomp_GetAnimTransform` body. It consumes the same object's `+0x1f`, `+0xf6`, and `+0xf4`, validates the animation table and frame, resolves the compressed frame data, and—when needed—allocates/prepares hierarchy and calculation-order arrays. Its diagnostics include “Hierarchy required to decompress anim”, “Bad anim number in pSuper”, and “Bad frame number in pSuper”. That establishes the animation-to-hierarchy/bone-transform boundary without decoding the full animation packet format or renderer internals.

The next pose stages are also statically bounded:

1. `0x00480f50` initializes model-part mapping state when needed, calls
   `0x00465060` to prepare a tween buffer, and, when the object is active,
   calls `0x00461bf0` for object/world transform preparation followed by
   `0x00465300` for skeleton pose generation.
2. `0x00465060` obtains the current animation table entry and calls
   `0x00465110` for all animation parts. `0x00465110` chooses neighboring
   keyframes around the discrete `+0xf4` frame, computes a 12-bit blend
   fraction from the animation entry's high-word key spacing, and
   interpolates each part's three short transform components. The extracted
   `SK2ANIM.PSX` has that high word zero for every clip, so its current
   skater data uses one decoded sample per frame and the blend fraction is
   zero; preserve the general path because other PSX animation resources can
   use sparse key spacing.
3. `0x00465300` calls `Decomp_GetAnimTransform`, walks the animation's
   parent/calculation-order records, copies or composes local transforms, and
   writes the resulting per-part pose records. This is the skeleton/bone
   boundary needed by a faithful engine; the compressed packet decoder and
   renderer are intentionally out of scope here.

The source assets corroborate the structure. `SK2ANIM.PSX` and `SK2MOD.PSX`
both begin with the observed PSX resource header (`version=4`, `type=2`) and
declare 19 parts. Their matching `SK2ANIM.PSH` and `SK2MOD.PSH` files list the
same part names and parent hierarchy: pelvis/root, thighs, shins, shoes,
stomach, chest, arms, head, board, and two wheels. The animation and model
part lists are therefore deliberately name-compatible.

The binding direction is now exact. `0x00480d90` iterates model/render part
names in its outer loop and animation-part names in its inner loop; an exact
string match calls `SetAnimOrder` (`0x00480cd0`) with
`(model/render_part_id, animation_part_order)`. `SetAnimOrder` asserts both
bytes are below `0x13`, stores
`object+0x150[model/render_part_id] = animation_part_order`, and sets
`object+0x14c`. The model-side pose/render loops at `0x004604f0` and
`0x004610f0` then use that byte to select the corresponding animation pose
record while iterating model parts. This is a model-to-animation lookup, not
an animation-to-model map; preserving this direction is required for correct
limb/board correspondence.

`0x00464c00` builds the adjacent model binding state. It points `+0x140` at
the model descriptor records after the resource's two-byte part count,
allocates `+0x138` as one `0x18`-byte record per animation part, allocates
`+0x13c` as one `0x0c`-byte record per model part, clears six shorts in each
mapping record, and stores the matching model-descriptor index at record
offset `+0x0a` by comparing descriptor keys. The two mappings serve different
purposes: `+0x13c` binds model descriptors, while `+0x150` reorders animation
pose records for model/render consumption.

`SK2ANIM.PSX` SHA-256 is
`70cf84ac83e86a99815472325fa8ea875e01ef5c50152eff144731152f9ba1e5` and
`SK2MOD.PSX` SHA-256 is
`ee74c8325fdeb1cbb2e6f13af70c5b8c694b189f824c2a184abce7056f8ab8b3` in the
extracted asset set used for this investigation.

### Recovered SK2ANIM packet layout

The animation packet is sufficiently understood to implement a faithful
asset reader without involving the renderer. `SK2ANIM.PSX` has a standard
header (`version=4`, `type=2`), then a tag at file offset `0x47fc` with type
`0x2c` and payload size `0x8f1b0`. The tag payload begins at `0x4804`:

```text
u32 animation_count = 218
repeat animation_count:
    u32 relative_data_offset   // relative to payload start, 0x4804
    u32 frame_count_and_flags  // low 16 bits are frame count
```

For this retail `SK2ANIM.PSX`, every high 16-bit flags/key-spacing value is
zero. Frame counts range from 5 through 97. The controlled clips are:

```text
ID 0  -> 12 frames       ID 1  -> 10       ID 2  -> 29
ID 3  -> 31               ID 6  -> 23       ID 7  -> 23
ID 8  -> 27               ID 9  -> 29       ID 10 -> 28
```

The relative offset points to a stream containing 19 parts, with six channel
streams per part. The six signed 16-bit channels are three Euler rotation
values followed by three translation values. `Decomp_GetAnimTransform`
passes a destination stride of `19 * 6` shorts and calls the channel decoder
six times at offsets `0, 2, 4, 6, 8, 10` within each 12-byte part record.
Decoded records are row-major by frame and part:

```text
decoded_frame(frame)[part].rotation[3]
decoded_frame(frame)[part].translation[3]
```

The matching `SK2ANIM.PSH`/`SK2MOD.PSH` hierarchy is exact for this skater;
the parent indices recovered from the names are:

```text
0 pelvis       -> root       1 right_thigh -> 0       2 right_shoe  -> 3
3 right_shin   -> 1          4 left_thigh  -> 0       5 left_shoe   -> 6
6 left_shin    -> 4          7 stomach    -> 0       8 chest       -> 7
9 left_hand    -> 10         10 left_forearm -> 11   11 left_bicep  -> 8
12 right_hand  -> 13         13 right_forearm -> 14  14 right_bicep -> 8
15 head        -> 8          16 board     -> root    17 front_wheel -> 16
18 back_wheel  -> 16
```

The resource's runtime hierarchy array uses the same part numbering; the
pose builder derives a root-first calculation order from it rather than
assuming that file order is a valid traversal order.

The allocation is `(max_frame_count + 2) * part_count * 0x0c` bytes. The
first two per-part slots are reserved as the final 24-byte transform records;
the compressed stream is decoded starting at the third slot. The selected
raw frame starts at:

```text
cache + (current_frame + 2) * part_count * 0x0c
```

and the final per-part record contains a 3x3 matrix (nine signed shorts) plus
three translation shorts. The hierarchy pass fills the first two-slot prefix
in root-first calculation order and composes each child against its parent.

The channel control byte is `(interpolation_group << 4) | codec`:

* codec `0`: initial little-endian signed 16-bit value, then signed 16-bit
  endpoints; each endpoint is linearly expanded across `group + 1` samples;
* codecs `1..13`: initial little-endian signed 16-bit value, then signed
  deltas of `codec + 1` bits from a most-significant-bit-first packed stream;
  each delta is expanded across `group + 1` samples;
* codec `14`: one little-endian 16-bit constant repeated for every frame;
* codec `15`: zero for every frame and no payload bytes.

For an animation with `F` frames and `P = interpolation_group + 1`, the
decoder computes `full = (F - 1) / P` full endpoint segments and
`tail = (F - full * P) - 1` samples in the final partial segment. The first
sample is always the initial value. For codecs `0..13`, each full segment
then emits `group` integer intermediate values followed by its endpoint; the
tail emits `tail - 1` intermediates followed by its endpoint. Streams are
stored part-major and channel-major: all six channel streams for part 0,
then all six for part 1, and so on. A decoder should preserve the original
16-bit bit patterns and wrap/truncate each emitted result to `int16_t`.

The final partial segment uses the same integer interpolation, with signed
division truncating toward zero as in the original C/C++ implementation.
Packed delta streams consume the ceiling of the number of bits used; the
decoder returns the next byte when a stream ends mid-byte. Replaying the
decompiled decoder independently against all 218 indexed streams consumed
exactly 19 × 6 channels for every animation, exercised every codec nibble
`0..15`, and ended at `0x939b2`, exactly two bytes before the tag boundary
`0x939b4`. Those final two bytes are padding/trailer, not another animation.

The pose math uses the same fixed-point conventions as the packet data:
rotation shorts convert as `angle * (2*pi / 4096)`, matrix identity is
`0x1000`, and matrix/vector products shift right by 12. This is enough to
reproduce local matrices and hierarchy composition while intentionally
leaving model polygon decoding and renderer behavior to the model/renderer
sessions.

## Runtime check

The controlled headless sessions verified the input harness separately:
writing Return/Space/Pad2 scan-code bytes at the keyboard state buffer caused
`PCInput_BuildActionMask` (`0x004e42c0`) to produce action mask `0x40`, and
the frontend advanced through `MAIN_MENU`, `CAREER_SELECT`, and
`SKATER_SELECT`; earlier attempts also reached `LEVEL_SELECT`. The bounded
automated sessions then launched Warehouse and hit the real player object at
`0x05f39530`. A breakpoint on `RunAnim` filtered to the global player pointer
observed startup ID `94`, idle requests for ID `0`, the crouch-turn request
`8, 0, 26, 19`, and the live transition IDs `4` and `5`. A longer run also
observed the push sequence `1 → 3 → 0` with `+0x108 = 0x14000` during the
fast push, followed by repeated idle/push cycles; normal ticks showed
`+0xf4/+0x104` changing on that same object while `+0x108` remained at
`0x10000` outside the fast push. This directly confirms that the static
cursor fields are live gameplay state, not dead code or a menu-only object.

The injected accept-key schedule did not yet produce a clean left/right
steering interval in the same trace, so no runtime turn-ID claim is made. The
static selector still directly requests IDs `6`/`7` (and `9`/`10` for crouch),
and the existing Warehouse input evidence independently confirms the Left and
Right action bits. This is a bounded harness limitation, not an unresolved
selection or pose-path issue.

## C++ recreation contract

The smallest faithful replacement should preserve the following separation:

```cpp
struct AnimationCursor {
    std::uint16_t id;          // +0xf6
    std::int16_t  frame;       // +0xf4
    std::uint16_t fraction;    // +0x104, low 16 bits of 16.16 time
    std::uint32_t rate;        // +0x108
    std::uint8_t  mode;        // +0xf8
    std::int8_t   direction;   // +0x100: -1, 0, +1
    std::int8_t   endpoint;    // +0x101
    std::int8_t   alternate_endpoint; // +0x102
    std::uint8_t  frame_count; // +0x106
    bool           finished;   // +0x107
    std::int16_t   request_start; // +0x114
};
```

Keep the cursor, animation-table/resource binding, and skeleton pose cache as
separate C++ objects even if a compatibility layer later packs them into the
retail object layout. The required operations are:

* `request(id, start, end, alternate_endpoint)`: validate against the selected
  part set, substitute `frame_count-1` for `-1`, clamp endpoints, reset
  fraction, set direction, and mark equal-endpoint requests finished. On a
  mode-0/2 endpoint, swap `endpoint` with `alternate_endpoint` and negate
  direction when the latter is non-negative.
* `cycle(id, direction)`: select the ID, set mode `1`, start at frame zero,
  reset fraction, and clear finished;
* `advance(global_scale)`: add or subtract
  `(rate * global_scale) >> 8` to the 16.16 `(frame << 16)|fraction`
  accumulator, then apply the mode's endpoint rule;
* `decode_pose()`: if ID/frame changed, decode the selected frame into local
  part transforms, apply the resource's key-spacing interpolation, and compose
  parents in the cached calculation order. For the extracted skater resource,
  the key-spacing high word is zero, so this is a direct per-frame sample.
* `bind_parts()`: match animation and model part names and build the
  model/render-part→animation-part order map before exposing the pose to
  rendering; keep the separate `+0x13c` model-descriptor records as well.

The first differential tests should be deterministic unit tests rather than
visual tests:

1. `RunAnim` initialization, `CycleAnim`, `-1` endpoint substitution, clamping,
   equal-frame completion, and the invalid-ID fallback behavior.
2. 16.16 advancement at rates `0x10000` and `0x14000`, including forward,
   reverse, stop, cycle, ping-pong, and endpoint-swap cases.
3. Steering target-frame convergence using the exact 1/3/5 step thresholds,
   including direct `+0xf4` writes and equal-frame requests.
4. Push sequence `1 → 3 → 0`, idle restoration, and the absence of an
   invented top-level roll animation.
5. The 19-part name/parent binding and animation-to-model order map.
6. Pose-cache invalidation on ID/frame changes and root-first hierarchy
   composition, with compressed-packet decoding tested separately once its
   format is deliberately researched.

## Open questions / falsifiers

* A gameplay trace can still confirm whether a concrete level ever selects a distinct straight-roll ID, and can identify which derived vtable callbacks are active for a concrete player object. The base `CSuper` callbacks observed statically are no-ops, so this is not a blocker for the pose path already established.
* If a gameplay trace shows a distinct roll animation ID under straight motion, the “stable ID 0 after push” statement should be narrowed to the tested grounded path; it does not affect the selection/current/time/pose pipeline.
* The source inventory also exposes adjacent compatibility methods—`FrameReached`,
  `ApplyPose`, `SetAnimSpeed`, `StoreNewFrame`, `StoreOldFrameAnim`,
  `M3dUtils_ClearPoseStuff`, `M3dUtils_BuildPose`, and
  `M3dUtils_GetPartPosition`. Their names establish useful future API seams,
  but their exact standalone PC entry points are outside this minimal pipeline
  and should not be mistaken for missing selection or advancement stages.
