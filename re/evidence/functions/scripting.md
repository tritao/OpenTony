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

After the load/object pass, `Front_LoadGame` calls `FUN_004c4e30` before the
first gameplay loop. That call consumes the restart selected by the autoexec
(`0x8c`/`0xb0`), applies its position and auxiliary fields, and dispatches the
restart's post-name command stream. Native `GameplaySession::initialize()` now
reproduces this final load-to-player edge; explicit `execute_restart()` remains
available for later gap/death/restart events.

The restart facing handoff is now bounded as well. `FUN_004c4e30` copies the
restart record's position to skater `+0x08/+0x0c/+0x10` and history
`+0xbc/+0xc0/+0xc4`, copies the adjacent u32/u16 fields to `+0x14/+0x18`,
then calls `FUN_004c4d10`. That helper reads the high word of `+0x14`,
computes `((word - 0x800) & 0xfff)`, rotates the `-X` and `-Z` Q12 seed
vectors, installs `-Y` as the down column, and refreshes the nine-short
orientation plus `+0x30f4/+0x3100/+0x310c` basis. Native
`PlayerState::apply_restart()` now preserves the raw fields and applies this
same negative-identity/yaw restart matrix; the angle is represented by
`retail_restart_angle12()` rather than inferred from the TRG node name.

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

The remaining concrete PC field writers are now preserved as raw dispatcher
state rather than left in the legacy bucket. `0x99`/`0x9a` call
`FUN_0040bd00`/`FUN_0040bd10` and write signed values to the current object's
`+0x4d4/+0x4d8`; `0xa4`/`0xa5` write u16 values to `+0x4dc/+0x4de`; `0xa0`,
`0xa8`, and `0xac` write u16-derived values to `+0x504/+0x434/+0x436`; and
`0xad` copies `+0x3a4` to `+0x3dc`. `0xa3`/`0xb1` write skater fields
`+0x3198/+0x319c`. Finally, `FUN_0040f1a0` behind `0xa7` writes `+0x410` and
either initializes `+0x40c` or computes `+0x414` from the two u16 operands.
Native `LevelTriggerState` exposes those offset-named fields and retains each
source/opcode/operand record; it does not assume which live object the retail
global current-object pointer selects.

`0xa2` is a bounded unsupported branch rather than an unknown command. Retail
consumes its aligned NUL-terminated string, calls the diagnostic helper with
the embedded message `LoadAI command not supported`, and continues dispatch.
The native cursor now consumes the same string shape and emits that diagnostic
while retaining the command in the legacy callback stream.

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

The type-10/type-11 pulse branch is now also source-correlated. Retail
`FUN_004aa8c0` allocates a dedicated runtime-list object with the source node
index at `+0x06`, a state byte at `+0x04`, and a next link at `+0x24`; its raw
post-position u16 is read through `FUN_004a9f70`. `FUN_004aa420` reaches
`FUN_004aa3c0(node, 1)`, which writes that state byte to one, while the kill
path's `FUN_004aa410` writes it to zero. The native build now retains the
type-10/type-11 position and raw flag word, initializes the same derived mode
and state, sets the state on pulse, and clears it on kill. The extracted
corpus contains 9,732 type-10 and 394 type-11 nodes; these are no longer
discarded as unhandled nodes, although the larger factory/list behavior still
needs parity work.

The type-1/type-7 object factory also consumes a bounded option-byte list before
the fixed-point position. `FUN_004c5460` scans it with `FUN_004c5420`: option
`4` clears the constructed object's flag bit `0x2`, while type 7 additionally
sets bit `0x4`; when option `2` is absent, the factory performs an extra
environment/baddy-list registration check. Native `TrgFile::node_spawn_options`
and `LevelTriggerState` now preserve the raw option bytes and expose these
three derived factory boundaries. This is still a factory-input correlation,
not a claim that the final baddy AI/object list has been recreated. Across the
32 extracted TRGs there are 793 type-1 records; all carry option `2`, and 642
carry option `4`. Type 7 is retained as a statically verified factory branch,
but does not occur in this extracted corpus.

The returned type-1/type-7 object also has a verified initial target flag word.
`FUN_00403000` (subtype `0xcb`) enters the common object constructor with
`+0x04 = 0x41`; `FUN_0049f250` (subtype `0x192`) applies
`(+0x04 & ~0x02) | 0x111`. Native `TriggerObjectState::flags` now starts from
those constructor values before the SendVisible/SendKill mutations, while the
separate option-4/type-7 bits remain factory-side fields rather than being
incorrectly merged into `+0x04`.

