# Grounded turn producer to animation selector

Build: retail `THawk2.exe`, SHA-256
`f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`.

Question: what does the grounded action/turn producer pass through
`Skater_UpdateHeading` (`0x00492f20`), and which animation state is mutated
before and after the request?

## Boundary and call order

The relevant per-frame path is:

```text
0x00493370 Skater_ActionPhysicsStep
    writes player+0x3144, mirrors it to player+0x3148
    calls 0x00492f20 Skater_UpdateHeading
        calls 0x00492ed0 for frame easing
        calls one of 0x004903f0/0x00490450/0x00490480
            calls 0x004902e0, resets +0x108 to 0x10000
            calls 0x00480730 Animation_Start
        special completion may call 0x00496280
    clears player+0x2e84
```

This ordering is from the PyGhidra decompilation of `0x00493370` and
`0x00492f20`. The selector is `__fastcall` with the skater object in the
first argument; the request wrappers are `__thiscall` methods with the object
in `ECX`. The selector does not advance the animation clock. It seats a pose,
and the separate object-update dispatcher `0x00480fa0`/`0x00480950` consumes
that request on the object-update pass; the selector and cursor advance are
not the same operation or an assumed same-frame callback.

The reviewed generated decompilation is retained at
`build/ghidra/decomp/animation-selector.c`; the combined function context is
at `build/ghidra/inspect/animation-selector.json`.

## Exact selector behavior

The decompiled selector first chooses:

```text
limit = (signed_i8(player+0x31a2) > 0x1e || profile(+0xb0) != 0)
          ? 0x5a000 : 0x2d000
turn = player+0x3148
```

For nonzero `turn`, a zero transition gate at `player+0x2dd8`, and current
animation IDs `0` or `6..10`, the normal grounded branch computes

```text
target = min((abs(turn) * 0x16) / limit, 0x16)
```

The selected ID and pre-approach frame are:

| `player+0xd8 & 2` | turn sign | ID | reset frame unless current ID is | extra step |
| ---: | ---: | ---: | ---: | --- |
| 0 | negative | 6 | 6 | none |
| 0 | nonnegative | 7 | 7 | none |
| nonzero | negative | 7 | 7 | none |
| nonzero | nonnegative | 6 | 6 | increment frame once when below target |

The resulting frame is `Skater_ApproachHeadingFrame(current, target)`, whose
step is 5 when the distance is greater than 12, 3 when greater than 3, and 1
otherwise. The selector then calls
`FUN_00490450(id, frame, frame, -1)`. Equal start/end values deliberately
create a stopped pose request.

When the transition gate is nonzero, the special branch handles current IDs
`6..10`. It uses target `15` for ID 9 and target `12` for ID 10. The sign and
`+0xd8 & 2` choose the pair as follows:

| `+0xd8 & 2` | turn sign | ID | target | source ID seated directly at target |
| ---: | ---: | ---: | ---: | ---: |
| 0 | negative | 9 | 15 | 6 |
| 0 | nonnegative | 10 | 12 | 7 |
| nonzero | negative | 10 | 12 | 7 |
| nonzero | nonnegative | 9 | 15 | 6 |

If the current ID is the listed source ID, the selector writes the target to
`+0xf4` before calling the approach helper. If the current ID is not the
selected ID, it writes frame zero. It calls `FUN_00490450(id, frame, frame,
-1)`, and when the resulting frame equals the target it calls
`FUN_00496280` after the request. The callback checks the current velocity and
may issue a separate event; this slice records the ordering but leaves that
service caller-owned.

Finally, when `player+0x3148 == 0`, the selector has a release stage:

```text
current ID 6 or 7:
    FUN_004903f0(0, 0, -1, -1)
current ID 9 or 10:
    FUN_00490480(8, 0x13, 0x1a, 0x13)
```

The first request therefore starts idle at frame 0 and uses idle's last frame
as its endpoint. The second starts animation 8 at frame `0x13`, runs to
`0x1a`, and returns toward `0x13` through the signed alternate endpoint.
No release request is made for other current IDs.

## Wrapper and cursor mutations

Each selector wrapper first calls `FUN_004902e0`, writes
`player+0x108 = 0x10000`, and calls `Animation_Start`:

| wrapper | `Animation_Start` arguments | cursor effects |
| --- | --- | --- |
| `0x004903f0` | `(anim, start, -1, -1)` | rate reset, last-frame endpoint substitution |
| `0x00490420` | `(anim, start, -1, -1)` | rate reset, last-frame endpoint substitution |
| `0x00490450` | `(anim, start, end, -1)` | rate reset, equal-frame stop when selector calls it |
| `0x00490480` | `(anim, start, end, alternate)` | rate reset, signed alternate endpoint |

`Animation_Start` writes `+0xf6`, loads `+0x106`, substitutes exactly `-1`
with `frame_count - 1`, clamps other negative values to zero and oversized
values to the last frame, then writes mode 0, direction, `+0x101`, `+0x102`,
`+0x114`, `+0xf4`, zero fraction, and the initial finished flag. The native
`GroundAnimationRequest` records the wrapper and these arguments, while
`apply_ground_animation_request` performs the wrapper rate reset before
delegating to `AnimationCursor::request`.

## Confidence and tests

Confidence is confirmed for the selector branch ordering, target arithmetic,
the alternate-positive pre-increment, source-state target seating, release
arguments, wrapper reset ordering, and the post-request completion check. The
conclusions come from the local PyGhidra decompilation and are covered by
`src/runtime/ground_animation_test.cpp`. The `FUN_004902e0` preflight writes and
the velocity/event behavior inside `FUN_00496280` remain explicit unresolved
services.
