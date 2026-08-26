# PC binary-asset runtime paths

Status: confirmed for `TRICKS.BIN` section/database/bytecode/score handoff and `CRETEX.BIN`; a generic relocatable
module helper is observed, but the extracted module pairs remain unproven or
belong to the legacy/console side of the asset set
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0041db50`, `0x0041d860`, `0x0041dea0`, `0x0041ef50`,
`0x0048cbc0`, `0x0048cd40`, `0x0048d710`, `0x0048e680`, `0x0048e8a0`,
`0x0048f720`, `0x00491b80`, `0x00492a90`, `0x004bb4f0`, `0x004bb7e0`,
`0x004bba50`, `0x004bc300`, `0x004be450`

The `.BIN` extension covers several unrelated formats. Two files have a
provable PC runtime path:

```text
TRICKS.BIN  -> raw resource read -> seven relative section pointers
            -> trick/physics manager -> runtime trick tables

CRETEX.BIN  -> raw resource read -> null-separated texture-set records
            -> texture database lookup -> runtime texture-set manager
```

This is separate from the MIPS-style menu/editor binaries, which have no
matching PC loader or filename cross-reference in this executable.

## TRICKS.BIN

The extracted `data/TRICKS.BIN` is 33168 bytes with SHA-256
`3f8846a9577dd908dc59e05bfeaca79cdf0d64ca2be4321f9c48ce7528848ff7`.
`0x00492a90` is the one-time loader. It requests the lower-case resource name
`tricks.bin` through the raw resource reader `0x00448dd0`, then treats the
returned buffer as a table of signed 16-bit offsets:

```text
section_0 = buffer + (s16)buffer[0x00]   // 0x00ac in the extracted file
section_1 = buffer + (s16)buffer[0x02]   // 0x3440
section_2 = buffer + (s16)buffer[0x04]   // 0x6bcc
section_3 = buffer + (s16)buffer[0x06]   // 0x7990
section_4 = buffer + (s16)buffer[0x08]   // 0x7d90
section_5 = buffer + (s16)buffer[0x0a]   // 0x1736
section_6 = buffer + (s16)buffer[0x0c]   // 0x161a
```

The seven resulting pointers are published in globals used by the trick
manager. The loader is idempotent: once the global resource handle is set,
later calls return without rereading the file. This is a table-of-offsets
loader, not a decompression or object allocator; the allocation belongs to
the common raw resource layer.

The shipped image has one additional signed word at `buffer + 0x0e`
(`0x2b3b`), but the exact-build loader at `0x00492a90` reads and publishes
only the seven offsets at `+0x00..+0x0c`. That eighth header word is retained
as raw file data here; no runtime section is assigned to it without a caller
that independently reads it.

`0x004bb4f0` is the downstream manager constructor/initializer. It calls the
loader, stores the successful result at manager `+0x1408`, and
validates relationships between the section pointers. It then reads a version
word from one section and selects one of several fixed runtime table sizes
(`0x200`, `0x100`, `0x800`, or `0x2000` in the observed branches). The same
manager initializes its record count at `+0x1404`; `0x004bba50` appends records
inline at `manager + 0x04 + count * 0x28` and enforces a maximum of `0x80`
records. Thus the fixed manager prefix is independently bounded through
`+0x1408`: 128 records, a count, and the retained raw resource pointer.

The constructor also proves the descriptor dispatch rather than merely
observing it in the first record: it reads a 1..4 button-count prefix, uses the
word at that indexed position as the type, and maps types 1/2 to table class
`0x2000`, type 9 to `0x100`, type 10 to `0x200`, and type 12 to `0x800`.
The following signed offset is added to the loaded TRICKS base, and the next
word is passed as descriptor flags. The descriptor walk advances by exactly
`prefix_count + 3` 16-bit words, so the next record is not found through a
second offset field.

The section stream has a second independently supported record boundary. Each
descriptor begins with a 1..4 prefix-count word; the executable uses that word
as an index to find the type, then reads a signed data offset and one raw
descriptor word. The next descriptor begins immediately after those fields;
there is no separate next-offset field. `0x004bb4f0` selects the runtime table
class from the type (`0x2000` for types 1/2, `0x100` for 9, `0x200` for 10,
`0x800` for 12) and passes the resolved data pointer to `0x004bb7e0`.

The other published sections have distinct consumers. Section 1 (`0x3440`) is
the 0xd0-entry signed-offset table used by `0x00491c10` for bounded grind
selection (`0..0xcf`). Section 2 (`0x6bcc`) supplies the signed offsets used
by the skater trick/state-start path in `0x004904d0`. Sections 3 and 4 are the
zero-filled/generated per-player destinations at `0x7990` and `0x7d90`;
sections 5 and 6 are the source streams at `0x1736` and `0x161a`. These roles
are runtime consumer classifications, not names for the intermediate table
records.

For the first descriptor in the loaded GTricks section at file offset `0x1736`,
the independently decoded values are:

```text
prefix-count = 2
prefix word  = 1
type         = 12
data offset  = 0x325e
descriptor   = 0x4032
data stream  = "Handplant" command stream
```

`0x004bb7e0` parses that stream, including `Q` count and `X` flag tokens. It
also consumes the first vertical-tab and `P` name tokens, strips braces and
the `fs `/`bs ` prefix when the table class is `0x800`, and passes the cleaned
name to `0x004bba50`. The latter searches the
manager's existing 0x28-byte records by their 16-bit source offset through
`0x004bc300`; if absent it appends a new record at `manager + 0x04 + count *
0x28` and copies the name. The proven record fields are:

```text
+0x00..0x1f  NUL-terminated trick name (maximum 31 characters plus NUL)
+0x20         source data offset / lookup key (u16)
+0x22         decoded Q value (u16)
+0x24         runtime table-class flags (u16)
+0x26         raw descriptor word / flags (u16)
+0x28         record stride
```

For the first `Handplant` descriptor, the initial record values are therefore
`source=0x325e`, `Q=0x07d0`, `table_class=0x0800`, and `descriptor=0x4032`.
The merge logic then propagates descriptor bits `0x0800`, `0x1000`, `0x2000`,
and `0x8000` into the runtime flags; descriptor byte bit `0x40` clears runtime
bit `0x0800` and sets runtime bit `0x4000`. A duplicate listed record sets
`0x8000` in both words and emits the duplicate warning. The record boundary,
flag propagation, and disk-to-runtime name/source correspondence are proven.
The downstream trick physics fields remain open, but the input-history,
sequence-match, delayed-start, and cursor-initialization path is now
independently documented below.

The manager/record ownership is now concrete even though the action semantics
are not. `TRICKS.BIN` is read once into a raw buffer; the manager retains that
buffer at `+0x1408`, owns up to 128 fixed records beginning at `+0x04`, and
publishes the live count at `+0x1404`. A faithful recreation can therefore
preserve the source offset as the stable bridge from a decoded command stream
to its runtime trick record without pretending that the table-class bits are
physics labels.

The level-side object bridge is now separate and proven in
[trick-object-runtime.md](trick-object-runtime.md): TRG type-12/14 nodes carry
model-name checksums into 0x18-byte runtime records, which are activated by
player/rail events and consumed by the per-frame trick-object update. This
does not yet assign the full `TRICKS.BIN` scoring/physics semantics.

### Per-player key table and script handoff

The runtime also materializes a per-player key table from the same database.
`0x00492de0` loads `TRICKS.BIN`, resets the extra-animation list, initializes
the trick manager through `0x004bd1e0`, and then parses the table at
`DAT_00568a50` (and `DAT_00568a4c` for the second player). The table-builder
`0x004bcf00` writes a zero-terminated sequence of variable-length entries. Its
ordinary and special append passes resolve each source offset through
`0x004bc300`, so the output does not duplicate an unrelated copy of the disk
database.

Each generated key-table entry has this physical layout:

```text
+0x00                 u16 button_count (1..4)
+0x02                 u16 button_code[button_count]
+0x02 + 2*count       s16 source_data_offset
+0x04 + 2*count       u16 descriptor_flags
+0x06 + 2*count       next entry; a zero button_count terminates the table
```

`0x004bcf00` combines player-return combos, ordinary built-in combinations,
special combinations, and configuration slots filtered by table-class masks.
The diagnostic bound is 512 shorts or less. The output table is therefore a
runtime selection/index structure, not a second format for the raw trick
script data.

The consumer boundary is explicit. `0x00492d50` walks the generated table,
adds each signed `source_data_offset` to the loaded TRICKS base, and passes
the resulting script pointer to `0x00492d10`. That stream parser validates
variable-length commands with `0x004bf6c0`; command type 1 registers an extra
animation through `0x00492ba0`, and the parser stops at command type 7. The
same source offsets are used by gameplay start paths: `0x00491c10` selects a
grind-script offset from the loaded section table, and `0x00491b80` stores the
resolved pointer at the skater's `+0x29cc` command cursor. `0x004be450` then
executes the cursor's bytecode during the physics/trick state transitions.

The resulting disk-to-runtime chain is:

```text
TRICKS.BIN section/key descriptor
  -> RuntimeTrickRecord (+0x20 source offset, +0x26 descriptor flags)
  -> per-player key table [button codes, source offset, flags]
  -> selected source offset + TRICKS base
  -> skater trick cursor +0x29cc
  -> 0x004be450 trick-script execution