Type-12/type-14 construction has a parallel bounded record. `FUN_004bd760`
registers the node's resolved link key at `+0x04`, source node at `+0x08`,
active byte at `+0x0a`, next record at `+0x10`, and the resolved live asset
pointer at `+0x14`. `FUN_004bdc40` resolves that key, installs the live asset,
sets its object flag, and writes the record active byte. Native
`LevelTriggerState` now records registration, marks the record active when the
type-12/type-14 node is pulsed, and preserves the verified asset-side writes as
`flags_or = 0x04` and marker `0x202020`; the actual `+0x14` heap pointer and
player-owner fields remain explicit seams.
The extracted corpus contains 3,886 type-12 nodes and 6 type-14 nodes; the
Warehouse TRG contributes the first family and node 120 is used as the native
end-to-end join fixture.

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

The bounded script decoder in `tony.assets._trg_decode_script` was checked against all 32 retail PC `*_T.TRG` files in the extracted asset tree. It found 3,594 streams and 9,000 dispatcher records; two streams end in the retained opaque legacy tails. The decoder follows the retail cursor rules rather than assuming every command is a sequence of tightly packed 16-bit values:

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

The corpus contains 41 values that decode as dispatcher cases, plus two old type-6 tails in `SKATE_T.TRG` and `SKPARK_T.TRG` beginning with `0xc5a5`; the complete opcode-kind count is 42 when that opaque tail value is included. Those tails do not reach the normal `0xffff` terminator and are retained as opaque bytes. The remaining low-frequency cases are statically recognized but not yet assigned gameplay names. This is intentional: the C++ implementation should preserve their raw operands and dispatch addresses until a side effect is correlated, rather than silently inventing semantics.

The native inspector now has `--dispatch-all`, which pulses every type-6 command point after load/build. Across all 32 extracted `.TRG` files this executes 3,244 command-point streams without a cursor or runtime exception. The two `0xc5a5` historical tails follow the retail unknown-command fallthrough: the native dispatcher records the unknown word, advances by its opcode only, and continues instead of rejecting the entire level.

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

## Skater action-script archive boundary

The level-side skater setup also has a separate action-script asset path. On
the identified retail PC build, `FUN_0046a250` calls `FUN_00492a90` after the
skater resources are prepared. `FUN_00492a90` loads `tricks.bin`, reads eight
signed 16-bit relative section offsets, and publishes the loaded image at
`DAT_0056a894`. Section 0 (`0x00ac`) is a 20-entry selector for player-input
short-record tables; the trailing signed offsets in those records resolve
against that image base. Section 1 (`0x3440`) is adjacent metadata, while
sections 3/4 (`0x7990` and `0x7d90`) are the per-player sequence-table
destinations. The extracted Warehouse-era `TRICKS.BIN` is 0x8190 bytes and
begins with offsets `0x00ac, 0x3440, 0x6bcc, 0x7990, 0x7d90, 0x1736,
0x161a, 0x2b3b`; these are asset-relative offsets, not pointers.

`FUN_00492de0` performs the on-demand build path: it calls `FUN_004bd1e0`,
walks the generated per-player sequence tables through `FUN_00492d50`, and
uses `FUN_00492d10`/`FUN_004bf6c0` to advance over each stream until opcode
`0x07`. Runtime `FUN_004925e0` scans the active sequence table against the
recent action records, resolves the selected signed-16-bit stream offset
directly against `DAT_0056a894`, and passes the stream through `FUN_00492290` into the
player action cursor at `+0x29cc`. This is separate from the TRG dispatcher;
it is the asset-to-skater-action path that eventually reaches
`FUN_004be450`.

The native player now preserves the remaining confirmed cursor lifecycle.
`FUN_00491b80` marks `+0x29c8` active, stores the resolved stream at `+0x29cc`,
initializes `+0x29c0` to `0x7b`, and dispatches it in the same update through
`FUN_00492ea0`; opcode `0x07` clears the active state. When opcode `0x2c`
finds pending queued motion, retail rewinds `+0x29cc` by one byte and returns
to the frame loop. Native `PlayerState` retains the signed stream-relative
identity and a stream-local byte cursor, resumes it before attempting a new
history match, and clears the active state only on completion or malformed
input. A two-frame fixture verifies `0x2b -> 0x2c` yield, queued-motion drain,
resume, and `0x07` completion through `PlayerPhysicsFrame`.

