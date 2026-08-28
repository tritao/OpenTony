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

The callback-final-store probe at `0x004dad68` remains available for direct
callback characterization. Normal recording instead samples the atomic
`timer_state + 0x0c` counter at deterministic gameplay boundaries. Because the
callback increments that counter before storing its floating accumulators, a
boundary can observe an in-flight delivery. The sampler therefore compares the
simulation accumulator at adjacent boundaries, defers such a counter tick as
pending, and records only completed deliveries with logical boundary counters.
The sampled raw counter and pending count are retained as consistency metadata.
New recordings also retain the callback-owned initial timer state in the
header.

Strict replay now owns a `TimerReplayService`. It applies exactly the recorded
delivery events through the same transition model at their recorded causal
phases (`physics_entry`, `timer_update`, the simulation-clock read, and the
timing-producer reads), publishes only after actual deliveries, and leaves
`DAT_0056e320` to the retail player load/store chain. The asynchronous callback
is suppressed at entry with a process-local ABI-preserving return patch, so it
cannot add an unrecorded delivery. Producer-phase samples separately assert the
modeled clock stream and timing-ring inputs without making volatile process
clock fields part of the player snapshot comparison.

The offline `advance_timer` model in `src/camera/camera_timing.hpp` mirrors the
callback's state transition and x87-style double-to-integer publication. Its
fixture covers repeated 16 ms deliveries and both pause gates. A fresh 257-frame
idle Warehouse recording now strict-replays with every canonical player frame
matching; repeated long recordings and action-bearing fixtures remain required
before this slice is considered complete.

The `0x004f5ff0` path remains a separate millisecond-clock helper and is not the
callback boundary that produced the observed Warehouse simulation-time values.
