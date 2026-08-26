# Ground movement / orientation

Status: supported static and runtime dataflow. A clean Warehouse run captured idle, Left, and Right at the same `0x0049f0e5` callsite while all directional records remained grounded (`physics_state == 0`). The turn update, 12-bit Y rotation, basis mapping, and fixed-point projection are identified; collision/surface handling still prevents claiming one universal closed-form X/Z equation.

Branch: `re/ground-movement`

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

## Result

The normal grounded path is:

```text
Left/Right action record
    -> player+0x2ccc controller pointer
    -> controller+0x80 (Left) / +0x90 (Right)
    -> 0x00493370
    -> signed turn-angle accumulator player+0x3144
       (Left decreases, Right increases; player+0x3148 mirrors it)
    -> 0x00496360 in grounded state 0x00496550
    -> 0x0049b500(angle & 0xfff, 1, 0)
    -> 0x004e80e0 / 0x004e7de0 fixed-point Y rotation and 0x004e3130 matrix multiply
    -> player+0x2e58..+0x2e68 orientation matrix
    -> 0x0049c7d0 integer basis player+0x30f4..+0x3114
    -> grounded correction/projection using that basis
    -> live X/Y/Z candidate
    -> 0x0049f0e5
    -> 0x00496060
```

The supported semantic is “Left/Right changes a signed turn angle which rotates the movement basis.” `player+0x3144` is not promoted to an absolute heading. The code also does not support calling `player+0x4c/+0x50/+0x54` a direct Left/Right vector: those fields are velocity/motion state and are modified by several other paths.

For a C++ recreation, the supported grounded abstraction is therefore:

```cpp
// +0x3144 is an accumulator, not a proven absolute world heading.
// Positions/velocities are Q16; short matrix and basis entries are Q12.
int32_t base_turn = player.tuning_29b7 == 0 ? 0x3c
                  : player.tuning_29b7 == 1 ? 0x78 : 0xb4;
int32_t turn_step = (base_turn * 0x100 * frame_scale_q8) >> 8;
int32_t turn_cap = player.fast_turn ? 0x5a000 : 0x2d000;

if (grounded) {
    // The original tests Left first; a simultaneous press takes this branch.
    if (left_held) {
        turn_accum = std::max(turn_accum - turn_step, -turn_cap);
    } else if (right_held) {
        turn_accum = std::min(turn_accum + turn_step, turn_cap);
    } else {
        int32_t decay = player.fast_turn ? (turn_accum >> 1)
                                         : (turn_accum >> 2);
        turn_accum -= decay;
        if (((turn_accum + 0x800) & 0xfffff000) == 0)
            turn_accum = 0;
    }
    turn_mirror = turn_accum;
}

int32_t angle12 = (frame_scale_q8 * (turn_accum >> 12)) >> 8;
angle12 &= 0xfff;
rotate_short_matrix_about_y(matrix, angle12); // Q12 sin/cos, M <- M * R_y
basis = {
    matrix.column(2), // player +0x30f4..+0x30fc
    matrix.column(0), // player +0x3100..+0x3108
    matrix.column(1), // player +0x310c..+0x3114
};
correction = grounded_projection_and_collision(velocity_4c, basis, surface);
position = commit_candidate(position, history, correction);
```

The turn constants and branches above are from `0x00493370`; `frame_scale_q8` is runtime `DAT_0056865c`. The remaining surface/collision decisions are branch-shaped rather than one universal equation, so the recreation should preserve them as explicit data-driven stages.

## Action handoff

The input mapping is already established in [input.md](../input.md):

- Left: global mask `0x8000`, DirectInput scan `203`, action record `0x0056b078`.
- Right: global mask `0x2000`, DirectInput scan `205`, action record `0x0056b088`.

The skater constructor at `0x0046c720` stores the controller object at `player+0x2ccc`. For the first player its controller base is `0x0056aff8`, so its `+0x80` and `+0x90` bytes are the Left and Right action records respectively:

```text
player+0x2ccc -> 0x0056aff8
0x0056aff8 + 0x80 = 0x0056b078  Left
0x0056aff8 + 0x90 = 0x0056b088  Right
```

## Grounded turn state

`0x00493370` is called by the per-frame skater physics wrapper `0x0049e680`, before the physics dispatcher. In the grounded state (`player+0x30b8 == 0`, also shared with state 7), it reads the controller bytes above and applies a frame-scaled signed increment to `player+0x3144`.

The relevant operation is:

```text
i3 = base_turn * 0x100 * DAT_0056865c >> 8

Left:  player+0x3144 = clamp(player+0x3144 - i3)
Right: player+0x3144 = clamp(player+0x3144 + i3)

player+0x3148 = player+0x3144
```

For the normal grounded branch, `base_turn` is selected exactly as:

```text
+0x29b7 == 0: 0x3c
+0x29b7 == 1: 0x78
otherwise:    0xb4
```