The native boundary is now represented by `src/assets/tricks_bin.*`,
`src/runtime/action_sequence.*`, and `src/runtime/action_commands.*`. The
archive view resolves a bounded offset-table entry to a direct image-relative
stream span. Section 0 itself is a 20-entry selector for player-input
short-record tables: Warehouse index 0 points at `0x00d4`, index 1 at
`0x0ab0`, and the remaining observed entries point at the other bounded
tables. Those input tables are not command streams; their trailing signed
offsets resolve command streams against the loaded image. The view exposes
the 596-record `0x1736`/27-record `0x161a` source
tables. The sequence
runtime preserves the retail 32-record action-history ring, the generated
`[length][actions][stream-relative][flags]` table shape, age/mode filtering,
primary-action fallback, and bounded backward matching. The player/frame seam
can publish the profile records, select a generated entry, resolve its stream,
and invoke the command dispatcher. The command layer maps the confirmed
`0x01`, `0x0a`, `0x0e`, `0x0f`, `0x13`, `0x14`, `0x1c`, `0x1f`, `0x23`, `0x29`,
`0x2b`, `0x2c`, `0x2d`, and `0x07` effects and retains the retail
`FUN_004bf6c0` cursor-width table for unknown fixed, NUL-string, and relative
commands. The builder at `0x004bd1e0`/`0x004bcf00` now synthesizes the
zero-filled per-player table sections from the metadata/resource sections;
the remaining asset-side bridge is the live selection/configuration population
described below. No generic keyboard-acceleration meaning is assigned to the
recovered action opcodes.

The four newly mapped direct cases preserve raw action-object state through
`ActionCommandRuntimeState`: `0x1f` writes signed i16 to `+0x29ec`, `0x23`
writes signed i16 to `+0x2f00`, and `0x29` writes signed i16 to `+0x2c0c`.
`0x2d` writes the integer absolute value of its signed i16 argument to
`+0x2e2c`. These fields are source-correlated but intentionally unnamed until
their consumers are traced.

Header word 7 is a separate special-resource pointer at `0x2b3b`, used by the
nonzero-mode path of `FUN_004bbf00` instead of the normal section-0 stream
lookup. Its shipped leading word is `0x0100`, so the native view exposes it as
a raw bounded span rather than incorrectly decoding it as an ordinary
`[length][actions...]` table. Its internal record format and versus-mode
selection rules remain open.

The builder is no longer an opaque allocation boundary. `0x004bd1e0` allocates
`0x140c` bytes and calls the `0x004bb4f0` constructor. The constructor walks the
source records from header section 5 (`0x1736`) and registers each stream in a
heap record array whose count is at `object+0x1404`, whose first record begins
at `object+0x04`, and whose stride is `0x28`. The recovered fields are the
stream-relative key at `+0x20`, source/type data at `+0x22/+0x24`, and copied
source flags at `+0x26`; `0x004bd170(index)` returns
`object + 0x04 + index*0x28`. The constructor's post-pass promotes raw flag
bits `0x0800`, `0x1000`, `0x2000`, and `0x8000` into the filter word at
`+0x24`; raw `0x4000` clears the provisional `0x0800` bit and promotes
`0x4000`. Before that post-pass, `FUN_004bb7e0` starts the filter at `0x7b`,
walks the direct stream with `FUN_004bf6c0`, and replaces it when opcode
`0x51` reads a signed little-endian 16-bit value. The native exact resource
parser now reproduces this stream metadata walk when the loaded image is
available; the image-free overload retains the conservative raw-flags
fallback for isolated fixtures.

