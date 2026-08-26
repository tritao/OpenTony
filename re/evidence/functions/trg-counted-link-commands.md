# Counted TRG link-command payloads

Status: confirmed
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004c5dc0`, `0x004c5c70`, `0x004c5b60`, `0x004c7c50`

## Observation

The command-stream dispatcher at `0x004c5dc0` routes `0x0004` and `0x0005`
through the activation/suspend helper at `0x004c5c70`. That helper reads a
`u16` count from the stream and then consumes exactly that many `u16` node
indices before applying the operation. The signal helper at `0x004c5b60`,
used by `0x000a`, has the same counted node-index input shape.

The conditional payload walker at `0x004c7c50` repeats these widths when it
skips a non-matching `0x0094` block. This independently fixes the cursor
contract: the counted list belongs to the command payload and is not the
serialized link list preceding a type-6 command-point checksum.

`0x0003` is the distinct no-operand command in this family. Its helper
executes the source type-6 node's serialized links. `0x000d` is also distinct:
it consumes one following `u16` visibility value.

## Native reconstruction

`CommandCursor::read_node_index_list()` performs the bounded count-and-list
read. `TriggerRuntime::dispatch()` passes that list to
`on_suspend_activate()`/`on_signal()`, while `skip_operands()` uses the same
reader during conditional scanning. The native service still applies the
retail target filter—suspend/activate and signal mutate type-1/type-7
gameplay objects while retaining an event for every listed target.

The regression fixture deliberately gives its type-6 source a serialized link
of node `2`, then supplies command lists targeting node `1` and node `2`. It
also places `0x0086` after the lists to prove that execution reaches the next
opcode. A second fixture places the same commands inside a skipped conditional
and verifies no side effect occurs; a truncated list must raise `FormatError`
instead of consuming the node terminator as an invented target.

## Open questions

The higher-level gameplay labels of the target object state remain outside
this slice. The list width, source/payload ownership, dispatch ordering, and
malformed-stream behavior are fixed by the two helper control flows and the
bounded native fixtures above.
