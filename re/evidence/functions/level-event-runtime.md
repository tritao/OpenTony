# TRG level-event runtime initialization

Status: confirmed first-call state writes for trigger opcode `0x009e`; mode/versus consumers remain open
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Branch: `re/asset-runtime`
Addresses: `0x004c5dc0`, `0x00466c10`, `0x00469a30`, `0x00469de0`, `0x00568658`, `0x00568818`, `0x00568610`, `0x006a3d49`

## Proven path

The no-payload TRG command `0x009e` is a level-event entry point. The shared
command interpreter dispatches it to `0x00466c10`; the command itself does not
consume any bytes from the stream:

```text
SkWare_T.TRG command stream
    -> command 0x009e
    -> 0x004c5dc0 / TRG_InterpretCommandStream
    -> 0x00466c10
    -> first-call event/global initialization
    -> mode/versus-dependent event work
```

This is a trigger-to-runtime-state path, not a geometry loader. It is recorded
here because a faithful level runtime must preserve the event initialization
boundary reached from the loaded level's TRG data.

## Exact first-call writes

The first-call path in `0x00466c10` performs these directly observed writes:

| Destination | Observed operation | Supported interpretation |
| --- | --- | --- |
| `DAT_00568658` | write `1` when the latch is zero | one-shot active guard for this event path |
| `DAT_00568818` | write `0x50` | active event countdown consumed by the per-frame finalizer |
| `DAT_00568610` | write `0x40` | event step value consumed by the level/player update path; its public name remains open |
| secondary player `+0x3144` | clear the 32-bit field | clears the second skater's shared turn/movement accumulator; the field correspondence is independently recorded in [player-runtime.md](player-runtime.md) |
| `DAT_006a3d49` | set the byte flag | event-initialized global latch/flag; public mode meaning remains open |

The two player-specific pointers are not reconstructed from this function
alone. The safe correspondence is the already-proven player table at
`DAT_0056a858`, whose second entry is at `+4`; `0x00466c10` reaches the
secondary player object before clearing its `+0x3144` field.

## Per-frame event consumer

The event state is consumed by `0x00469a30`, the per-level skater/animation
update boundary. When the eligible player objects are in the normal/ground or
state-7 paths, it requires `DAT_00568658 == 1`, decrements
`DAT_00568818`, and waits for the countdown to reach zero. The same routine
starts the state-selected idle/step-off animation for eligible state-7
objects, so the event latch gates both the countdown and this animation-side
work.

At countdown expiry, the consumer has two directly observed outcomes:

```text
no pending landed score (or the dedicated mode-7 branch)
    -> clear DAT_00568658
    -> call the shared event-completion/reset helper 0x0044e0f0

pending landed score exists
    -> clear DAT_00568610
    -> set DAT_00568818 = 1
    -> add player +0x2a8 to player +0x16c
    -> clear player +0x2a8
```

The second-player block performs the matching `+0x2a8 -> +0x16c` transfer
when a secondary skater exists and its mode/flags allow it. Thus the exact
runtime chain extends past initialization into a countdown-gated score
commit; the owner of the surrounding presentation/stat mode remains open.

The enclosing gameplay update at `0x00469de0` is a second consumer of the
same state. While the event latch is active and the countdown is below
`0x28`, and the primary/secondary skaters are in the eligible normal, ground,
or state-7 paths, it adds `DAT_00568610` to camera `+0x5b4` through each
skater's `+0x29b0` camera pointer. This proves that the event step word also
drives a bounded camera update; it does not identify the presentation effect
represented by that accumulator.

## Boundary and open work

The command cursor contract is independently supported by the execution and
skip interpreters: `0x009e` has no inline payload, so a conditional skip lands
on the following command at the same position as normal execution.

The function also contains mode/versus-dependent branches. Their player-state
inputs and surrounding gameplay/stat service have not been assigned from this
entry point, so this record does not claim that the byte flag is a score,
objective, or player identifier. The useful proven seam for a recreation is the
one-shot initialization, countdown, secondary-player field clear, and observed
score transfer; the remaining presentation/mode policy needs a separate trace.
