# TRG gap/objective runtime path

Status: confirmed executable gap-definition table, trigger-command lookup, and
native immediate/deferred completion boundary; score/checklist service names
remain partial
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004544a0`, `0x004c5dc0`, `0x004c7c50`, `0x0049db80`, `0x0048f5f0`, `0x0048f690`, `0x00414c50`, `0x0056dd58`, `0x00546110`

The level trigger command stream reaches the gap/objective service through
opcode `0x00c9`. Its cursor contract is independently confirmed by both the
execution dispatcher at `0x004c5dc0` and the conditional-skip helper at
`0x004c7c50`:

```text
0x00c9
  -> align (opcode position + 5) down to a four-byte boundary
  -> read u32 runtime checksum
  -> read u16 gap divider/argument
  -> compare checksum with the source command-point record +0x0c
  -> find the divider in the runtime gap-definition table
```

The command therefore joins three independently bounded values: a type-6
command-point checksum from the TRG allocation, a divider/argument encoded in
the command stream, and a definition record from the executable's table. A
checksum mismatch or missing divider does not establish a completed gap.

## Executable definition table

The general/Warehouse table begins at `0x00546110`. It contains 132 records of
`0x2c` bytes followed by a terminator record whose divider word is `0xffff`.
The recovered record layout is:

```text
+0x00  u16 flags
+0x02  u16 unknown word
+0x04  u16 divider ID
+0x06  i16 score value
+0x08  char[36] NUL-terminated display name
```

The table is grouped by flag values in the retail image. Representative
entries include flag `0x0013` with dividers `0x0000`/`0x0001` and scores
250/500 (`[WIMPY GAP]`/`[GAP]`), flag `0x0011` channel-gap entries, and flag
`0x0019` rail/grind entries. The names are presentation strings stored in the
runtime table; they are not inferred from TRG node names.

The pointer at `0x0056dd58` selects which compiled table the command path uses.
The level setup at `0x004544a0` selects `0x00546110` for the normal
Warehouse/general-level branch, `0x00541990` for the early/general level
branch, and `0x005283d0` when the custom-park flag is active. Thus
`0x00546110` is the correct table for the Warehouse evidence above, while a
recreation must keep the table base as runtime state rather than hard-code it
for every level family.

## Runtime effect boundary

After the checksum/divider match, the dispatcher updates the active gap state
and either completes the definition immediately or retains the source
command-point for a deferred player/checklist action. The observed ordinary
path marks the gap complete and awarded, then emits one pulse back through the
source node's counted link list. Definitions carrying flag bits `0x08` or
`0x40` remain on the deferred path in the recovered runtime; their eventual
player-position/checklist service is not assigned a guessed name here.

The deferred boundary is visible in the skater object and in the physics
dispatcher. The c9 path keeps twenty checksum slots at `player + 0x2f74` and
twenty matching definition-index slots at `player + 0x2fc4`. On a matching
definition, the checksum slot is cleared and the table index selects the
`0x2c`-byte executable record. The flag branches then preserve the definition
pointer and source command-point node in one of two pairs:

```text
definition flags & 0x08:
    player +0x3014 = definition pointer
    player +0x301c = source command-point node

definition flags & 0x40:
    player +0x3018 = definition pointer
    player +0x3020 = source command-point node
```

For definitions with neither deferred bit, the c9 branch calls the gap/state
helpers at `0x0048f5f0`, `0x0048f690`, and `0x00414c50`, then pulses the source
node and submits sound `0xa1` through the existing sound path. The helper
arguments include the definition's word at `+0x04` and the negated signed score
at `+0x06`; this proves the score record is consumed without assigning the
remaining helper parameters public meanings.

The skater physics dispatcher at `0x0049db80` is the downstream consumer for
the deferred records. In the ordinary ground state, a pending `+0x3018`
definition is applied through `0x0048f690` with mode `2` and a terminal value
of `0xffffffff`, marked/cleared through `0x00414c50`, and followed by a pulse
from the source node at `+0x3020`; both pending definition slots are then
cleared. The separate state-4 branch consumes `+0x3014`, clears it, and pulses
the source node at `+0x301c`. These are independently supported state-specific
consumers, not a claim that the two flags are named gameplay categories.

## Native command/state contract

The portable implementation keeps the command cursor and the gameplay state
as separate evidence boundaries. `CommandCursor::read_gap_operands()` aligns
the absolute `(opcode position + 5)` down to four bytes, reads the u32 checksum
and u16 divider, and leaves the cursor at the next command. The dispatcher
rejects a checksum that does not equal the source type-6 command-point record
before calling the gap service; the rejected command therefore creates no gap
state and sends no source pulse.

`LevelTriggerState::on_gap()` joins the divider to the selected executable
record. A definition with neither `0x08` nor `0x40` completes immediately,
records `GapSeen` followed by a source-correlated `GapCompleted` event, and
arms one pulse. Either flag sets the shared native `deferred` state without
collapsing the two retail player-consumer branches. `mark_gap_complete()` is
the later deferred handoff and retains the same one-shot pulse rule.

The native fixtures also keep the containing node as the hard cursor boundary:
a missing aligned checksum/divider pair raises `FormatError` and cannot reach
the gap service. These tests cover the malformed stream and admission cases
without assigning names to the unresolved player/checklist fields.

This gives the faithful recreation a precise service boundary:

```text
type-6 TRG command point +0x0c checksum
    + command 0xc9 aligned checksum/divider
    -> executable gap definition (+0x00/+0x04/+0x06/+0x08)
    -> immediate completion or deferred gap state
    -> ordinary completion: one source-node pulse
```

The score value, divider, display name, completion/award state, and source
pulse are independently representable. The unresolved part is the owner of
the deferred player-position test and the public names of the score/checklist
fields, not the disk/command/table lookup, pending-pointer storage, or
state-specific completion branch.

## Corpus cross-check

The extracted 32-file TRG corpus contains 1,821 `0xc9` records. The same
decoder that checks the command cursor reports no width desynchronization for
these records. The table and command are therefore distinct asset/runtime
inputs: the divider is in TRG bytecode, while the score/name definition is
compiled into the PC executable.