Then `turn_step = (base_turn * 0x100 * DAT_0056865c) >> 8`. The normal cap is `0x2d000`; the fast branch sets `+0x2e78 = 1`, doubles the step, and uses cap `0x5a000`. Left subtracts the step and clamps at the negative cap; Right adds the step and clamps at the positive cap. With no direction, the low-lean grounded path decays the accumulator by `turn >> 2` (or `turn >> 1` in the fast branch) and snaps values within the low 12-bit bucket to zero. `+0x2e7c` is set when an active turn branch is taken. These are turn-state operations, not world-space displacement.

`0x00492f20` consumes the mirrored `+0x3148` value for steering/pose response, but the orientation-to-basis operation used by the grounded physics path is the separate `0x00496360` call below.

## Exact orientation writer

The grounded function `0x00496550` calls `0x00496360` after its initial motion/collision preparation. `0x00496360` reads the signed accumulator and, when the grounded `+0x2d90` correction path is inactive, calls:

```text
turn_units = (player+0x3144) >> 12
angle12    = (DAT_0056865c * turn_units) >> 8
0x0049b500(angle12, 1, 0)
```

The `0x0049b500` writer is the strongest orientation evidence:

1. It masks the angle to 12 bits (`angle & 0xfff`).
2. `0x004e80e0` receives the angle as the Y component of a zero X/Z angle vector. Its X and Z inputs are zero on this path, so the nontrivial helper is `0x004e7de0`.
3. `0x004e7de0` computes Q12 `sin`/`cos` with x87 `fsin`/`fcos` after converting `angle12` to radians as `angle12 * (2*pi / 4096)`. It builds the signed-Q12 matrix

   ```text
   R_y = [  cos   0   sin ]
         [   0  4096   0 ]
         [ -sin   0   cos ]
   ```

   and the three `0x004e3130` calls compute `M <- M * R_y` using `(a*b) >> 12` products.
4. The rotated short matrix is stored at `player+0x2e58..+0x2e68`. `0x0049c7d0` exposes its columns as integer basis fields:

   ```text
   basis_0 = matrix column 2 -> player+0x30f4/+0x30f8/+0x30fc
   basis_1 = matrix column 0 -> player+0x3100/+0x3104/+0x3108
   basis_2 = matrix column 1 -> player+0x310c/+0x3110/+0x3114
   ```

The normal grounded call passes `param_3 == 0`, so this call is primarily the orientation/basis rotation. The optional velocity rotation in `0x0049b500` is behind the `param_3 != 0` branch and is not part of this normal call.

## Basis to grounded movement

The basis is consumed in the same per-frame grounded pipeline rather than being an isolated animation value.

`0x0049b010`, called before the dispatcher by `0x0049e680`, uses `player+0x30f4/+0x30f8/+0x30fc` to write the temporary correction vector at `player+0x58/+0x5c/+0x60` in grounded branches. `0x00496550` also uses the basis after its collision work. The exact helper arithmetic is:

```text
dot_q12(a, b) = trunc_toward_zero((a.x*b.x + a.y*b.y + a.z*b.z) / 4096)
mul_q12(a, b) = trunc_toward_zero(a*b / 4096)

lateral = dot_q12(player+0x4c, basis_1)
player+0x4c -= mul_q12(lateral, basis_1)

forward = dot_q12(player+0x4c, basis_0)
projection = mul_q12(forward, basis_0)
if (+0x2d94 == 0 && surface_gate != 0)
    correction -= frame_scale_q8 * mul_q12(8, projection) >> 8
```

The helpers at `0x004f5f90` and `0x004f5fc0` perform the divisions by `4096`; their common `fistp` path sets x87 round-toward-zero. This is the smallest supported movement formula: the current motion vector is projected/scaled against the rotated basis, producing a world-space correction vector. `0x0049bad0` and the surrounding `0x00496550` collision code can additionally project velocity, rotate/normalize the surface basis, and construct collision-adjusted candidates. That is why the final X/Z delta is not a pure `sin/cos` expression of Left/Right.

`0x00496550` contains direct calls to `0x00496060` for intermediate collision candidates. After the dispatcher returns, the outer wrapper reaches the observed final commit at `0x0049f0e5`.

The final wrapper sequence is explicit in `0x0049e680`:

```text
current live X/Y/Z = player+0x08/+0x0c/+0x10
player+0x08/+0x0c/+0x10 = history +0xbc/+0xc0/+0xc4
call 0x00496060(current live X, current live Y, current live Z)
```

At `0x00496060`, the shared commit receives X/Y/Z in that order and writes the three live position words (`+0x08`, `+0x0c`, `+0x10`). With `player+0x3200 == 0`, it first collision-tests the candidate and tries component fallbacks if necessary; with `+0x3200 != 0`, it writes directly. Thus `0x0049f0e5 -> 0x00496060` is a collision-safe commit, not just a blind assignment.

## Dynamic Warehouse corroboration

