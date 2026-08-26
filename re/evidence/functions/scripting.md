# Warehouse TRG/script runtime

Status: live load/dispatch path confirmed; a bounded C++ runtime now replays the retail TRG corpus, with gameplay services still being expanded
Build: f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c
Asset: SKWARE_T.TRG, SHA-256 335a13d5041c473f0a2578ec97f16db78d6ea506646534e6cecfc6212b0a581d
Addresses: 0x004524a0, 0x0046a8d0, 0x004c5130, 0x004c8050, 0x004c8130, 0x004c84d0, 0x004c58b0, 0x004c5dc0, 0x004c77f0

## Observation

The Warehouse launch reaches the trigger loader through the already-established level path:

Front_LaunchGameLevel 0x004544a0
  -> FUN_0046a8d0(level index)
  -> Front_LoadGame 0x004524a0
  -> FUN_004c5130(PTR_s_prsn_t_0052dd14)
  -> FUN_004c8050()       // autoexec scripts
  -> FUN_004c8130()       // level node/object setup

For level index 12, Front_LoadGame selects the Warehouse SkWare_T level record before the call at 0x0045284d. FUN_004c5130 is therefore the TRG loader, not a generic gameplay update routine. It appends the executable's .trg suffix at 0x00547958, allocates/reads the file through FUN_0046f490, and stores the resulting file base in DAT_0056e20c. It checks:

- *(u32 *)(base + 0x00) == 0x4752545f (_TRG);
- version 2 in the low half of *(u32 *)(base + 0x04); and
- the node count from the low half of *(u32 *)(base + 0x08).

The loader then sets DAT_0056e210 = base + 0x0c and DAT_0056e214 = node_count. Each u32 in the node-offset table is changed in place from a file-relative offset to base + offset, so DAT_0056e210[i] is the runtime pointer to node i. The final node must have type 0xff; the aligned end/size value derived from that terminator is passed to FUN_0046f4e0 and recorded in DAT_0056dcb0.

FUN_004c8050 handles the level's autoexec node before normal object creation. In one-player mode it scans DAT_0056e210[0..DAT_0056e214) for node type 4 and calls FUN_004c5dc0(node + 2, node_index, 1). In two-player mode it first looks for type 15, then falls back to type 4. This is the direct load-to-script-dispatch edge.

FUN_004c8130 runs after the autoexec pass. It first prepares restart positions, then iterates the same pointer table. Relevant branches are:

- type 1/7: FUN_004c5460 creates a gameplay object from the node;
- type 5: FUN_004c5460 creates a pickup/powerup object;
- type 2/9 and type 6: FUN_004c84d0 creates a small trigger-link/command-point runtime record;
- type 8: restart-point position/name data is collected for later restart handling; and
- type 12/14: another node/object allocation path is entered through FUN_004bd760.

The same command dispatcher is reused after loading. FUN_004c58b0 is the Trig_SendPulseToNode path. For a type-6 command point it finds the runtime record with FUN_004c5ac0, increments that record's byte at +0x07, and calls:

FUN_004c5dc0(*(u16 **)(runtime_command_point + 0x00), node_index, 0)

Restart execution follows the analogous path in FUN_004c4e30: the selected restart node is resolved by name through FUN_004c4c50, player positions are copied from FUN_004c8650, the two fields after the position triplet are copied to player restart fields at `+0x14` and `+0x18`, and the command stream after the restart name is passed to FUN_004c5dc0.

## Runtime structures

The loaded trigger representation is conservative but sufficient for dispatch:

DAT_0056e20c  -> heap/file allocation beginning with _TRG
DAT_0056e210  -> u32 node-pointer table at allocation + 0x0c
DAT_0056e214  -> node count
DAT_0056e210[i] -> node i, beginning with u16 node_type

FUN_004c84d0 allocates 0x18 bytes for the command-point record. Its writes support this provisional layout:

+0x00  command_stream      pointer passed to FUN_004c5dc0
+0x04  state byte, cleared
+0x05  state byte, cleared
+0x06  state byte, cleared
+0x07  pulse/count byte, incremented by FUN_004c58b0
+0x08  u16 state, cleared
+0x0a  u16 source node index, used by FUN_004c5ac0 lookup
+0x0c  u32 event/link key
+0x10  next record in the key bucket
+0x14  next record in the all-command-point list

