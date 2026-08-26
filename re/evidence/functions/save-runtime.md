# Windows save-game/MMU runtime path

Status: confirmed Windows save-file discovery, naming, block validation, and
career/replay/custom-park read/write handoffs
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x00473d20`, `0x00474010`, `0x00474100`, `0x00474250`,
`0x004742e0`, `0x00474380`, `0x004744a0`, `0x004745d0`, `0x004746d0`,
`0x004747d0`, `0x00474d70`, `0x00475480`, `0x00475620`, `0x00475640`,
`0x00475660`, `0x00475700`, `0x00475840`, `0x004758e0`, `0x00476200`,
`0x004780d0`, `0x0047a030`, `0x0047a0b0`, `0x0047a220`, `0x0047a480`,
`0x0047a6c0`, `0x0047a8a0`, `0x0047aa50`

This is the executable's PC save-data family. It is not part of `ALL.PKR`
and must not be routed through the PSX scene loader or the replay `.REC`
parser. The implementation is an MMU/memory-card compatibility layer whose
user-visible files use the `.SAV` suffix.

## File discovery and record table

The card scan at `0x00474d70` calls the file backend's directory enumerator
with the literal `*.SAV`, stores up to the manager's discovered file records,
and rejects file sizes that are not multiples of 8192 bytes. The manager
record layout supported by the callers is:

```text
manager +0x04 + index * 0x94   file/path record
manager +0x88 + index * 0x94   file size in bytes
manager +0x8b0 + index * 0x04   runtime file type
manager +0x928 + index * 0x21   display/user name
manager +0xb18                 file count
manager +0xb1c                 card-ready/state word
manager +0xb20                 free 8192-byte blocks
manager +0xb28                 save-buffer count
manager +0xb2c + index * 4     save-buffer pointer, up to four
manager +0xb3c + index * 4     save-buffer size, up to four
```

The scan computes free blocks as `15 - ceil(total_file_bytes / 8192)` and
classifies headers by the fixed strings `THPS2_CAREER`, `THPS2_REPLAY`, and
`THPS2_PARK`. Type values are stable at the runtime boundary: `1` is career,
`2` is replay, and `3` is custom park. The type-specific display labels are
also fixed by `0x0047b8e0`:

```text
1 -> THPS2_CAREER
2 -> THPS2_REPLAY
3 -> THPS2_PARK
```

`0x00473f30` selects the oldest eligible record using the file-record helper;
the selection policy is kept behind that helper because the timestamp/age
field is not independently named in the PC image.

The manager exposes case-insensitive name queries at `0x00474380` and
`0x004744a0`, indexed record access at `0x00474250`, the stored display name
at `0x004742e0`, and the type-specific description at `0x0047b8e0`. The
delete path at `0x00475660` passes the selected record to the backend delete
operation and returns its success status.

## Type-specific buffer registration

The generic four-slot registration method is wrapped by a set of type-specific
setup functions. These callers prove which runtime buffers reach the MMU
layer:

```text
0x0047a0b0  type 1 save   -> allocated career buffer, 0x1e00 bytes
0x0047a6c0  type 1 load   -> allocated career buffer, 0x1e00 bytes
0x0047a220  type 2 save   -> replay stream 0, optional stream 1,
                             each registered as 0x7fe00 bytes
0x0047a8a0  type 2 load   -> allocated replay/video buffer, 0x280 bytes
0x0047a480  type 3 save   -> caller-provided custom-park buffer, aligned size
0x0047aa50  type 3 load   -> caller-provided custom-park buffer, aligned size
```

`0x0047a030` is the thin wrapper that forwards each pointer/size pair to
`0x00475840`. The type-2 save setup validates one or two players, serializes
each active replay header through `0x004cb810`, and registers the resulting
fixed-size stream chunks. The type-3 save setup validates the custom-park
action, rounds the caller size up to 128 bytes, and checks the resulting
header-plus-payload range against the card capacity. The corresponding type-1
and type-3 load setups register the destinations before `0x00476200` reads
from offset `0x200`.

The `0x280` type-2 load buffer is recorded only as a transfer-size contract;
the surrounding video/replay state owns its internal fields. The career,
replay, and custom-park payloads therefore have distinct producer/consumer
edges even though they share the same MMU registration and file header.

## Save-file naming

`0x004747d0` constructs the user-file name before the card operation. It
creates a four-character token from the caller's first and last characters
with two random uppercase letters in between, prefixes it with `THPS2_`,
selects a type suffix (`G` for type 1, `V` for type 2, `P` for type 3), and
appends `.SAV`:

```text
THPS2_<first><random-uppercase><random-uppercase><last><G|V|P>.SAV
```

The generated token is bounded by the caller's supplied name buffer. The
random source is only a collision-avoidance/name-generation detail; it is not
part of the save payload.

## Header and block ABI

When no existing user file is selected, `0x004758e0` builds the first 512
bytes through `0x00475700`. The header starts with the ASCII bytes `SC`, a
third byte equal to `0x10 + action_type`, and the action type byte. It clears
the following reserved area, copies a 32-byte user name at `+0x60`, and
copies three 128-byte blocks at `+0x80`, `+0x100`, and `+0x180`. For action
type 2 or 3 the latter blocks may come from separate caller-provided buffers;
the header builder itself does not interpret their gameplay fields.

The save manager then requires the header size and every buffer size to be
128-byte aligned. The card file's data offset begins at `0x200`; successive
save buffers are written at the accumulated offset, with each buffer size
also required to be a multiple of 128. The total payload must fit the
available card-block count, and each header/data range is sent to the backend
write helper at `0x004e90d0`. The unrelated resource path helper at
`0x004e7900` is not part of this save-file write edge.

The save operation is therefore:

```text
runtime career/replay/park state
    -> up to four registered save buffers
    -> 0x00475700 512-byte SC/type/name header
    -> 8192-byte card capacity and 128-byte alignment checks
    -> THPS2_*.SAV through the resource writer