The clean corrected run is `build/debug/sessions/ground-orient11/orient.trace.ndjson`. It was started in Warehouse through the Free Skate path with the level-launch override, then recorded an idle prefix followed by short Left and Right holds. The directional records were filtered to the same callsite and state:

| phase | records | action mask | callsite | physics state | first X/Y/Z argument | last X/Y/Z argument | argument delta |
|---|---:|---:|---|---:|---|---|---|
| idle prefix | 350 | `0` | `0x0049f0e5` | `0` | `(365.1250, -90.0625, 170.6875)` | — | — |
| Left | 8 | `0x8000` | `0x0049f0e5` | `0` | `(365.2778, -90.0625, 274.7060)` | `(362.4892, -89.6207, 294.9813)` | `(-2.7886, +0.4418, +20.2753)` |
| Right | 9 | `0x2000` | `0x0049f0e5` | `0` | `(352.8268, -87.6475, 313.3138)` | `(342.5837, -79.0211, 333.8102)` | `(-10.2431, +8.6264, +20.4964)` |

Both controlled phases stay on the grounded state-0 dispatcher path and alter the horizontal X/Z commit arguments. Their starting positions are intentionally sequential rather than reset-identical, so the total deltas are not a paired magnitude comparison. The trace does establish the action-to-orientation sign and the orientation-to-commit correlation in one reproducible callsite/state window.

The orientation fields captured alongside those commits are the strongest runtime candidates:

| field | idle | Left window | Right window | supported interpretation |
|---|---:|---:|---:|---|
| `player+0x3144` | `0` | `-30720` … `-184320` | `-2085` … `+184320` | signed turn accumulator; Left decreases, Right increases |
| `player+0x3148` | `0` | equals `+0x3144` | equals `+0x3144` | mirror used by steering/pose code |
| `player+0x2e58..+0x2e68` | stable initial basis | changes as Left angle accumulates | changes as Right angle accumulates | 3x3 short matrix updated by the angle writer |
| `player+0x30f4..+0x3114` | fixed basis | changes with the matrix | changes with the matrix | integer/fixed-point basis consumed by grounded correction |

The matrix entries and basis words are Q12. Representative commit-adjacent samples from the same run:

```text
Left  turn=-30720  basis_0=(+44,    0, -4096)  ~= (+0.0107,  0.0000, -1.0000)
Left  turn=-184320 basis_0=(+978,   0, -4096)  ~= (+0.2388,  0.0000, -1.0000)
Right turn=-2085   basis_0=(+2151,-344, -3470) ~= (+0.5251, -0.0840, -0.8472)
Right turn=+184320 basis_0=(+1038,-676, -2677) ~= (+0.2534, -0.1650, -0.6536)
```

The short matrix and fixed basis are not merely pose snapshots: the static writers and consumers above show that they feed the correction vector and the candidate passed to the shared position commit. The position samples also show that X/Z changes continue while the turn sign changes, but the world-space result is affected by inherited velocity and collision/surface projection.

As a sign/order check, the first Left sample records `turn_accum = -0x7800`. With the ordinary `0x78` turn branch at `DAT_0056865c = 0x100`, `angle12 = -8`; applying the Q12 `R_y(-8)` matrix to the preceding short matrix predicts the observed first-turn entries (`row_0.z = +44`, `row_2.x = -45`). This ties the measured accumulator to the measured basis rotation, rather than only correlating both with the input mask.

The trace footer says `complete: false` because the debugger was stopped after collection, but all 17 controlled position records are present and all are state 0 at the target callsite. The later seven state-2 records were excluded.

## What is and is not established

Established:

- `player+0x2ccc` is the controller pointer used by the skater physics code.
- Controller `+0x80/+0x90` are the grounded Left/Right reads.
- `0x00493370` applies opposite signed updates to `player+0x3144` and mirrors the result to `+0x3148`.
- `0x00496360` consumes `+0x3144` in the normal grounded function.
- `0x0049b500` performs the fixed-point 12-bit angle/Y-trigonometric matrix rotation.
- `player+0x2e58..+0x2e68` and `+0x30f4..+0x3114` are the resulting orientation/basis storage used by grounded correction code.
- The short matrix is Q12, and `+0x30f4..+0x3114` are sign-extended Q12 basis columns, not 16.16 position vectors.
- Left and Right both reach `0x0049f0e5 -> 0x00496060` with `physics_state == 0` and produce different X/Z arguments.
- The Warehouse runtime probe records Left and Right at that same callsite with opposite `+0x3144` trajectories and corresponding matrix/basis changes.

Not promoted:

- `+0x3144` is a signed turn-angle accumulator, not proven to be an absolute world heading.
- `+0x3148` is a mirrored steering/pose value, not an independent orientation vector.
- `+0x4c/+0x50/+0x54` are not direct Left/Right displacement fields.
- No single closed-form X/Z equation is claimed: inherited velocity, surface basis, and collision correction all contribute to the final candidate. The faithful recreation target is the fixed-point action/turn/matrix/projection pipeline, not a direct `X += left/right` shortcut.