The field names are intentionally provisional. The allocation size and offsets are directly visible in FUN_004c84d0; the pointer, node index, and two linked-list fields are independently consumed by FUN_004c58b0/FUN_004c5ac0.

Opcode `0xab` has a separate runtime-object allocation path. The retail
dispatcher aligns `(opcode + 5)` down to a four-byte boundary, reads one u32
key and three u16 values, allocates `0xcc` bytes, and calls FUN_00401060. That
constructor installs vtable `0x51836c`, links the object into the live list
headed by DAT_0055f6b0, resolves the u32 key through FUN_004b1de0 into the
object's `+0x1a` field, and assigns the list identifier at `+0xc8`. The base
object's list links are `+0x20`/`+0x34`; its flag word is at `+0x04`. The
constructor does not read the three trailing u16 values in this PC build, so
the native runtime preserves them as raw parameters. This branch occurs 118
times in the extracted corpus, predominantly in restart streams; native
`LevelTriggerState::script_objects()` now exposes the created objects when
those restart streams execute, and `0x83`/`0x84` can address their recovered
identifier space.

## Source asset correlation: command point -> SendVisible

SKWARE_T.TRG node 141 is the first unambiguous source-to-runtime command sample:

file offset  size  type  payload
0x17f0       0x2a  6     14 linked node ids, then 0x000d 0x0001 0xffff

The node's 14 linked ids are:

0x00cf 0x00f3 0x00e0 0x00ce 0x00c3 0x00d2 0x00d3
0x00bc 0x00d4 0x00cb 0x00d0 0x00df 0x00cc 0x00cd

For a type-6 node, the type/count/link area is followed by a u32 key and then the command stream. For this record:

node pointer       = DAT_0056e20c + 0x17f0
link count         = *(u16 *)(node + 0x02) = 14
event/link key     = *(u32 *)(node + 0x20) = 0
command stream     = node + 0x24 = { 0x000d, 0x0001, 0xffff }

FUN_004c8130 reaches its type-6 branch, and FUN_004c84d0 stores node + 0x24 in the new runtime record's +0x00 field and 141 in +0x0a. When a pulse is sent to node 141, the execution path is:

SKWARE_T.TRG node 141 @ file+0x17f0
  -> loaded node pointer DAT_0056e210[141]
  -> command-point record from FUN_004c84d0
       +0x00 = loaded node + 0x24
       +0x0a = 141
  -> FUN_004c58b0(141)
  -> FUN_004c5dc0(command_stream, 141, 0)
  -> opcode 0x000d, value 1
  -> FUN_004c77f0(141, 1)
  -> static diagnostic string "SendVisible"
  -> linked node gameplay-object flags at object + 0x04 are changed

The 0x0d case in FUN_004c5dc0 is direct: it calls FUN_004c77f0(param_2, *puVar27). FUN_004c77f0 obtains the link list with FUN_004c8550, resolves each linked node's runtime object with FUN_004b1ef0, and updates its u16 field at +0x04. With value 1, the implementation clears mask 0x41 rather than setting it. The function's embedded string names this operation SendVisible; the exact two-bit semantics of the target object's +0x04 field remain unconfirmed, so “make linked level objects visible/active” is an inferred gameplay interpretation, not a recovered field name.

The target filters are also now bounded. `FUN_004c5b60` (SendSignal) and
`FUN_004c5c70` (SendSuspend/SendActivate) only forward linked type-1/type-7
gameplay objects to their live object lists. `FUN_004c77f0`'s visible path
handles type-2/type-5/type-9/type-12/type-14 crate/link records; type-1/type-7
objects are diagnosed as non-crates instead. Native `LevelTriggerState` keeps
the command event record for every link but only mutates the same accepted
target families.

The linked Warehouse nodes are mostly type 2 nodes, with one type 12 node. Those are exactly among the node types handled by FUN_004c77f0's visible-object path. This makes node 141 a source-correlated level event rather than only a generic script parser example.

