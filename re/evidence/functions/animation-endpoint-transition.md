# Animation endpoint transition

Build: retail `THawk2.exe`, SHA-256 `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`.

Question: when the current frame is already at the selected endpoint, how does
`CSuper::UpdateFrame` (`0x00480950`) interpret the alternate endpoint byte?

Evidence:

- `CSuper::RunAnim` (`0x00480730`) stores the requested endpoint at `+0x101`,
  the alternate/next endpoint at `+0x102`, the direction at `+0x100`, and the
  16.16 frame accumulator at `+0xf4/+0x104`.
- `CSuper::UpdateFrame` checks modes `0` and `2` before advancing. At an
  inclusive endpoint, a signed `+0x102` value `>= 1` swaps `+0x101` and
  `+0x102` and negates `+0x100`. Values `< 1` set `+0x107` finished.
- The same update then applies the signed fixed-point rate step. Mode `0`
  clamps an overshoot to the active inclusive endpoint.

Exact transition model:

```text
at_endpoint = (direction == +1 && frame >= endpoint)
           || (direction == -1 && frame <= endpoint)

if mode in {0, 2} and at_endpoint:
    if signed_i8(alternate_endpoint) < 1:
        finished = 1
    else:
        (endpoint, alternate_endpoint) =
            (alternate_endpoint, endpoint)
        direction = -direction

product_u32 = uint32(rate) * uint32(time_scale_q8)
step = arithmetic_s32(product_u32) >> 8
accumulator += direction * step
mode_0_clamp_to_endpoint_if_crossed()
```

This makes `0xff` a terminal `-1`, not a positive endpoint. A positive
alternate produces an inclusive ping-pong transition and preserves the
reached endpoint for the next sample. Request setup treats exactly `-1` as
the last-frame substitution; other negative start/end inputs clamp to zero.

Native coverage is in
`src/assets/psx_animation_runtime_test.cpp` and
`src/runtime/animation_cursor_test.cpp`. The test is deterministic and uses
explicit Q8 time scales; it does not claim a new live retail trace. Confidence
is confirmed for the branch ordering, signed-byte sentinel, and inclusive
clamp from the existing decompilation evidence.