```

The interpreter's raw command ABI is now established, including byte/u16/u32
operand widths and cursor advances. The direct state mutations and helper-call
edges are listed below; the original gameplay names and full physics effects
remain intentionally unassigned.

### Trick-script command/state contract

The command loop at `0x004be450` is more than a parser: it mutates the live
skater object and can leave its cursor at the current command for a later
physics frame. `0x004be3b0` reads one byte, `0x004be3c0` reads one little-endian
u16/s16, and every relative branch adds a signed 16-bit operand to
`TricksBinaryBase`. `0x004bf6c0` independently validates the same variable
lengths before the script-table parser advances.

The following effects are direct executable contracts. Names such as
`wait_frame` and `branch_if_*` describe the observed operation only; they are
not claims about the original source enum.

| Opcode | Operand | Proven effect |
| --- | --- | --- |
| `0x01` | `u16` | select animation `+0xf6`, clear frame `+0xf4` |
| `0x02` | `u16` | call the animation transition helper with the current animation and mode `0xd8` |
| `0x03` | none | negate direction `+0x100`, swap range fields `+0x114`/`+0x101`, clear `+0x107` |
| `0x07` | none | finish the script: clear `+0x2c68`/`+0x29c8`, set `+0x2c6c` |
| `0x09` | `s16` | advance frame `+0xf4` by direction and clamp to the active range |
| `0x0a` | `u16` | write script scalar `+0x29c0` |
| `0x0b` | NUL string | save string pointer at `+0x29c4`, advance after the terminator |
| `0x0c` | none | call the runtime action helper using `+0x29c0`/`+0x29c4`, set `+0x2c70` |
| `0x0d` | `s16` | frame-gated wait; when the comparison passes, rewind the cursor to this command |
| `0x0e` | `s16` | write `+0x2c64` |
| `0x0f` | `s16[3]` | write three position/vector words at `+0x4c`, `+0x50`, `+0x54` as `operand << 12` |
| `0x10` | `s16` | conditionally rewind on the frame gate and `0x004924f0` result |
| `0x11` | `s16` | conditionally call `0x0048f720(operand)` and rewind |
| `0x12` | `s16` | conditionally set the cursor to `base + operand` |
| `0x13`/`0x14` | none | write animation mode `+0xf8 = 2`/`0` |
| `0x15` | `s16` | unconditional relative branch |
| `0x16` | none | rewind while any pending movement word `+0x2ca0/+0x2ca4/+0x2ca8` is nonzero |
| `0x17` | `s16[2]` | call `0x0048f720` with a speed-scaled expression and the two script operands |
| `0x18` | `s16` | set tween/range fields `+0xfa/+0xfc/+0xfe`, select animation mode `+0xf8 = 3` |
| `0x19`/`0x2a` | `s16` | save relative pointers at `+0x29d0`/`+0x29d4` |
| `0x1a` | none | rewind while `+0x2e90` is nonzero |
| `0x1b` | `s16` | write `+0x2ed8`; the executable validates the value as `1` or `-1` (`RailFlip` diagnostic) |
| `0x1c` | none | write animation mode `+0xf8 = 4` |
| `0x1d` | `s16` | write fixed-point timing/scalar `+0x108 = operand * 0x20000 / 60` |
| `0x1e` | none | call the state/animation reset helper `0x004904d0(0, 0)` |
| `0x1f` | `s16` | write `+0x29ec` |
| `0x20` | none | clear script-active word `+0x2c68` |
| `0x21` | `s16` | rotate/rebuild the three basis vectors stored at `+0x2e58..+0x2e68` |
| `0x22` | none | toggle bit `0x02` in `+0xd8` |
| `0x23` | `s16` | write `+0x2f00` |
| `0x24..0x28` | `s16` | branch when one of raw condition words `+0x2be8..+0x2bf8` is nonzero |
| `0x2b` | `s16[3]` | write indexed words at `+0x2ca0` and `+0x2c94`; a zero third operand clears the associated pending slots |
| `0x2c` | none | rewind while any pending movement word `+0x2ca0/+0x2ca4/+0x2ca8` is nonzero |
| `0x2d` | `s16` | write `abs(operand)` to `+0x2e2c` |
| `0x2e` | `s16` | write `+0x2c10` |
| `0x2f` | `s16` | branch only when physics state `+0x30b8` is `1` or `2` |
| `0x30` | none | call `0x0046dd20(0, 0xfa)` |
| `0x31` | none | clear/reset movement and motion fields, then choose one of two animation/state transition helpers |
| `0x32`/`0x33` | none | call the two distinct helpers `0x0046c630`/`0x0046c5f0` |
| `0x34` | `s16` | if `+0x2c04` is nonzero, branch relative; then clear `+0x2c04` |
| `0x35` | none | set `+0x2c78` |
| `0x36..0x38` | `s16[1..3]` | submit one sound request with one, two, or three script arguments |
| `0x39` | `s16` count, `s16[count]` | choose one relative target using the runtime PRNG and branch to it |
| `0x3a` | `s16` | branch based on the bit `+0xd8 & 2` and byte `+0x163` |
| `0x3b`/`0x3c` | none | write `+0x2c80 = 1`/`0` |
| `0x3d` | `s16` | one-level `Gosub`: save return cursor at `+0x29d8`, branch relative |
| `0x3e` | none | `Gosub` return through `+0x29d8`, then clear it |
| `0x3f`/`0x40` | `s16` | write `+0x29f2`/`+0x29f0` |
| `0x41` | `s16` | branch when `+0x2bfc` is nonzero |
| `0x42` | `s16` | build/normalize a vector from skater state and replace position `+0x08/+0x0c/+0x10` through the shared math helpers |
| `0x43` | `s16` | branch when bit `0x02` in `+0xd8` is clear |
| `0x44` | `s16` | write `+0x29f4` |
| `0x45` | `s16[3]` | invoke vibration with the absolute first operand, motor `0/1`, and power `0..255`; the executable validates both ranges |
| `0x46` | none | call `0x00431260(+0x30b0, 1)` |
| `0x47` | `s16[2]` | branch when the current `+0x4c` vector magnitude crosses the supplied threshold |
| `0x48` | `s16[2]` | branch when the resolved frame has reached the supplied target, or `+0x107` is set |
| `0x49` | `s16` | reset motion/state fields, set physics state `+0x30b8 = 8`, rebuild position state, and clear `+0x2c68` |
| `0x4a` | none | choose a state-dependent transition/sound path and write a vertical motion word at `+0x50` |
| `0x4b` | `s16` | branch on the shared magnitude test involving `+0x30f4` and `+0x4c` |
| `0x4c` | `u8, s16` | branch when `+0x30b8` equals the byte operand |
| `0x4d` | none | set `+0x2c7c` |
| `0x4e` | `s16` | if `+0x31f8` is nonzero, clear it and branch relative |
| `0x4f` | `s16[4]` | choose one of four sound IDs by `+0x2e8c` and submit it; the source labels this path `TRICK_LIPSND` |
| `0x50` | NUL string | skip a NUL-terminated script string |
| `0x51`/`0x58` | `s16` | consume and discard one signed word |
| `0x52` | `u8` | write `+0x2dd4`; clear `+0x3078` when `+0x306c` is zero |
| `0x53` | `s16` | branch when `+0x29e8` is nonzero |
| `0x54` | `s16` | write `+0x2dfc` |
| `0x55` | `u8` | write `+0x31f8` |
| `0x56` | none | choose the `0x513`/`0x50f` animation/state transition path |
| `0x57` | none | set `+0x2c08` |
| `0x59` | `u8` | write `+0x3210` |

Commands `0x04..0x06` and `0x08`, plus any other byte not in the validator,
take the executable's undefined-command path. This table closes the
asset-to-bytecode-to-skater-state boundary while deliberately leaving the
original gameplay names of the raw fields and helper calls open.

### Trick point-stack handoff

The score-facing edge is indirect but concrete. Script commands `0x11` and
`0x17` call `0x0048f720`; several ground/air state paths call the same helper.
That helper advances the active trick timer at `+0x2f54`, checks the optional
limit at `+0x2f58`, and either requests the script/state reset through
`0x004904d0` or forwards a frame-scaled point adjustment to `0x0048d710`.

`0x0048d710` requires a nonzero trick-stack count at `+0x2858` and adds its
signed adjustment to the first word of the current 0x28-byte stack entry at
`+0x8f4 + count * 0x28`. It then calls `0x0048ddc0`, the panel refresh path.
The executable's diagnostic is “Tweaking trick, but nothing on stack” when
the count is empty, so this is a score/stack mutation rather than a generic
motion write. The exact stack-entry fields after the point word and the
score-table constant names remain open; the landed-total/best-score commit is
independently recovered below.

The adjacent panel display routine at `0x0048c1a0` contains the independent
diagnostic “Points on stack, and in mLandedTotal” and reads the landed-total
word at its `+0x2a8` object-relative position when preparing the score display.
This proves that the point stack and landed-total paths meet in the panel
layer, but not which landing helper performs the transfer or how combo
multipliers are applied.

### Landed score commit

The landing commit is now identified at `0x0048e680`, reached by the wrapper
at `0x0048d7a0` with the score state at `skater + 0x168`. When its stack count
at `skater + 0x2858` is nonzero, it calls `0x0048cd40`, stores the returned
value as the current landed-trick score at `+0x2ac`, and adds it to the
landed-total accumulator at `+0x2a8`. It also compares the value with the
per-skater best score at `+0x178`; a new best copies the current stack count
to the adjacent short field at `+0x176` and snapshots three stack-name slots.
The stack count is then cleared. This is the missing stack-to-landed-total
edge, independently tied to the same object whose `+0x2a8` field is read by
the panel display.

`0x0048cbc0` supplies the pre-multiplier stack sum used by `0x0048cd40`. For
each active 0x28-byte entry it combines the leading point word with a
record-local quality/index field, a table-selected factor, and a bounded
0..4 table entry, using the executable's fixed-point `/2` and `/100` steps.
When the skater's level flags at `+0x3078` or `+0x307c` are set, the final sum
is multiplied by 150% or 75%. `0x0048cd40` then applies the stack-count
multiplier (the first 15 counts use `DAT_00536194`; later counts use the
continuation formula through `DAT_005361cc`) and divides by two. The tables'
original gameplay names remain open, but the score arithmetic and its input
fields are no longer an unresolved asset boundary.

After committing the stack, `0x0048e680` passes the landed-total accumulator
and the owning skater pointer to `0x004cb5c0`, the downstream score/UI state
consumer. If a pending gap-trick pointer exists at `skater + 0x3024`, the
same commit path validates it through `0x00414d90` and clears both gap fields
at `+0x3024/+0x3028`. The separate `0x0048e8a0` path computes the same
current value with a negative sign for the rollback/cancel path and also
clears the stack after its side effects; it is recorded as a distinct reverse
edge rather than being conflated with the landed commit.

The proven selected-script handoff is consequently:

```text
TRICKS.BIN script cursor
  -> 0x004be450 opcode 0x11/0x17
  -> 0x0048f720 timer/point scaling
  -> 0x0048d710 current trick-stack point word
  -> 0x0048ddc0 panel/message refresh
  -> 0x0048e680 landed stack commit
  -> skater +0x2a8 landed total / +0x178 best score
  -> 0x004cb5c0 score/UI state consumer