## Secondary samples

The first Warehouse node is type 4 at file offset 0x04f0 and contains the load-time autoexec stream:

0x009d 0x0003
0x008c "Re_Start_Skate"
0x00b2 "Re_Start_2P"
0x0093 0x0001
0x007e "SkWare_L"
0x007e "SkWare_O"
0x008e "SkWare_O"
0xffff

FUN_004c8050 sends this stream through FUN_004c5dc0 during level loading. For example, opcode 0x7e calls FUN_004b37a0 with the following string, then advances over the NUL-terminated string with FUN_004c5d70; this is a confirmed script-controlled resource-load action, but it is not used as the gameplay-behavior proof above.

The `0xb2 "Re_Start_2P"` entry is mode-gated: retail consumes the string in
ordinary mode, but in two-player mode it resolves and selects the named restart
through the same restart lookup used by `0x8c/0xb0`. Native
`TriggerRuntime::run_autoexec(true)` now preserves this distinction, so the
Warehouse type-15/two-player autoexec has a verified restart-selection path.

Warehouse restart node 167 is type 8 at file offset 0x1c06, named Ho_SkWare_HPGap. Its command stream begins with 0x0068, 0x000a, 0x4e20, 0x0002 and continues with 0x009d, 0x00a6, 0x00a9, and 0x0080 "SkWare". FUN_004c4e30 resolves restart nodes by their name, copies the restart position to active players, and dispatches this stream through FUN_004c5dc0. The 0x68 case calls FUN_00464710(10, 0x4e20, 2), which sets a power-of-two fogging range; that visual side effect is observed statically but is outside the gameplay-action mapping above.

## Retail TRG corpus audit

The bounded script decoder in `tony.assets._trg_decode_script` was checked against all 32 retail PC `*_T.TRG` files in the extracted asset tree. It found 3,579 streams and 8,956 dispatcher records; two streams end in the retained opaque legacy tails. The decoder follows the retail cursor rules rather than assuming every command is a sequence of tightly packed 16-bit values:

- streams end with the 16-bit sentinel `0xffff`;
- NUL-terminated string operands advance with the byte-pointer alignment used by `FUN_004c5d70`;
- type-6 command-point keys use the original conditional 2-byte alignment;
- `0xab` and `0xc9` align `(command + 5)` down to a 4-byte boundary before reading their u32 payload; and
- `0x85`/`0x8d` contain repeated six-word fixed-point records followed by a byte `0xff` table marker.

The most useful command counts are:

| opcode | count | static branch / effect |
| --- | ---: | --- |
| `0x03` | 2,164 | send a pulse through `Trig_GetLinks` and the linked-object pulse path |
| `0xc9` | 1,821 | compare a gap checksum, complete or queue the matching gap, and pulse the source node on completion |
| `0x86` | 1,803 | initialize the command-point pulse count/state |
| `0x8d` | 330 | apply fixed-point path records through `FUN_004ca2d0` |
| `0x80` | 289 | load a named resource in mode 1, then optionally flush resource state |
| `0x68` | 288 | defer `FUN_00464710` fog-range update until the stream ends |
| `0xab` | 118 | allocate/register a script object from an aligned u32 key plus three raw u16 values |
| `0x0d` | 54 | `SendVisible`, updating linked level-object state |
| `0xcb` / `0xcc` | 8 / 8 | set or conditionally skip on one of eight career flags |
| `0xcd` | 0 | supported goal-condition branch in the dispatcher, absent from this 32-file corpus |

The corpus contains 41 values that decode as dispatcher cases, plus two old type-6 tails in `SKATE_T.TRG` and `SKPARK_T.TRG` beginning with `0xc5a5`; those tails do not reach the normal `0xffff` terminator and are retained as opaque bytes. The remaining low-frequency cases are statically recognized but not yet assigned gameplay names. This is intentional: the C++ implementation should preserve their raw operands and dispatch addresses until a side effect is correlated, rather than silently inventing semantics.