The runtime selection bytes are not part of the TRICKS.BIN source tables. The
normal builder obtains a player slot through `FUN_004416050(index)`, whose base
is `0x568a6c + index*0x104`, and passes `slot + 0xcc` to the four direct-group
builder calls. The selection view stores mapped resource IDs at view-relative
`+0x2b` and independent mapping indices at view-relative `+0x30`; for the
normal slot these are physical `slot + 0xf7` and `slot + 0xfc`. The
`FUN_004c36d0` path reads and updates those pairs through `FUN_00416340` and
`FUN_00416380` while processing a loaded resource. Native configuration now
keeps these arrays separate, and `RetailActionResourceSelection` models the
confirmed lookup/update behavior: `0xff` empty resource slots, mapping-index
replacement, duplicate-resource rejection, first-empty-slot insertion, and
the `-1/0xff` resource-removal sentinel. `FUN_00416380` stores the mapping
byte verbatim, including `0xff`; the trick-selection caller at
`0x004c36d0` validates its resource-derived mapping value to `0..0x2a`
before the update. These are selection-view offsets, not direct
player-slot offsets: the normal setup passes `FUN_004416050(index)+0xcc`, so
the mapped fields are physically slot `+0xf7/+0xfc`; the helper sees them as
view-relative `+0x2b/+0x30`. The unresolved work is to connect the actual
selection/config setup caller to this native object at the correct lifecycle
point, rather than invent values from the archive. The lifecycle seam is now
narrower: the setup constructor at `0x004c04de` selects either the special
`0x0056a690` array or `FUN_004416050(mode)+0xcc` at object `+0x2154`, and the
resource-processing path at `0x004c3bbd` enters `FUN_004c36d0` after validating
the loaded resource. The remaining native equivalent is the loaded-resource
name/index mapping, not a TRG loader.

The remaining live-selection bridge is now source-correlated. During the
resource setup path, `0x004c3bbd` obtains a source-record index from
`FUN_004bc330` (the `+0x24` filter-word scan), then calls `0x004c36d0` with
that index. The selection routine matches a loaded resource in the 0x60-entry
resource list, validates its type mask through `FUN_004c3350`, and uses the
loaded record's `+0x08` value as the mapping candidate. When the source ID is
not already installed, it calls `FUN_00416380` with the semantic pair
`(resource_id = source-record index, mapping_index = loaded-resource +0x08)`.
This is the concrete native bridge still needed for automatic mapped-action
population; it is not a TRG record field or a level-script opcode.

The native builder now exposes this bridge explicitly. `retail_action_resource_id_for_filter`
implements the first-match `FUN_004bc330` scan over constructor `+0x24` filter
words. `RetailLoadedActionResourceRecord` retains the loaded entry's `+0x08`
mapping value, and `bind_loaded_action_resource` limits the catalog walk to
the retail 0x60 entries, requires the `FUN_004c3350` result's `0x8000` bit,
rejects mapping values outside `0..0x2a`, installs the pair when absent, and
reports the retail already-installed versus conflicting-mapping cases without
silently replacing the live selection. The remaining caller-side input is
the resource/type catalog that supplies the current `FUN_00470ab0` key; no
TRG field is used as a substitute.

`0x004bcf00` is now ported in bounded form by
`src/runtime/action_sequence_builder.*`: it parses the section-0 input table,
matches each trailing stream-relative key to the first constructor resource
record, emits ordinary records for masks `0x1000`, `0x0800`, `0x2000`,
`0x4000`, and `0x0300`, then applies the four static combo groups and the
five-entry mapping pass. `0x004bcdd0` emits the static player-combo map at
`0x540e30`, copying the selected source record's `+0x20` and `+0x26` into the
final stream-relative/flags pair; `0x004bcb70` handles ordinary filtering and
`0x004bcc90` applies the four mask-selected groups. The remaining unresolved
inputs are the runtime selection view at player record `+0xcc` and its mapped
fields at view-relative `+0x2b`,
plus the special alternate-resource path selected in versus mode. Static setup
identifies the player record stride as `0x104`, the direct selection view as
`slot + 0xcc`, the five mapped resource IDs as view `+0x2b` (physical slot
`+0xf7`), and their independent mapping indices as view `+0x30` (physical
slot `+0xfc`). They are explicit native inputs rather than guessed constants;
the builder materializes them through the native selection object before applying the retail passes. The Warehouse session tests now exercise the real
archive through this builder both with the optional groups unset (ordinary
records) and with source resource ID `0` plus mapping index `0` (the mapped
static pass), proving both generated-table branches before the matcher and
stream resolver.

## Native C++ replay validation

The portable implementation in `src/trg/trg_runtime.*` keeps the same relative offsets and node indices while replacing retail pointers with bounded spans and list indices. `src/trg/level_trigger_state.*` now supplies a deterministic renderer-independent state service for linked nodes, object flags, event ordering, retail timer-reset traces, restarts, and objective state. `src/trg/level_runtime.*` composes that service with PSX binding, scene entities, frame ticks, pulses, restarts, and catalog-backed resource requests. `src/assets/psx_asset.*` parses the scene-side fixed-point object/model tables, geometry, texture metadata/palettes, tags, and blockmaps. Running the TRG inspector against Warehouse alone reports:

`GameplaySession::pulse_node()` and `pulse_checksum()` now expose that same
dispatcher boundary to gameplay code: a node/checksum event executes the TRG
stream, applies the resulting `LevelTriggerState` mutations, and refreshes
the level scene registry before returning. The Warehouse source sample is
therefore exercised end to end in the session test (`node 141` -> `0x0d` ->
linked-object visibility state), alongside the named restart path.

```text
nodes=313 command_points=109 objects=53 pickups=12 positioned=130 oriented=65 restarts=9 resources=2 bound_models=0 scene_instances=0 scene_positioned=0 scene_entities=0 scene_static=0 scene_trigger=0 scene_bound=0 scene_unresolved=0 legacy=0 diagnostics=0
```

With `SKWARE.PSX` supplied, type-2/type-12 link keys bind to PSX model-name
hashes and scene instances. The scene registry composes the 252 PSX static
entities with 67 trigger-created entities:

```text
nodes=313 command_points=109 objects=53 pickups=12 positioned=130 oriented=65 restarts=9 resources=2 bound_models=95 scene_instances=95 scene_positioned=95 scene_entities=384 scene_static=252 scene_trigger=132 scene_bound=95 scene_unresolved=132 legacy=0 diagnostics=2
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

For type-12/type-14 activation, `FUN_004bd760` allocates the compact runtime
record with source node at `+0x08`, active byte at `+0x0a`, owner byte at
`+0x0b`, control value at `+0x0c`, and the resolved live asset pointer at
`+0x14`. `FUN_004bdc40` fills the owner/control values from the current-player
globals before setting the active byte and mutating the asset. The native
`LevelTriggerState::set_special_runtime_context()` boundary now preserves
those raw owner/control writes when the player service supplies them; it does
not assign a player meaning to the control value or claim to own the retail
heap pointer.

Opcode `0x9e` reaches `FUN_00466c10`. Its mode/versus counter branches still
need player-state inputs, but the first-call initialization is exact: it sets
the event latch at `DAT_00568658`, writes `0x50` to `DAT_00568818`, writes
`0x40` to `DAT_00568610`, clears the secondary player's `+0x3144` turn field,
and sets `DAT_006a3d49`. The native state records that one-shot initialization,
its raw values, and each `0x9e` event without inventing the missing player
ownership/stat inputs.

Factory resource resolution is also verified against the extracted catalog:
`SKPH_T.TRG` + `SKPH.PSX` produces 10 special-vehicle entities, and all 10
resolve to and lazily parse the retail `C_TAXI.PSX` resource family through
the case-insensitive catalog; the native records retain its object/model
counts. Warehouse has no vehicle factory records, as expected from its subtype
distribution.

The same load → autoexec → build pass succeeds for all 32 extracted retail TRG files. The corpus-wide `--dispatch-all` run executes 3,244 command-point streams with no cursor/runtime failures; the reported diagnostics are retained as evidence for unresolved retail branches, not treated as parser success. The native runtime also reproduces the command-point pulse budget: `0x86 N` initializes the runtime `+0x08` state, `0x03` suppresses propagation at zero, decrements finite budgets after sending linked pulses, and treats `0xffff` as unlimited. Its linked-node pulse routing now includes the retail type-10/type-11 object branches in addition to the type-1/type-5/type-7/type-12/type-14 paths. Restart-selection commands `0x8c`/`0xb0` resolve a type-8 node by name without prematurely executing it; `0xb2` selects the named restart only in two-player mode. Opcode `0x98` is now source-correlated on `SKB1_T.TRG`: node 11 links restart node 2, and the dispatcher applies that restart's position plus `+0x14/+0x18` auxiliary fields through the native restart service before the following pulse command. Opcode `0xab` is represented as a bounded script-object creation record rather than a guessed gameplay effect. `GameplaySession::pulse_node()` consumes the resulting `RestartApplied` event and resets the player/clock at the same boundary as explicit restart execution.

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
- Type-10/type-11 state transitions are now mapped through FUN_004aa8c0/FUN_004aa420/FUN_004aa410; the remaining question is the larger factory/list behavior around those records and the separate type-12/type-14 FUN_004bdbd0 path.
- The current evidence is for the retail PC build above. Do not reuse these addresses for the separate game/thps2-demo.exe build (3b39fd23...) without a new identity record.