```

### Input history, sequence matching, and queued script start

The remaining selection boundary is now explicit. `0x00492ea0` runs the player
trick update in this order: refresh the input-history records through
`0x00492190`, select matching entries through `0x004925e0`, advance the
short-lived pending queue through `0x00492400`, then execute the active stream
through `0x004be450` when `+0x29c8` is set. This separates input recognition,
script selection, delayed start, and bytecode execution.

`0x00492120` derives the composite action-pattern index used by the history
writer. The four action-bank bytes at `+0x80`, `+0x90`, `+0xa0`, and `+0xb0`
contribute weights `4`, `8`, `1`, and `2` respectively. The signed analog/
lean bytes at skater `+0x31a2` and `+0x31a1` contribute low/high threshold bits
`1/2` and `4/8` when below `-0x28` or above `0x28`. The resulting index selects
a value from the table at `DAT_005369c8`; the returned value is retained as a
raw action-pattern selector rather than being given a public trick name.

`0x00492190` publishes that selector as button states 1 through 8 and also
publishes six profile/action-bank bytes as states 9, 10, 11, 12, 14, and 16.
`0x00491c90` keeps a 32-entry history ring at skater `+0x2a14`, with an
8-byte entry stride:

```text
+0x00  u8 action/button code
+0x01  u8 pressed/state value
+0x04  u32 update timestamp or age anchor
```

The current per-button latch array begins at `+0x2b18`; a changed latch emits
one history entry and advances the ring cursor at `+0x2b14`, wrapping after
32 entries. This is the proven action-bank-to-history edge; the table values
remain operational because the original action names are not all recoverable
from this function alone.

`0x00492560` searches that ring backwards from the most recent entry, stopping
at its start sentinel. It ignores empty entries, requires the pressed byte to
be nonzero, and rejects entries older than the supplied timestamp window. Its
optional filter excludes action codes 5 through 8. `0x004925e0` walks the
generated variable-length player table, resolves each entry's signed stream
offset against `TricksBinaryBase`, compares its button sequence against this
history, and passes a matching stream request to `0x00492290`.

The delayed-start queue in `0x00492290` is another runtime object boundary.
Ordinary requests occupy ten 0x10-byte records beginning at `+0x2b38`; the
record stores the raw stream/request word at `+0x00`, the enqueue timestamp at
`+0x04`, and two selection/parameter words at `+0x08/+0x0c`. The queue count
and circular head are at `+0x2bd8` and `+0x2bdc`. A separate action-12 path
uses `+0x29dc/+0x2be0/+0x29e4` for its pending request and expiry values.

`0x00492400` consumes the ordinary queue only when no active script or blocked
state is present. It accepts raw physics states 1, 2, and 3, rejects entries
older than `0xb4` ticks, updates the current selection/time fields, expires
older history entries, and calls `0x00491b80` with the queued stream pointer.
`0x00491b80` then initializes the script cursor and state: it sets
`+0x2c68/+0x29c8`, clears `+0x2c6c`, `+0x2f58`, `+0x2f54`, `+0x29d0`,
`+0x29d4`, and `+0x29d8`, installs `+0x29cc`, resets `+0x29f0/+0x29f2`,
`+0x2e2c`, and `+0x3210`, sets `+0x108 = 0x10000`, and initializes
`+0x29c0 = 0x7b` with the static name `"Trick"` at `+0x29c4`.

The complete action-side path is therefore:

```text
action-bank/profile bytes + analog thresholds
  -> action-pattern table
  -> 32-entry pressed/timestamp history
  -> generated per-player key table
  -> signed TRICKS.BIN stream offset
  -> ten-entry delayed-start queue
  -> 0x00491b80 cursor/state initialization
  -> 0x004be450 bytecode and player-state writes
