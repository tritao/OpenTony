# TRG restart runtime path

Status: confirmed source-correlated `KILLBRUCE` restart application; broader restart-selection mode rules remain partial
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Branch: `re/asset-runtime`
Addresses: `0x0046a8d0`, `0x004c5dc0`, `0x004c4e30`, `0x004c4d10`, `0x004c4c50`

## Proven path

The `SKB1_T.TRG` corpus provides a concrete runtime witness for command
`0x0098`, whose embedded name in the PC command table is `KILLBRUCE`:

```text
SKB1_T.TRG command-point node 11
    -> command 0x0098 (no inline payload)
    -> link list contains restart node 2
    -> restart node 2 position + 0x14/0x18 auxiliary fields
    -> player/restart application boundary
    -> following command-point stream work
```

This is a source-correlated command-point relationship, rather than a generic
assumption that every kill command restarts the player. The linked target is a
type-8 restart node in this witness.

## Restart fields consumed

The type-8 node layout already recovered by the TRG parser supplies the values
used by the application path:

| Restart-node field | Runtime use |
| --- | --- |
| fixed-point position words | copied to the active player/restart position |
| word at `+0x14` relative to the aligned restart payload | preserved as the restart auxiliary value |
| word at `+0x18` relative to the aligned restart payload | preserved as the restart auxiliary word |
| post-name command stream | dispatched after the restart application when the restart is explicitly executed; `0x0098` itself has no payload to consume |

The direct restart lookup helper is `0x004c4c50`; the restart execution/reset
path is `0x004c4e30`. The exact public meanings of the two auxiliary fields are
not assigned here. They are runtime inputs, not assumed heading/score fields.

The restart application has a second independently visible player-side effect
beyond position. `0x004c4e30` copies the selected node's position into the
player's live position at `+0x08/+0x0c/+0x10` and history at
`+0xbc/+0xc0/+0xc4`, copies the adjacent u32/u16 restart fields into
`+0x14/+0x18`, then calls `0x004c4d10`. That helper reads the high word of
`+0x14` (the u16 at `+0x16`), derives `((word - 0x800) & 0xfff)`, rotates the
negative-X and negative-Z Q12 seed vectors, installs the negative-Y basis
column, and refreshes the nine-short orientation plus the player-relative
basis beginning at `+0x30f4`. This is a confirmed restart-facing handoff;
the auxiliary fields' later gameplay semantics remain open.

## Cursor and limits

`0x0098` consumes only its two-byte opcode. The link list comes from the
source command-point node, so a conditional skip and normal execution leave
the cursor at the same following command. The source-correlated `SKB1_T.TRG`
case has one linked restart node; behavior for missing, multiple, or non-restart
links remains a separate error/edge-case question.

The restart-selection commands `0x008c`/`0x00b0` use a NUL-terminated name and
the same lookup family, while `0x00b2` is mode-gated for the two-player
restart name. Those selection commands should remain separate from
`KILLBRUCE`'s linked-node application path.
