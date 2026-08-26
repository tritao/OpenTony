# Career save image and runtime career state

Status: confirmed career `.SAV` buffer ownership, load/save producer chain,
fixed per-skater records, selector handoff, and custom-appearance copy
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004138f0`, `0x00413c10`, `0x00413d60`, `0x00413ed0`,
`0x00416050`, `0x004165d0`, `0x004166f0`, `0x004170d0`, `0x004171e0`,
`0x00417260`, `0x00417300`, `0x00417330`, `0x0047a030`, `0x0047a0b0`,
`0x0047a6c0`, `0x0047ae10`, `0x004bbd70`, `0x004bbf00`

This closes the career payload side of the PC save path. The MMU evidence in
[save-runtime.md](save-runtime.md) proves that career saves use a registered
`0x1e00`-byte buffer. The career routines below prove what part of that buffer
is copied into gameplay state, how it is prepared before saving, and which
fixed records are contained in it. The remaining bytes are intentionally kept
raw where no field consumer has been independently recovered.

The corresponding schema is recorded in
[`career-runtime.yml`](../../types/career-runtime.yml).

## Disk-to-runtime load

The selected `.SAV` file is read by `0x00476200` into the buffer allocated by
`0x0047a6c0`:

```text
THPS2_*.SAV
  -> 0x00476200 MMU read, payload offset 0x200
  -> 0x0056ad04, allocated size 0x1e00
  -> 0x0047ae10(1)
       memcpy(0x0056ad04, 0x00568a68, 0x1dc4)
  -> 0x004165d0 career validation/application
```

`0x0047ae10` copies exactly `0x1dc4` bytes, not the registered `0x1e00`
bytes. The last `0x3c` bytes of the transfer allocation are therefore not
part of the proven career image. The copy destination is the contiguous
career image beginning at `0x00568a68`.

`0x004165d0` first requires the image word at `+0x00` to equal `0x101500`.
The other paths print an old-format or corrupt-file diagnostic and call
`0x004138f0` to rebuild initial career state. For the current format it:

1. reads two skater selector bytes at image `+0x1870` and two costume selector
   bytes at `+0x1872`;
2. calls the skater/costume selection helpers `0x004b0e60` and `0x004b0fe0`;
3. calls `0x00413d60` to copy serialized scalar/table fields into the live
   career globals;
4. calls `0x00417330` to restore static skater-selection defaults and reset
   runtime career flags; and
5. calls `0x00417060` to apply the currently enabled career/cheat flags to
   the selection records.

This is a real consumer chain: the file bytes are copied into the career
image, selector values choose runtime skater resources, and the image's
per-skater records become the input to the trick/configuration setup. It is
not merely a save-screen buffer.

## Save producer

`0x0047a0b0` is the career save setup. It calls `0x004166f0`, then registers
`&0x00568a68` with `0x0047a030` at size `0x1e00`:

```text
live career globals
  -> 0x004166f0 Career_PrepareSave
       0x00413c10 copy live fields into the image
       0x004b18a0 / 0x004b0cd0 capture two skater/costume selectors
  -> 0x00568a68, 0x1e00
  -> 0x0047a030 registration
  -> 0x004758e0 SC header + payload write
