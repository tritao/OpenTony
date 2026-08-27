# TRG level-event runtime initialization

Status: confirmed opcode boundary, mode-8/mode-9 first-call writes, eligible-frame countdown/score/camera side effects, and native player/replay/camera ownership; broader menu/stat ownership remains open
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Branch: `re/scripting`
Addresses: `0x004c5dc0`, `0x00466c10`, `0x00469a30`, `0x00469de0`, `0x0044e0f0`, `0x00568658`, `0x00568818`, `0x00568610`, `0x006a3d48`, `0x006a3d49`, `0x0056b798`, `0x0056b79c`, `0x0056b7a0`, `0x0056b7a4`, `0x0056b7b0`, `0x0056b7b4`, `0x0056b7dc`, `0x0056b7e0`

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

The current Ghidra project gives the exact call/return boundary: case `0x9e`
calls `LevelEvent_InitializeAndDispatch` with no operand and resumes at the
word after the opcode. The initializer first calls `0x004dee50` and
`0x004ab9c0`, whose broader services remain outside this slice, then executes
the mode-specific counter branch and the common latch initialization.

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

## Mode-dependent first-call counters

The mode branch is now represented with explicit caller inputs and raw
address-named outputs. It runs only while byte `DAT_006a3d49` is zero; the
initializer sets that byte to one before returning, so later `0x009e` calls do
not repeat the branch.

| Condition | Direct writes |
| --- | --- |
| `DAT_00533f38 == 8`, primary `+0x2cdc` differs from secondary `+0x2cdc`, and primary is lower | increment `DAT_0056b7e0` and `DAT_0056b79c`; update the signed pair `DAT_0056b7b4`/`DAT_0056b7b0` with the observed negative-step rule |
| Same mode-8 condition, primary is not lower | increment `DAT_0056b7dc` and `DAT_0056b798`; update the signed pair with the observed positive-step rule |
| Any mode-8 differing comparison | raise `DAT_0056b7a4` from primary `+0x2cdc` and `DAT_0056b7a0` from secondary `+0x2cdc` when lower |
| `DAT_00533f38 == 9`, `DAT_0056db64 == 2`, and `DAT_006a3d48 == 0` | increment `DAT_0056b7dc` and `DAT_0056b798`, then apply the positive-step rule |
| Same mode/versus condition and `DAT_006a3d48 != 0` | increment `DAT_0056b7e0` and `DAT_0056b79c`, then apply the negative-step rule |

The native `TriggerLevelEventRawStats` keeps these writes separate from any
guessed score/team names. Tests cover both mode-8 comparison directions,
mode-9 side branches, and suppression on the second dispatch.

## Per-frame event consumer

The event state is consumed by `0x00469a30`, the per-level skater/animation
update boundary. The outer gate accepts a primary skater in `(mode 0 and
field +0x2dd4 == 0)` or mode 7, and applies the same predicate to a present
secondary skater. While `DAT_00568658 == 1`, state-7 skaters request animation
`0x5d` when `+0xf6 == 0`, or animation `0x5f` when `+0xf6` is `0x5d`/`0x5f`
and byte `+0x107` is nonzero. The routine then decrements `DAT_00568818` once
per eligible gameplay frame.

For mode 7, input activity while the countdown is below `0x3c` re-arms it to
one before the decrement. At countdown expiry the routine requests replay
input reset for both player slots. If mode 7 is active, or no pending landed
score is present, it clears `DAT_00568658` and calls `0x0044e0f0`. Otherwise it
clears `DAT_00568610`, sets `DAT_00568818` to one, and conditionally transfers
each pending `+0x2a8` score into its `+0x16c` total before clearing the pending
word. The native frame result reports those two score transfers and reset/menu
requests to the owning player service.

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
same state. For non-mode-7 gameplay, while the event latch is active and the
countdown is below `0x28`, and both present skaters pass the same eligibility
predicate, it adds `DAT_00568610` to camera `+0x5b4` through each skater's
`+0x29b0` camera pointer. The native frame result exposes one `0x40` delta per
eligible camera while the countdown is in this range; it leaves application of
the actual camera field to the camera owner.

## Native contract and fixtures

`LevelTriggerState::on_level_event_state()` implements the opcode's common and
mode-specific writes. `advance_level_event_frame()` implements the observed
per-frame gate and returns animation IDs, replay-reset count, completion
request, score handoffs, and camera deltas. The frame input is opt-in because
the trigger service does not own the retail player/camera pointers; when set,
`LevelTriggerState::advance_time()` invokes it at the `LevelRuntime::tick`
boundary.

`src/trg/trg_runtime_test.cpp` uses `{0x009e, 0x0086, value, 0xffff}` to prove
that the no-payload command reaches the following `0x0086`, repeats that proof
through a conditional skip, and rejects a one-byte command tail. The state
fixture covers both mode counter directions, state-7 animation requests,
countdown/camera boundaries, pending-score re-arm and commit, mode-7 early
completion, and final latch clearing.

## Boundary and open work

The command cursor contract is independently supported by the execution and
skip interpreters: `0x009e` has no inline payload, so a conditional skip lands
on the following command at the same position as normal execution. A truncated
one-byte tail still fails because the cursor requires a complete opcode word;
no adjacent bytes are guessed as event payload.

The remaining uncertainty is limited to ownership of the broader services:
`0x004dee50`, `0x004ab9c0`, and `0x0044e0f0` perform stat/menu work outside this
slice. The player, replay-slot, and camera side effects are now connected to
their concrete native owners without renaming unresolved globals.

## Promoted native ownership

The static call chain and the native fixture now agree on the following
boundaries:

| Retail result/write | Native owner | Native side effect |
| --- | --- | --- |
| `0x00469a30` animation ID `0x5d`/`0x5f` | `LevelEventGameplayOwner -> PlayerState` | records an animation-service request while preserving the raw `+0xf6` selector input |
| replay reset for slots 0 and 1 | `LevelEventGameplayOwner -> PlayerReplayResetOwner` | records one reset per slot, including the null-secondary one-player slot |
| pending `+0x2a8 -> +0x16c` transfer | `LevelEventGameplayOwner -> PlayerState` | adds the returned value to raw `+0x16c` and clears raw `+0x2a8` |
| `0x00469de0` camera delta to `+0x5b4` | `LevelEventGameplayOwner -> CameraRuntime` | applies the 16-bit camera field update through `CameraRuntime::apply_level_event_delta` |

The deterministic `src/runtime/level_event_owner_test.cpp` fixture drives the
native boundary reached by decoded `0x009e`, advances the recovered `0x50`
countdown, and checks all four side-effect owners at expiry/window boundaries.
Together with the existing `src/trg/trg_runtime_test.cpp` cursor fixture it
keeps command dispatch and gameplay ownership separate. The owner fixture also
checks the two raw deferred-gap slots and script-object reset lifecycle, so no
parser expansion is needed to establish this event-to-gameplay chain.
