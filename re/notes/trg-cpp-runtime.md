# C++ TRG/script runtime contract

This is the conservative implementation boundary for reproducing the retail PC trigger system. It separates facts recovered from the file/dispatcher from gameplay services that still need to be recreated.

## File and node views

Use fixed-width little-endian reads from a bounded byte buffer. Do not cast the file buffer to native structs: the original loader is 32-bit, the command operands are variably aligned, and the retail loader mutates its node-offset table into pointers.

```cpp
struct TrgNodeView {
    uint16_t type;
    uint32_t offset;       // relative to the TRG allocation
    uint32_t size;         // next node offset, or file size, minus offset
};

struct TrgFile {
    uint32_t version;
    std::vector<uint32_t> relative_offsets;
    std::vector<TrgNodeView> nodes;
    std::shared_ptr<const std::vector<std::byte>> backing;
};
```

The loader contract is:

1. Read `_TRG`, version 2, node count, and the complete offset table.
2. Validate strict increasing offsets, node bounds, and a final type `0xff` node.
3. Keep offsets relative in the portable representation. Create indexed views over the shared backing bytes.
4. Run type-4/type-15 autoexec streams.
5. Build gameplay objects, restart views, and type-6/type-2/type-9 command points.
6. Deliver pulses and restart execution through the common dispatcher.

Restart-selection commands (`0x8c` and `0xb0`) resolve a type-8 node by its
name and retain the selected node; they do not execute it immediately. An
explicit restart execution applies the recovered position triplet through a
service callback, preserves the two post-position auxiliary fields used by
the player restart path, then dispatches the stream after the restart name.

For a type-8 node, the recovered restart payload after its links is:

```text
aligned +0x00  i32 fixed-point X
aligned +0x04  i32 fixed-point Y
aligned +0x08  i32 fixed-point Z
aligned +0x0c  u32 restart auxiliary field copied to player +0x14
aligned +0x10  u16 restart auxiliary field copied to player +0x18
aligned +0x12  NUL-terminated restart name
```

The retail PC runtime instead stores `base + relative_offset` into the table at `DAT_0056e210`. That relocation is an implementation detail, not a requirement for a faithful C++ port.

## Type-6 command point

The observed type-6 layout is:

```text
+0x00  u16 type = 6
+0x02  u16 link_count
+0x04  u16 link_node[link_count]
       conditional 2-byte alignment matching FUN_004c8130
       u32 event/link checksum
       u16 command stream: opcode/operands ... 0xffff
```

The runtime record allocated by `FUN_004c84d0` is 0x18 bytes. Its confirmed offsets are:

```cpp
struct CommandPointRuntime {
    const std::byte* stream; // +0x00, points into the loaded TRG backing
    uint8_t state0;          // +0x04
    uint8_t state1;          // +0x05
    uint8_t initialized;     // +0x06
    uint8_t pulse_count;     // +0x07
    uint16_t state;          // +0x08
    uint16_t source_node;    // +0x0a
    uint32_t checksum;       // +0x0c
    CommandPointRuntime* bucket_next; // +0x10
    CommandPointRuntime* all_next;    // +0x14
};
```

The names of the three state bytes are deliberately not retail names. The allocation is 0x18 bytes, although the separate allocation counter advances by 0x1c; preserve the observed record offsets but do not infer that counter as a C++ object size.

This is a logical layout, not a serialized C++ ABI. On a 64-bit build, native pointers change the offsets; use 32-bit compilation for an ABI-compatible emulation, or represent stream/list pointers as offsets or handles in a portable runtime object.

## Dispatcher design

Use a cursor over `std::span<const std::byte>` with explicit `read_u16`, `read_u32`, `read_cstring`, and bounds checks. The dispatch loop is conceptually:

```cpp
while (cursor.remaining_at_least(2)) {
    const uint16_t opcode = cursor.read_u16();
    if (opcode == 0xffff) break;
    dispatch_one(opcode, cursor, context);
}
```

The command table must encode operand shape, not just an opcode-to-function map. Confirmed shapes include:

```text
0x02                 NUL-separated string list
0x03                 opcode-only pulse command; it uses the source node links
0x04/0x05/0x0a       u16 count followed by that many u16 target node indices
0x06..0x0c           opcode-only commands, except 0x0a
                     (0x0b/0x0c are kill variants)
0x0d                 one u16 visibility value
0x68                 three u16 values; fog update occurs after the stream
0x69/0x6a            one u16 music/sound value
0x7e/0x7f/0x80       NUL-terminated resource string
0x85/0x8d            fixed-point six-word record tables with 0xff marker
0x8c/0x8e/0xb0/0xb2  NUL-terminated strings
0x86/0x93..0x9d      one u16, with 0x95, 0x98, and 0x9e being opcode-only
0xc8/0xca            two u16 values
0xc9                 aligned u32 checksum + u16 argument; table lookup by argument
0xcb/0xcc/0xcd       one u16 condition/flag/goal index
0xab                 aligned u32 script key + three u16 raw values
type 10/11 nodes     fixed-point position + trailing u16 runtime flag word
type 12/14 nodes     u32 link key + node-indexed runtime registration record
```