```

This closes the missing asset-to-player script-selection boundary. It still
does not assign the original names of the pattern-table values, remaining
score-table fields, or all gameplay effects produced by the selected bytecode.

The on-disk witness at `TRICKS.BIN + 0x325e` begins with the same command
grammar: `40 03 00` writes a script word, `0b` introduces the NUL-terminated
name `Handplant`, and `0a ee 02` writes a following u16 script value. The
remaining bytes select animation/range, movement, branch, and sound commands
through the table above. This is a direct file-byte-to-interpreter-state
example rather than a command table inferred only from the disassembly.

### Runtime character/config consumer

The loaded trick database has a direct downstream consumer in the career/menu
configuration path. `0x00416050` maps a character index to a fixed global
configuration record at `0x00568a6c + index * 0x104`. `0x004170d0` clears that
0x104-byte record, copies ten character-specific cost bytes into `+0x38`,
sets `+0x34` to either `1` or `0x3ff` from the character metadata flags, then
invokes both `0x004bbd70(index, 0)` and `0x004bbf00(index, 0)`.

`0x004bbf00` consumes the key stream from the TRICKS section, whose record
shape is independently visible in the extracted bytes:

```text
u16 button_count
u16 button_code[button_count]
u16 source_data_offset
u16 descriptor_flags
```

For each entry it resolves `source_data_offset` through
`0x004bc300` against trick-record `+0x20`. Ordinary entries are then matched
against the built-in button-combination table by `0x004bc7e0`; the resulting
slot and a secondary runtime-record key from `0x004bc330` are written into the
configuration's `+0xcc` assignment map by `0x00416230`. Entries with descriptor
bit `0x8000` use the parallel special-combination table at `0x004bc900` and
the five-slot updater `0x00416380`. Unassigned bytes are initialized to
`0xff`, so the sentinel is part of the runtime contract.

The companion `0x004bbd70` pass walks the same section family and updates the
per-character trick state through `0x00416140`: the helper checks/decrements
the remaining-point field at config `+0x08` when a nonzero cost is supplied,
and the observed database pass invokes it with zero cost to set a bit in the
config unlock bitmap at `+0xac`. The larger menu object constructor at `0x004c0430` also
initializes the trick manager and invokes this database-to-config path for the
active character. This is a proven

```text
TRICKS.BIN source offset 0x325e (Handplant descriptor)
  -> RuntimeTrickRecord { name, +0x20 source, +0x24 flags, +0x26 descriptor }
  -> matching key-stream source-offset lookup when selected for a character
  -> per-character config +0xcc assignment/special slots
