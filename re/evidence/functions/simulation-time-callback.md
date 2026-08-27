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

The current recording probe is at `0x004dad68`, immediately before the second
store. It captures the callback arguments, timer state, source register, and
computed value as evidence; it does not restore that derived value during
strict replay. The `0x004f5ff0` path remains a separate millisecond-clock
helper and is not the callback boundary that produced the observed Warehouse
simulation-time values.