The gap path is the second strong source/runtime correlation after Warehouse node 141. Each `0xc9` record carries an aligned u32 checksum and a u16 argument. The dispatcher first compares that checksum with the command-point record at runtime `+0x0c`, then searches the level gap table. Depending on gap flags it calls scoring/completion helpers immediately or stores the command-point source for a deferred gap action. This is a confirmed gap-gameplay path from the retail dispatcher; the exact score/checklist field names remain unresolved.

## Live Warehouse launch sample

A live WineDbg session against the same retail PC build reached the Warehouse path after forcing level 12. This sample confirms that the static pointers are live runtime pointers, not only decompiler artifacts. Heap addresses below are session-specific; the relative relationships are the useful result.

At `Trig_LoadTRG` 0x004c5130:

- the string argument was `"SkWare_T"`;
- `DAT_0056e20c` became `0x05f46ac0`;
- `DAT_0056e210` became `0x05f46acc`, exactly allocation + `0x0c`; and
- `DAT_0056e214` became `0x139` (313 nodes).

The first dispatcher entry, 0x004c5dc0, received stream `0x05f46fb2`, source node `0`, and mode `1`. That is `base + 0x4f2`: Warehouse node 0 at file offset `0x4f0`, plus its type word, matching the decoded autoexec stream.