```

bridge. It stops at input/configuration state; no physics interpretation of
the resulting trick index is claimed here.

```text
ALL.PKR/data/TRICKS.BIN
  -> 0x00448dd0 raw resource read
  -> 0x00492a90 seven section pointers
  -> 0x004bb4f0 manager validation/table allocation
  -> 0x004bb7e0 command/name decode
  -> 0x004bba50 / 0x004bc300 fixed trick-record database
  -> trick/physics action consumers
```

The first seven offsets are independently confirmed from both the loader's
signed-16 reads and the extracted file bytes. The section roles listed above
are proven at the consumer boundary; the semantic identity of every
intermediate table field and the complete record layout remain open.

## CRETEX.BIN

The extracted `data/CRETEX.BIN` is 20460 bytes with SHA-256
`b31ec233b87cd8910edda07606d96bef0e8294271006e865a1a6df7e6351fa8ec`.
Its payload begins as a sequence of null-terminated names and labels, for
example `afro`, `fceafro1.bmp`, and `Afro guy`; it is text-like metadata rather
than a PSX geometry container.

`0x0041db50` initializes the texture-set manager and invokes
`0x0041dea0("cretex.bin")`. The loader reads the file through
`0x00448dd0`, walks the null-separated record groups, and populates a manager
with room for up to `0x180` texture-set entries. During parsing it calls
`0x0041d860` to resolve names through the texture database and
`0x0041ef50` to commit the assembled set into the runtime manager.

The supported path is therefore:

```text
ALL.PKR/data/CRETEX.BIN
  -> 0x00448dd0 raw resource read
  -> 0x0041dea0 null-separated texture-set parser
  -> 0x0041d860 texture database lookup
  -> 0x0041ef50 runtime texture-set commit
  -> texture-set manager initialized by 0x0041db50
