# PC binary-asset runtime paths

Status: confirmed for `TRICKS.BIN` and `CRETEX.BIN`; a generic relocatable
module helper is observed, but the extracted module pairs remain unproven or
belong to the legacy/console side of the asset set
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0041db50`, `0x0041d860`, `0x0041dea0`, `0x0041ef50`,
`0x00492a90`, `0x004bb4f0`, `0x004bb7e0`, `0x004bba50`, `0x004bc300`

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

`0x004bb4f0` is the downstream manager constructor/initializer. It calls the
loader, stores the successful result at an object field near `+0x1408`, and
validates relationships between the section pointers. It then reads a version
word from one section and selects one of several fixed runtime table sizes
(`0x200`, `0x100`, `0x800`, or `0x2000` in the observed branches). That gives a
concrete consumer of the loaded binary even though the individual trick
record fields are not yet fully named.

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
flag propagation, and disk-to-runtime name/source correspondence are proven;
the downstream trick physics fields remain open.

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
signed-16 reads and the extracted file bytes. The semantic identity of every
section and the complete record layout remain open.

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

The record-group boundaries and the manager capacity are confirmed. The
commit-side field contract is also narrower than the raw parser makes it look:
`0x0041ef50` reads the set ID at input `+0x00`, rejects an input part count of
five or more at `+0x40`, copies 17 words into the manager's 0x44-byte record,
and increments the manager count. `0x0041d860` compares the individual
texture name at texture-record `+0x08`; `0x0041d9c0` allocates and inserts the
same 0x1c-byte record in sorted name order. The remaining label fields and
the four-part array's semantic names are not yet sufficiently cross-checked to
publish.

The player frontend has an independently proven consumer of the set/deck
metadata at `0x004547f0`. It validates a set and deck index, returns a pointer
from the set/deck table for ordinary entries, and returns static fallback name
pointers for deck values eight and above. `Skater_SetupAppearance` then changes
the third character of the returned `s2d*.bmp` source name to `g` before the PC
bitmap loader runs. This is the concrete CRETEX metadata -> player bitmap-name
handoff; the full meaning of the other set-record words remains open.

## Other `.BIN` and adjacent metadata files

The corpus contains several files named `.BIN`, but the extension alone does
not imply one common runtime format:

- `MAINMENU.BIN`, `EDMOD.BIN`, and `CREATESELECT.BIN` begin with MIPS-style
  executable/data bytes. A generic relocatable-module helper does exist in
  the executable, but no PC caller was proven for these names, so they are
  not claimed as PC runtime assets.
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
- `REC`, `SEQ`, `SBL`, `TST`, and similar files have corpus signatures, but
  their PC disk-to-runtime consumers are not yet proven. They should not be
  silently mapped onto the PKR/PSX loader.

## Confidence boundary

- `confirmed`: both raw reads; `TRICKS.BIN` seven signed-16 section offsets,
  variable descriptor boundaries, type-to-table-class handoff, command/name
  decode including prefix cleanup, flag propagation, 0x28-byte trick-record
  allocation/lookup, and the first
  `Handplant` disk-to-record values; the trick-manager consumer; `CRETEX.BIN`
  parser, 0x1c-byte texture allocation/lookup, texture name at record `+0x08`,
  0x44-byte set commit, set ID at `+0x00`, part-count input at `+0x40`, and
  limits; extracted sizes and hashes.
- `observed`: fixed manager capacities and the version-selected trick table
  sizes.
- `observed`: generic relocatable `.bin`/`.rel` construction, relocation, and
  entrypoint call at `0x004ac0c0`/`0x004ac2e0`.
- `open`: complete trick command/physics fields, complete texture-set records,
  a proven PC caller for the MIPS-style `.BIN`/`.REL` modules, and direct
  `.SFX` loading.