```

For the concrete runtime producers, the common edge expands to:

```text
career state       -> 0x0047a0b0 -> 0x1e00 buffer -> type 1
replay state       -> 0x0047a220 -> one/two 0x7fe00 streams -> type 2
custom-park state  -> 0x0047a480 -> caller buffer -> type 3
```

The career payload is now resolved beyond this MMU boundary: see
[career-save-runtime.md](career-save-runtime.md) for the `0x1dc4` copied
career image, its 20 fixed `0x104`-byte skater records, selector bytes, and
load/save producer-consumer chain.

`0x00476200` is the matching selected-file load path. After the scan has
validated the selected record and its 512-byte header, it validates each
registered buffer's size and the current offset, reads the data blocks from
offset `0x200` into the caller-owned buffers, and requires the backend
operation to report the complete result. The scan also validates replay files
against three or six 8192-byte blocks. Career and custom-park loads update the
global current-file name before exposing the loaded buffers.

`0x00475480` is the status/result-code adapter used by the save UI. It maps
the card backend result codes `0..4` to the manager's raw state values
`0, 1, 3, 4, 2`; result code `3` also clears the card error globals and marks
the returned action as requiring a follow-up step. The exact Windows UI
message text is not part of the file ABI.

## Runtime consumers

The save-screen state machine at `0x004780d0` calls the MMU methods for
format, delete, save, and load operations. It uses the runtime type values
above to select the career, replay, or custom-park path, reports success or
failure through the frontend message state, and returns to the idle state
after the backend operation completes. `0x00473d20` rejects unsupported
action types and specifically rejects type 2 save requests outside the
supported mode.

This closes the executable-referenced `.SAV` family as runtime state storage:

```text
career / replay / custom-park runtime state
    -> MMU manager buffers and SC header
    -> THPS2_*.SAV file
    -> MMU scan/load
    -> selected frontend/runtime state
```

It does not claim that the card header's reserved bytes or the individual
replay/park payload schemas are interchangeable. Their producers and
consumers register different buffer sets. The career payload's supported
schema is recorded separately in [career-save-runtime.md](career-save-runtime.md);
replay and custom-park payload fields remain separate reverse targets.

## Confidence and limits

- `confirmed`: `.SAV` discovery, manager record offsets/strides used by the
  callers, type labels, filename construction, 8192-byte file/card rules,
  512-byte header construction, 128-byte buffer alignment, four save-buffer
  slots, type-specific career/replay/custom-park buffer registration, common
  writer/read/delete handoffs, and the save UI state-machine calls.
- `observed`: backend result-code mapping and the type-specific block-count
  validation in the load path.
- `inferred`: the public MMU class name, the age field used by oldest-file
  selection, and the internal gameplay schema of the career/replay/park data
  buffers.

## Native recreation boundary

`src/assets/save_asset.*` models the common `SC` file image: its type-bearing
first four bytes, 32-byte display name, three 0x80-byte header blocks, 0x200
payload origin, 0x80 alignment, and 0x2000 card-block rule. `CareerSaveImage`
then copies the first `0x1dc4` bytes of a registered `0x1e00` career buffer,
exposes the twenty `0x104`-byte records, selector bytes, the `+0x34` unlock
mask, `+0x38` seed bytes, and `+0xcc` appearance blob. `save_runtime.*` adds
the executable-facing manager state: validated `.SAV` discovery records,
case-insensitive lookup, free-card-block accounting, four aligned buffer
slots, type-specific career/replay/custom-park registration, the common
payload concatenation, and the observed filename/type-label helpers. Native
tests cover build, parse, career-image handoff, and manager registration.