```

The texture database side is also a concrete allocation path. `0x0041d860`
matches a requested name against manager texture records at record `+0x08`;
when absent, `0x0041d9c0` allocates a zeroed 0x1c-byte texture record, copies
seven words from the parsed name/metadata tuple, inserts the pointer in the
manager's sorted record array, and increments its count. `0x0041ef50` then
allocates one 0x44-byte texture-set record, rejects duplicate set IDs, limits a
set to fewer than five texture parts, copies all 17 words, and increments the
set count at manager `+0x6604`. This identifies the runtime ownership units:
individual texture records are 0x1c bytes, while assembled sets are 0x44 bytes.

The individual texture manager prefix is also bounded: its sorted pointer
array starts at `+0x04`, has capacity `0x100`, and its count is at `+0x404`.
`0x0041d9c0` allocates each pointed-to record as seven copied words. The name
used by `0x0041d860` is inline at record `+0x08` and occupies 16 bytes; it is
not a pointer. The parser writes one observed role byte at `+0x18`, while the
other copied words remain raw until a downstream texture consumer identifies
them.

The record-group boundaries and the manager capacity are confirmed. The
commit-side field contract is also narrower than the raw parser makes it look:
`0x0041ef50` reads the set ID at input `+0x00`, rejects an input part count of
five or more at `+0x40`, copies 17 words into the manager's 0x44-byte record,
and increments the manager count. The copy has a stable physical layout:

```text
+0x00         set ID / parser index
+0x04         parser grouping/link word (semantic name open)
+0x08..0x17   first copied label field (16 bytes)
+0x18..0x2f   second copied label field (24 bytes)
+0x30..0x3f   four pointers to resolved texture records
+0x40         texture-part count (u8, must be < 5)
+0x41..0x43   three parser flag bytes
```

The four pointers are populated from the records returned by
`0x0041d860`, so this is a proven disk-metadata -> texture-record ownership
edge even though the public meaning of the two labels and three flags remains
open. `0x0041d860` compares the individual texture name at record `+0x08`;
`0x0041d9c0` allocates and inserts the same 0x1c-byte record in sorted name
order.

The frontend has two independently visible consumers of the set/deck
metadata. `0x004547f0` validates a set and deck index, returns a pointer from
the set/deck table for ordinary entries, and returns static fallback name
pointers for deck values eight and above. `Skater_SetupAppearance` then
changes the third character of the returned `s2d*.bmp` source name to `g`
before the PC bitmap loader runs. This is the concrete CRETEX metadata ->
player bitmap-name handoff.

The create/customization path consumes the committed runtime set records more
directly. `0x004208a0` looks up a set by ID, selects one of the body groups,
and passes the texture records at set `+0x30`, `+0x34`, `+0x38`, and `+0x3c`
to the corresponding appearance/preview helpers. Its group-0, group-1,
group-4, group-5, and group-6 cases also use set flag `+0x41` to decide
whether a secondary texture is present; group 1 additionally checks texture
record `+0x18`, and group 4 derives the inverse of the same set flag into the
preview state. `0x00422d80` invokes this consumer for body groups 1 and 4 and
conditionally group 5. The exact UI names of the groups remain open, but
this proves that the four resolved texture pointers are runtime-selected
appearance inputs rather than dead parser fields.

The next handoff is now field-level. The part-database helpers used by
`0x004208a0` resolve model-part material checksums before the bitmap load:
`0x00422020` selects the fixed part slots (its table uses the strings `head`,
`chest`, `left_bicep`, `left_forearm`, `left_hand`, `left_thigh`, `left_shin`,
and `left_shoe`), while `0x00421910`/`0x00421ba0` search the source-region
part-name table and suppress duplicate checksum matches. The first helper can
select the candidate with the largest or smallest decoded palette area; this
is the same material evidence used by the player path, but here it is driven
by the create body-part slot.

`0x004221a0` then receives the selected material checksum and the inline name
at `RuntimeCreateTextureRecord +0x08`. It calls `0x004674d0(checksum, name)`;
that routine resolves the material, opens the bitmap, validates 4-bit versus
8-bit depth, and enters the common PC texture creation path. On success the
create context records the selected name at `+0x288 + slot*0x10` and stores
the material's palette pointer at `+0x140 + slot*4` through
`0x00422460`. The texture-animation update at `0x004214f0` consumes the same
part slot and palette state. A nonzero create-context word at `+0x04` takes
the streaming/spooler branch instead of the direct bitmap branch, so both
runtime modes are represented at this boundary.

The body-group-to-part-slot map is also explicit in `0x004208a0`:

| create body-group case | source texture-set fields | create part slots | checksum/name selection |
| --- | --- | --- | --- |
| `0` | `+0x30`, optional `+0x34` | `0`, `1` | `head` variants; secondary slot is omitted when set flag `+0x41` is zero |
| `1` | `+0x30`, `+0x34`, optional `+0x38`, `+0x3c` | `2`, `3`, `4`, `5` | `chest`/`left_bicep`/`left_forearm` relationships; texture-record `+0x18` selects the first variant |
| `4` | `+0x30` | `6` | `left_thigh`/`pelvis` |
| `5` | `+0x30` | `7` | `left_shin`/`left_thigh` |
| `6` | `+0x30` | `8` | `left_shoe`/`left_shin`, preview helper |

For cases `0`, `1`, and `4`, the source label at set `+0x08` is independently
looked up in the global part-name table by `0x0041fe50` using table columns
`0`, `1`, and `2`; the resulting entry indices are cached in the create
context at `+0x70`, `+0x74`, and `+0x78`. This is a name-to-model-part link,
not a positional assumption about the four texture pointers.

For the observed body-group calls, the proven path is therefore:

```text
CRETEX set +0x30/+0x34/+0x38/+0x3c
  -> 0x004208a0 body-group selection
  -> 0x00422020 / 0x00421910 / 0x00421ba0 model-part checksum
  -> 0x004221a0 checksum + texture-record name
  -> 0x004674d0 bitmap open/decode
  -> 0x00422460 create-context +0x140 palette slot
  -> create-context +0x288 applied-name slot
  -> 0x004214f0 texture-animation state
  -> appearance/model preview consumers