```

`0x00413c10` is the inverse of `0x00413d60` for the scalar/table fields. It
also rebuilds pointer views through `0x00413ed0` and copies the two player
selection records into the image. `0x004166f0` runs this copy first, then
overwrites the four selector bytes with the values returned by the active
skater and costume helpers. That ordering is why the selector bytes in a
career save represent the current frontend/gameplay selection rather than a
stale shadow copy.

## Career image layout

The image begins at `0x00568a68`. `0x004138f0` clears `0x771` dwords
(`0x1dc4` bytes), writes the format word, initializes the 20 static skater
rows, and preserves two additional blocks that live inside the same image.
The supported layout is:

```text
image +0x0000                         u32 format magic, 0x101500
image +0x0004 + index * 0x0104       RuntimeCareerSkaterRecord[20]
image +0x1870                         two active-skater selector bytes
image +0x1872                         two active-costume selector bytes
image +0x1874                         two copied secondary-selection bytes
image +0x1878                         two copied secondary-selection flags
image +0x1880                         two copied career scalar words
image +0x1950                         sixteen copied career/profile words
image +0x1970                         four raw local-profile words
image +0x1a04                         two copied career scalar words
image +0x1a0c                         bounded-text length/count word
image +0x1a10                         bounded text area, 0x0b bytes
image +0x1a20                         preserved raw block, 0x200 bytes
image +0x1c20                         two copied secondary-selection bytes
image +0x1c64                         preserved raw block, 0x160 bytes
image +0x1dc4                         end of the copied image prefix
image +0x1dc4..+0x1dff              registered-buffer tail, not copied by load
```

The scalar and block rows are deliberately descriptive rather than semantic:
`0x00413c10` and `0x00413d60` establish the exact copy pairs, but the PC
image does not independently assign public gameplay names to every source
global. The exact destination offsets and directions are:

| Image offset | Size | Save source / load destination | Evidence |
| --- | ---: | --- | --- |
| `+0x1870` | 2 bytes | `0x0055faf8` / `0x0055faf8` | `0x00413c10`, `0x00413d60`, `0x004166f0` |
| `+0x1872` | 2 bytes | direct active-costume capture / consumed by load application | `0x004165d0`, `0x004166f0` |
| `+0x1874` | 2 bytes | `0x0055fb36` / `0x0055fb36` | `0x00413c10`, `0x00413d60` |
| `+0x1878` | 2 bytes | `0x0055fb3a` / `0x0055fb3a` | `0x00413c10`, `0x00413d60` |
| `+0x1880` | 4 bytes | `0x0055fb3c` / `0x0055fb3c` | `0x00413c10`, `0x00413d60` |
| `+0x1884` | 4 bytes | `0x0055fb40` / `0x0055fb40` | `0x00413c10`, `0x00413d60` |
| `+0x1950..+0x198f` | 0x40 | `0x0055fc0c` / `0x0055fc0c` | `0x00413c10`, `0x00413d60` |
| `+0x1970..+0x197f` | 0x10 | `0x0055fc2c` and `0x0055fc34` / inverse | `0x00413c10`, `0x00413d60` |
| `+0x1a04` | 4 bytes | `0x0055fcc0` / `0x0055fcc0` | `0x00413c10`, `0x00413d60` |
| `+0x1a08` | 4 bytes | `0x0055fcc4` / `0x0055fcc4` | `0x00413c10`, `0x00413d60` |
| `+0x1a0c` | 4 bytes | `0x0055fcc8` / `0x0055fcc8` | `0x00413c10`, `0x00413d60` |
| `+0x1a10` | 0x0b bytes | `0x0055fccc` / `0x0055fccc` | `0x00413c10`, `0x00413d60` |
| `+0x1a20` | 0x200 | preserved in image reset | `0x004138f0` |
| `+0x1c20` | 2 bytes | `0x0055fb38` / `0x0055fb38` | `0x00413c10`, `0x00413d60` |
| `+0x1c64` | 0x160 | preserved in image reset | `0x004138f0` |

The `+0x1950` row and the `+0x1970` row overlap in the table because the
second is a separately used subrange of the copied profile area. They are
listed separately to retain the two independent source-copy contracts.

## Fixed per-skater records

`0x00416050(index)` returns:

```text
0x00568a6c + index * 0x104
```

The initializer at `0x004170d0` uses indices `0..19`, clears 0x104 bytes,
copies ten static metadata bytes to `record +0x38`, sets `record +0x34` to
`1` for ordinary rows or `0x3ff` for enabled custom rows, and then invokes
`0x004bbd70` and `0x004bbf00`. The latter two routines consume the TRICKS
key sections and write the record's unlock and button-assignment state.

The evidence-backed record view is:

| Record offset | Size | Field | Support |
| --- | ---: | --- | --- |
| `+0x00..+0x33` | 0x34 | zeroed/raw career state | cleared by `0x004170d0`; subfields not named |
| `+0x34` | 4 | trick/key unlock bit mask | initialized to `1`/`0x3ff`, tested and updated by trick-config routines |
| `+0x38` | 10 | raw trick/stat seed bytes | copied from static skater metadata and adjusted for custom rows |
| `+0x42..+0xcb` | 0x8a | raw career state | no independent field boundary recovered |
| `+0xcc` | 0x35 | custom appearance blob | copied by `0x004171e0` and `0x00417260` for custom rows |
| `+0x101..+0x103` | 3 | trailing raw bytes | within the fixed 0x104-byte stride; no consumer named |

The custom appearance edge is exact. `0x004171e0(slot)` maps a frontend
custom slot through `0x004b1980`, obtains its career record with
`0x00416050`, and copies 0x35 bytes from `record +0xcc` into the image-side
custom staging table at `0x0056a6d0 + slot * 0x58`. `0x00417260(slot)` performs
the inverse copy when the custom row is active. This gives a concrete
career-image record ↔ runtime selection-record correspondence even though the
individual appearance fields remain packed.

## Runtime consumers and limits

The recovered consumer sequence is:

```text
career image record +0x34/+0x38/+0xcc
  -> 0x004170d0 / 0x004bbd70 / 0x004bbf00
  -> character trick/configuration state
  -> skater selection/model setup
  -> player runtime object and gameplay consumers
```

The selector bytes are independently consumed before the serialized career
fields are applied, and the custom blob is independently copied into and out
of the selected skater record. That is sufficient to call this a proven
disk-to-runtime career-object path.

Still open are the public meanings of the scalar words, the complete internal
meaning of the 0x34-byte pre-mask record prefix, and the fields in the 0x35-
byte appearance blob. They should remain raw in a faithful recreation until
their UI/model consumers or controlled save differences provide independent
field boundaries.

## Confidence

- `confirmed`: `.SAV` load/save buffer size, `0x1dc4` copied career image,
  format magic, 20-row/0x104-stride record array, selector offsets, record
  mask/seed/custom-blob offsets, and the inverse save/load copy directions.
- `observed`: the exact runtime use of each scalar/profile source global and
  the preservation role of the two non-record blocks.
- `open`: semantic names for the remaining raw record/image fields and the
  trailing 0x3c registered-buffer bytes.