The first live command-point allocation at 0x004c84d0 received key `0`, source node `1`, and stream `0x05f47050` (`base + 0x590`, node 1's type-6 stream). After the function returned, `DAT_0056e220` pointed to runtime record `0x05f44604`, whose live fields were:

```text
+0x00 = 0x05f47050  command stream
+0x04..+0x06 = 0    cleared state bytes
+0x07 = 0            pulse count before the first pulse
+0x08 = 0            state
+0x0a = 1            source node
+0x0c = 0            checksum
+0x10 = 0            prior bucket entry
+0x14 = 0            prior all-record entry
```

The first observed `Trig_SendPulseToNode` call was for source node 1. Its dispatcher entry received the same stream, source node `1`, and mode `0`; the bytes were `{0x0086, 0x0001, 0x0003, 0xffff}`. At that dispatcher entry the runtime record's byte at `+0x07` had changed from 0 to 1. This is a live load → runtime record → pulse → dispatcher correlation and independently validates the command-point offsets documented above.

## Native C++ replay validation

The portable implementation in `src/trg/trg_runtime.*` keeps the same relative offsets and node indices while replacing retail pointers with bounded spans and list indices. `src/trg/level_trigger_state.*` now supplies a deterministic renderer-independent state service for linked nodes, object flags, event ordering, timers, restarts, and objective state. `src/trg/level_runtime.*` composes that service with PSX binding, scene entities, frame ticks, pulses, restarts, and catalog-backed resource requests. `src/assets/psx_asset.*` parses the scene-side fixed-point object/model tables, geometry, texture metadata/palettes, tags, and blockmaps. Running the TRG inspector against Warehouse alone reports:

`GameplaySession::pulse_node()` and `pulse_checksum()` now expose that same
dispatcher boundary to gameplay code: a node/checksum event executes the TRG
stream, applies the resulting `LevelTriggerState` mutations, and refreshes
the level scene registry before returning. The Warehouse source sample is
therefore exercised end to end in the session test (`node 141` -> `0x0d` ->
linked-object visibility state), alongside the named restart path.

```text
nodes=313 command_points=109 objects=53 pickups=12 positioned=65 oriented=65 restarts=9 resources=2 legacy=1 diagnostics=0
```

With `SKWARE.PSX` supplied, type-2/type-12 link keys bind to PSX model-name
hashes and scene instances. The scene registry composes the 252 PSX static
entities with 67 trigger-created entities:

```text
nodes=313 command_points=109 objects=53 pickups=12 positioned=65 oriented=65 restarts=9 resources=2 bound_models=95 scene_instances=95 scene_positioned=95 scene_entities=319 scene_static=252 scene_trigger=67 scene_bound=95 scene_unresolved=67 legacy=1 diagnostics=2
```

The two diagnostics are type-2 keys `0xbd7ce256` and `0x3be890f8`; they do not
occur in the Warehouse PSX model-name table and remain unresolved rather than
being assigned a guessed model. The native PSX inspector parses all 282
extracted PSX files successfully, including texture tables where present (for
example `SKB1.PSX`: 186 objects, 186 models, 649 faces, 51 textures, 39 4bpp
palettes, and 10 8bpp palettes). The optional `--warehouse-gaps` mode binds
the recovered 132-record general/Warehouse checklist table. Its 44-byte
records are flags, an unknown word, divider ID, signed score, and a 36-byte
display name; ordinary flags complete and pulse once, while flags `0x08` and
`0x40` remain on the deferred path pending the player/checklist service.

The native spawn state also records the retail object-factory family selected
by type-1/type-5/type-7 subtype: `0xcb` enters `FUN_00403000`, `0x192` enters
`FUN_0049f250`, `0xd5` through `0xdc` enter the special-vehicle path at
`FUN_00412640`, and type-5 records enter pickup construction at
`FUN_004a8e50`. This is a class/factory correlation, not yet a claim that the
native renderer has created the final object.

Factory resource resolution is also verified against the extracted catalog:
`SKPH_T.TRG` + `SKPH.PSX` produces 10 special-vehicle entities, and all 10
resolve to and lazily parse the retail `C_TAXI.PSX` resource family through
the case-insensitive catalog; the native records retain its object/model
counts. Warehouse has no vehicle factory records, as expected from its subtype
distribution.

The same load → autoexec → build pass succeeds for all 32 extracted retail TRG files. The native runtime also reproduces the command-point pulse budget: `0x86 N` initializes the runtime `+0x08` state, `0x03` suppresses propagation at zero, decrements finite budgets after sending linked pulses, and treats `0xffff` as unlimited. Its linked-node pulse routing now includes the retail type-10/type-11 object branches in addition to the type-1/type-5/type-7/type-12/type-14 paths. Restart-selection commands `0x8c`/`0xb0` resolve a type-8 node by name without prematurely executing it; `0xb2` selects the named restart only in two-player mode. Opcode `0x98` is now source-correlated on `SKB1_T.TRG`: node 11 links restart node 2, and the dispatcher applies that restart's position plus `+0x14/+0x18` auxiliary fields through the native restart service before the following pulse command. Opcode `0xab` is represented as a bounded script-object creation record rather than a guessed gameplay effect. `GameplaySession::pulse_node()` consumes the resulting `RestartApplied` event and resets the player/clock at the same boundary as explicit restart execution.

## Interpretation

The recovered runtime model is a two-stage system:

1. FUN_004c5130 owns the loaded TRG byte allocation and converts the file's relative node offsets into a pointer table.
2. FUN_004c8130 creates gameplay objects and compact command-point records from those pointers. Command streams are not copied into a new bytecode heap in the observed paths; the runtime records retain pointers into the loaded TRG allocation.
3. FUN_004c58b0 and FUN_004c4e30 are event/restart entry points into the shared FUN_004c5dc0 opcode switch.
4. The dispatcher calls ordinary gameplay/resource helpers. The node-141 sample maps opcode 0x0d to the level-object visibility/activation path at FUN_004c77f0.

This is enough to name the central dispatcher Trig_CommandDispatch provisionally. It should not be confused with skater physics or collision code.

## Open questions / falsifiers

- The live Warehouse launch already confirms the command-point allocation, +0x00 stream pointer, and +0x07 pulse increment; repeat this capture only when validating a replacement executable identity.
- Determine the precise meaning of target-object +0x04 bits 0x01 and 0x40; a flag watch around the linked type-2 objects would strengthen “visible/active.”
- Correlate the remaining command-point opcode families, especially the deferred gap/player-position services and the live event callers for 0xcd goal filtering.
- Recover the object factory and linked-object field meanings behind the type-1/5/7/12/14 node branches; the trigger dispatcher and static type-2/type-12 PSX join are now bounded, but their gameplay helpers still need their own evidence.
- Type-10/type-11 pulses now reach the native gameplay boundary, but the retail helpers FUN_004aa420/FUN_004bdbd0 still need object-factory and state-transition evidence before their effects can be implemented.
- The current evidence is for the retail PC build above. Do not reuse these addresses for the separate game/thps2-demo.exe build (3b39fd23...) without a new identity record.