```

This is the concrete CRETEX disk -> allocated texture record -> selected
runtime appearance slot bridge. It also explains why a set record's four
texture pointers are not themselves the final material handles: the selected
part database maps them into per-slot palette/bitmap state on the create
object.

## Other `.BIN` and adjacent metadata files

The corpus contains several files named `.BIN`, but the extension alone does
not imply one common runtime format:

- The 23 non-`TRICKS.BIN`/`CRETEX.BIN` `.BIN` files in the extracted corpus
  are console/tool blobs or placeholders. Six begin with the little-endian
  MIPS entry bytes `27 bd ff e8`, fifteen begin with the big-endian MIPS
  bytes `00 00 02 3c`, `TEST.BIN` is empty, and `XXXX.BIN` has an
  unidentified non-MIPS header. A generic relocatable-module helper does
  exist in the executable, but no extracted `.REL` pair or direct PC caller
  was proven for these names, so they are not claimed as PC runtime assets.
- The helper around `0x004ac0c0` computes a CRC key for a requested module
  name, constructs `<name>.bin` and `<name>.rel`, loads both through the raw
  resource reader `0x00448dd0`, relocates the `.rel` records into the `.bin`
  image, and calls the image's first function pointer. This is a real loader
  boundary, but it is not enough to assign the extracted MIPS modules to the
  PC runtime: the local `LEVELSEL`/`EQUIPSEL` pairs are big-endian MIPS code,
  no direct PC caller was recovered, and their runtime execution was not
  observed.
- `SFX` files are small fixed-record metadata files. The PC sound path instead
  builds `audio/<name>.wav` from hardcoded sound-description tables; no `.SFX`
  open or filename cross-reference was found. They remain legacy/input
  metadata until a producer or consumer is located.
- `REC` is a separate, proven replay/card family documented in
  [replay-runtime.md](replay-runtime.md). `SEQ`, `SBL`, `TST`, and similar
  files have corpus signatures, but their PC disk-to-runtime consumers are not
  yet proven. They should not be silently mapped onto the PKR/PSX loader.

## Confidence boundary

- `confirmed`: both raw reads; `TRICKS.BIN` seven signed-16 section offsets,
  variable descriptor boundaries, type-to-table-class handoff, command/name
  decode including prefix cleanup, flag propagation, 0x28-byte trick-record
  allocation/lookup, and the first
  `Handplant` disk-to-record values; the trick-manager consumer, including its
  `+0x04` record array, `+0x1404` count, and `+0x1408` raw resource pointer;
  the `0x104`-byte character trick configuration stride and its
  TRICKS.BIN-to-assignment-map consumer; input-history/queued-start state,
  script cursor execution, point-stack mutation, landed-total/best-score
  commit, fixed-point score arithmetic, and the score/UI handoff;
  `CRETEX.BIN` parser, 0x1c-byte texture allocation/lookup, inline texture
  name at record `+0x08`, sorted texture-pointer array/count, 0x44-byte set
  commit, set ID at `+0x00`, four resolved texture pointers at `+0x30`,
  part-count input at `+0x40`, the set-record consumer at
  `0x004208a0`/`0x00422d80`, body-part checksum selection through
  `0x00422020`/`0x00421910`/`0x00421ba0`, create-context texture-name and
  palette publication at `+0x288`/`+0x140`, and limits; extracted sizes and
  hashes.
- `observed`: fixed manager capacities and the version-selected trick table
  sizes, plus the remaining copied CRETEX label/flag words.
- `observed`: generic relocatable `.bin`/`.rel` construction, relocation, and
  entrypoint call at `0x004ac0c0`/`0x004ac2e0`.
- `open`: the original gameplay names and full physics consequences of the
  recovered trick-script state/helper calls, the semantic meaning of remaining
  CRETEX label/flag words beyond the observed secondary-texture behavior, a
  proven PC caller for the MIPS-style
  `.BIN`/`.REL` modules, and direct `.SFX` loading.
