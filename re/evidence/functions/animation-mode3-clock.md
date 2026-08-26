# Animation mode-3 clock ping-pong

Build: retail `THawk2.exe`, SHA-256
`f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`.

Question: what exact integer phase and terminal behavior does
`CSuper::UpdateFrame` (`0x00480950`) use for playback mode `3`?

Evidence:

- The reviewed Ghidra decompilation at `build/ghidra/decomp/animation-00480950.c`
  computes the signed range as `s16(+0xfc) - s16(+0xfa)` after the common
  accumulator update.
- Modes `2` and `3` zero the ordinary frame step. Mode `3` therefore keeps
  the current `+0xf4/+0x104` accumulator until its clock-derived frame write.
- For a nonzero range, the tick size is the integer quotient
  `0x20000 / s32(+0x108)`, and phase is
  `(AnimationClock - s16(+0xfe)) / tick_size`. The subsequent `% period` is
  x86 signed `IDIV` remainder; it is not normalized into a positive modulo.
- A zero range takes no mode-3 frame write and returns the common accumulator.
  The executable's diagnostic string is “Pingpong on same frame in anim %d,
  frame %d”; its logging/assert condition and the zero-rate divide failure are
  not treated as playback state here.

For valid nonzero positive playback rates, the exact mode-3 state transition
is:

```text
range = s16(target_frame2) - s16(target_frame)
step = 0                         // mode 3
accumulator = (s16(frame) << 16) | u16(frame_fraction)
write(frame, fraction, accumulator)

if range > 0:
    period = range * 2
    ticks = (AnimationClock - s16(mode3_clock)) /
            (0x20000 / s32(rate))
    phase = ticks % period       // retain the signed remainder
    if range < phase:
        phase = period - phase
    frame = s16(target_frame) + phase
    return ticks / period

if range < 0:
    period = -range * 2
    ticks = (AnimationClock - s16(mode3_clock)) /
            (0x20000 / s32(rate))
    phase = ticks % period       // retain the signed remainder
    if -range < phase:
        phase = period - phase
    frame = s16(target_frame) - phase
    quotient = ticks / period
    return (u16(u32(quotient) >> 16) << 16) | u16(frame)

return accumulator                // range == 0
```

Consequences covered by deterministic tests:

- With range `2..4`, rate `0x10000`, and origin `0`, clock `-2` gives
  `ticks=-1`, `phase=-1`, and frame `1`; modulo-normalizing the phase would
  incorrectly produce frame `3`.
- Equal targets do not snap a stale current frame to the target; the ordinary
  frame/fraction accumulator remains unchanged.
- Positive ranges return the quotient as an integer, while negative ranges
  return the quotient's high 16 bits packed with the resulting frame in the
  low word. The native cursor preserves this asymmetry in its return value;
  quotient `1` therefore returns `0x00000004` for frame `4`, while quotient
  `0x10000` returns `0x00010004`.

Native coverage is in
`src/runtime/animation_cursor_test.cpp` and
`src/assets/psx_animation_runtime_test.cpp`; both the semantic cursor and the
offset-preserving playback record exercise the negative-phase and equal-range
cases. Confidence is confirmed for the branch order, integer quotient/remainder
operations, range direction, frame writes, and return packing. The diagnostic
side effect and zero-rate failure remain open pending a live retail probe.
