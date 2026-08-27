# Simulation-time callback producer

Status: observed

The Warehouse runtime trace identifies the multimedia timer callback at
`0x004dace0` as the producer path that feeds the simulation-time accumulator.
The callback is registered by the timer setup path at `0x004dac88` and runs on
Wine's `TIME_MMSysTimeThread`, separate from the gameplay thread.

At callback entry the observed stack is:

```text
[esp+0x00] callback return address
[esp+0x04] callback argument 0
[esp+0x08] callback argument 1
[esp+0x0c] timer state pointer = 0x006a05a0
```

The callback reads the interval from `timer_state + 0x04`, accumulates it in
`timer_state + 0x0c`, updates the floating accumulators at `0x006a0590` and
`0x006a0598` when the pause gates permit it, and reaches the integer stores:

```text
0x004dad58  -> DAT_0056e31c
0x004dad68  -> DAT_0056e320
```

The recording probe is at `0x004dad68`, immediately before the second store.
It captures each delivery as a `timer_callback_delivery` event, including the
callback ordinal within the observed gameplay frame, callback arguments, timer
state before/after (with the `+0x0c` pre-value reconstructed from the retail
add), and the floating/integer outputs. The event is queued across frame
boundaries as an external input; none of its derived clock values are restored
during strict replay.

The offline `advance_timer` model in `src/camera/camera_timing.hpp` mirrors the
callback's state transition and x87-style double-to-integer publication. Its
fixture covers repeated 16 ms deliveries and both pause gates. Long idle
recordings remain required to validate the model against retail output across
more than one callback cadence.

The `0x004f5ff0` path remains a separate millisecond-clock helper and is not the
callback boundary that produced the observed Warehouse simulation-time values.
