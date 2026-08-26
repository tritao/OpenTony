# Session F — skater animation pipeline

Status: the complete minimal pipeline is confirmed by static control-flow, field-write, asset, and pose-cache evidence. Runtime launch/input was reproduced through the frontend, but a clean gameplay transition trace was not obtained; runtime claims below are marked accordingly.

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
turn IDs `9`/`10` request ID `8` over frames `0x13..0x1a`, with the exact
endpoint/transition byte supplied by the caller. The sign-to-left/right
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
| `+0x102` | request endpoint/transition behavior | Controls endpoint handling; exact blend semantics were intentionally not decoded. |
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
| `+0x150..` | animation-part → model-part order map | `SetAnimOrder(anim_part, model_part)` writes one byte per part. |

`0x00480950` combines `+0xf4` and `+0x104` as a 16.16 frame value, adds or subtracts `(+0x108 * DAT_0056865c) >> 8`, writes the fractional part back to `+0x104`, and writes the integer frame back to `+0xf4`. Its mode switch handles stop, loop, ping-pong, and reverse endpoint behavior. This is the reproducible time/frame advancement path.

## Animation resource identity

The animation resource is table-backed rather than a separately proven per-player pointer:

1. `Front_LoadGame` at `0x004524a0` requests the `sk2anim` resource with `0x004b46a0("sk2anim", 1)`.
2. `0x004b46a0` looks up or loads `sk2anim.psx`, stores the loaded resource in the global resource-slot table at `DAT_0056d440 + slot*0x44`, registers it, and marks the slot loaded.
3. `RunAnim` uses the object's model/part-set byte at `+0x1f` to select the animation table at `DAT_0056d444 + part_set*0x11`, then indexes that table with `+0xf6` to obtain the selected animation's frame count and data entry.

Thus the minimal identity is `(object+0x1f model/part set, object+0xf6 animation ID)`, with the loaded `sk2anim.psx` resource resolved through the global model-region tables. No unverified “current resource pointer” field is introduced.

## Per-update and pose boundary

`0x00480fa0` is the object update dispatcher. When the object's update flags
permit animation, it calls `0x00480950`; it then invokes two object callbacks
through the vtable. The timing routine is consequently separated from the
pose/data consumer. The base `CSuper` vtable entries observed at
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
   fraction, and interpolates each part's three short transform components.
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
part lists are therefore deliberately name-compatible; `0x00480d90` compares
those names and calls `SetAnimOrder` for each match.

`SK2ANIM.PSX` SHA-256 is
`70cf84ac83e86a99815472325fa8ea875e01ef5c50152eff144731152f9ba1e5` and
`SK2MOD.PSX` SHA-256 is
`ee74c8325fdeb1cbb2e6f13af70c5b8c694b189f824c2a184abce7056f8ab8b3` in the
extracted asset set used for this investigation.

## Runtime check

The controlled headless sessions verified the input harness separately:
writing Return/Space/Pad2 scan-code bytes at the keyboard state buffer caused
`PCInput_BuildActionMask` (`0x004e42c0`) to produce action mask `0x40`, and
the frontend advanced through `MAIN_MENU`, `CAREER_SELECT`, and
`SKATER_SELECT`; earlier attempts also reached `LEVEL_SELECT`. The bounded
automated runtime attempts did not reach a stable gameplay frame with the
animation watchpoints armed, so no runtime watchpoint trace of
`player+0xf6/+0xf4/+0x104/+0x108` is claimed. The selector and timing writes
above are exact static writes, and the state-specific callers are reproducible
in the retail image.

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
    std::int8_t   transition;  // +0x102
    std::uint8_t  frame_count; // +0x106
    bool           finished;   // +0x107
    std::int16_t   request_start; // +0x114
};
```

Keep the cursor, animation-table/resource binding, and skeleton pose cache as
separate C++ objects even if a compatibility layer later packs them into the
retail object layout. The required operations are:

* `request(id, start, end, transition)`: validate against the selected part
  set, substitute `frame_count-1` for `-1`, clamp endpoints, reset fraction,
  set direction, and mark equal-endpoint requests finished;
* `cycle(id, direction)`: select the ID, set mode `1`, start at frame zero,
  reset fraction, and clear finished;
* `advance(global_scale)`: add or subtract
  `(rate * global_scale) >> 8` to the 16.16 `(frame << 16)|fraction`
  accumulator, then apply the mode's endpoint rule;
* `decode_pose()`: if ID/frame changed, decode the selected frame into local
  part transforms, blend neighboring keys as the original does, and compose
  parents in the cached calculation order;
* `bind_parts()`: match animation and model part names and build the
  animation-part→model-part order map before exposing the pose to rendering.

The first differential tests should be deterministic unit tests rather than
visual tests:

1. `RunAnim` initialization, `CycleAnim`, `-1` endpoint substitution, clamping,
   equal-frame completion, and the invalid-ID fallback behavior.
2. 16.16 advancement at rates `0x10000` and `0x14000`, including forward,
   reverse, stop, cycle, ping-pong, and endpoint-transition cases.
3. Steering target-frame convergence using the exact 1/3/5 step thresholds,
   including direct `+0xf4` writes and equal-frame requests.
4. Push sequence `1 → 3 → 0`, idle restoration, and the absence of an
   invented top-level roll animation.
5. The 19-part name/parent binding and animation-to-model order map.
6. Pose-cache invalidation on ID/frame changes and root-first hierarchy
   composition, with compressed-packet decoding tested separately once its
   format is deliberately researched.

## Open questions / falsifiers

* A future gameplay trace should watch `player+0xf6`, `+0xf4`, `+0x104`, and `+0x108` while separately forcing idle, straight push/roll, and left/right steering. It should show ID changes at the selector wrappers and frame-only changes in `0x00480950` between requests.
* The precise meaning of request byte `+0x102` remains intentionally unresolved; static code proves it can replace the endpoint and reverse direction but not the higher-level blend policy.
* A gameplay trace can still confirm whether a concrete level ever selects a distinct straight-roll ID, and can identify which derived vtable callbacks are active for a concrete player object. The base `CSuper` callbacks observed statically are no-ops, so this is not a blocker for the pose path already established.
* If a gameplay trace shows a distinct roll animation ID under straight motion, the “stable ID 0 after push” statement should be narrowed to the tested grounded path; it does not affect the selection/current/time/pose pipeline.