Type-1/type-7 object construction has one additional bounded input before the
position triplet: a byte list terminated by `0xff`. Retail
`FUN_004c5460` tests option values `2` and `4`; the latter clears constructed
object flag bit `0x2`, type 7 sets bit `0x4`, and absence of option `2` enters
an environment/baddy-list registration check. The native loader preserves the
raw list and these derived factory-input predicates on `TriggerObjectState`.

The retail dispatcher has additional low-frequency branches (`0x82`–`0x92`, `0x99`–`0xb1`) whose operand widths are recoverable even where their helper semantics are not. A portable implementation should expose them as named raw-helper callbacks or no-op/unsupported callbacks with diagnostics, never consume a guessed width. Unknown values should report opcode, stream offset, source node, and remaining raw bytes.

The strongest first implementation mapping is:

```text
0x03 -> send linked pulses while the command-point budget is nonzero; decrement finite budgets
0x04 -> send_suspend(read_counted_target_list())
0x05 -> send_activate(read_counted_target_list())
0x0a -> send_signal(read_counted_target_list())
0x0b/0x0c -> send_kill(source_node, variant)
0x0d -> send_visible(source_node, read_u16())
0x83/0x84 -> clear/set object flag by the following object id
0x68 -> set_fog_range(read_u16(), read_u16(), read_u16())
0x7e/0x7f/0x80 -> resource_loader(read_cstring(), mode)
0x86 -> command_point.initial_pulse_count = read_u16()
0x8c/0xb0 -> select a named restart node for later execution
0xb2                 NUL-terminated two-player restart name; selected only in two-player mode
0xab                 allocate/register a script object from the aligned key; preserve the three raw values
0x94/0x95 -> pulse-count conditional block
0x97 -> clear the global timer object; the following read_u16() is passed to
       FUN_004c5d90, which is a ret-only stub in this PC build
0x9d -> set reverb type; 0x9e -> update level/event state (first call writes
       the verified `0x50`/`0x40` initialization values)
0xa2                 aligned NUL-terminated LoadAI string; emit the retail
                     `LoadAI command not supported` diagnostic and continue
0xa6/0xa9 -> publish the two known script global words
0xc9 -> complete_gap(read_aligned_checksum(), read_u16()); ordinary matched
        records award and pulse the source links once, flagged records defer
0xcb/0xcc -> set/conditional-career-flag(read_u16())
0xcd -> conditional-goal(read_u16())
0xab -> create/register a bounded script-object record from the aligned key and raw parameters
type 10/11 pulse -> retain the retail runtime-list state byte (+0x04): pulse sets 1; kill clears 0
type 12/14 pulse -> activate the registered node record (+0x0a) after key resolution
```

The PC dispatcher field-write subset is represented by
`LevelTriggerState::current_object_fields()` and
`current_skater_fields()`: 0x99/0x9a -> object +0x4d4/+0x4d8, 0xa4/0xa5 ->
+0x4dc/+0x4de, 0xa0/0xa8/0xac -> +0x504/+0x434/+0x436, 0xad -> copy
+0x3a4 to +0x3dc, 0xa3/0xb1 -> skater +0x3198/+0x319c, and 0xa7 ->
+0x40c/+0x410/+0x414. `dispatcher_field_writes()` retains source and raw
operands so the unresolved current-object identity can be joined later.

`0x0d` is source-correlated by Warehouse node 141: its stream is `{0x000d, 0x0001, 0xffff}` and it reaches `FUN_004c77f0`. `0xc9` is corpus-confirmed as a gap path, but its scoring/checklist services should be injected through a `GameState` interface rather than embedded in the bytecode reader.

`0xab` is source-correlated as an object-creation branch rather than a
generic legacy command. Retail aligns the operand cursor down at
`(opcode + 5)`, allocates `0xcc` bytes, and calls `FUN_00401060(key, raw)`,
which registers the object, resolves the key into `+0x1a`, and assigns its
object-list identifier at `+0xc8`. The PC constructor does not read the three
u16 values behind the key; native `TriggerServices::on_script_object` keeps
them available without assigning names to them.

## Services still required for faithful gameplay

The dispatcher is only the script-facing boundary. A faithful recreation still needs compatible services for object creation and lookup, link traversal, pulse scheduling/countdown, restart position application, gap/checklist scoring, goal/career state, resource loading and timer ownership, audio/music, fog/path state, and the target-object flags changed by `SendVisible`. Keep those behind interfaces so command decoding can be regression-tested independently.

## Regression fixtures

Keep two byte fixtures permanently:

- Warehouse node 141: type-6 links, checksum 0, stream `0x0d, 1, 0xffff`.
- Warehouse node 167: type-8 restart named `Ho_SkWare_HPGap`, stream beginning `0x68, 10, 20000, 512, 0x9d, 3, 0xa6, 512, 0xa9, 1000, 0x80, "SkWare", 0x03`.

These fixtures test both direct source-to-dispatch mapping and restart/string/alignment handling without depending on a renderer or physics implementation.
