# FNT font disk and runtime path

Status: confirmed file read, font record construction, slot lookup, and text
consumer
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x0044ada0`, `0x0044aea0`, `0x00449cd0`, `0x0044b000`,
`0x0044b0a0`, `0x0046a770`, `0x0046a8d0`

The executable carries 26 `.FNT` assets. `S2TRICKS.FNT` is 9104 bytes, with
SHA-256 `e4193af0c8136f01f3f9eb3145bb90239fde3f77958b9f10b4e816734330ba73`.

## Disk format and allocation

`0x0044ada0` is the game-owned loader. It obtains the file size through
`0x00449030`, rejects files at or below `0x28` bytes, allocates an exact-size
buffer, reads/synchronizes it with `0x00449230`/`0x00449660`, and passes the
buffer to `0x0044aea0`. The loader then frees the temporary file buffer.

`0x00449cd0` constructs one runtime font object in a `0x14c` allocation. The
first file word is a glyph/record count. The parser consumes a 16-byte record
for each entry, followed by a 0x20-byte 16-colour table and packed glyph data.
The exact glyph-record semantic names are not assigned, but the width and
pointer behavior are stable:

```text
FNT file
  +0x00 u32 record_count
  +0x04 record_count * 0x10-byte records
  +...  0x20-byte colour table
  +...  packed glyph data

runtime font +0x34 -> (record_count + 1) entries, stride 0x08
  entry +0x00 -> 0x5c-byte glyph/image runtime object
  entry +0x04/+0x05/+0x06 -> compact source/atlas bytes
runtime font +0x38 -> record_count
runtime font +0x3c -> packed atlas dimensions/format word
```

For the `S2TRICKS.FNT` witness, the count is 59, the record table occupies
`0x04..0x3b3`, the 0x20-byte colour table begins at `0x3b4`, and packed glyph
data begins at `0x3d4`. Record 0 is `(5, 14, 14, 19)` as four little-endian
words. The runtime entry stores the low bytes of record `+0x0c`, `+0x04`, and
`+0x08` at entry `+0x04`, `+0x05`, and `+0x06`; the full record `+0x00` is
used with entry `+0x05` to advance the packed-data cursor, including the
observed even-byte alignment. This is a direct file-offset-to-entry witness,
while the editor's original glyph metric names remain open.

Each glyph object is allocated through the normal game heap as a 0x5c-byte
image object. Its setup writes the decoded image width/height, bpp mode, and
atlas-format word, then creates the associated PC texture record through the
same image/texture helpers used by the bitmap path. The extra entry is a
sentinel/blank glyph built from the trailing header values. The font manager
has eight slots: `0x0044aea0` stores the runtime pointer in
`DAT_0056161c[slot]` and the name in the 0x10-byte name table beginning at
`DAT_0056163c`.

## Lookup and consumer

`0x0044b000(name)` performs the bounded eight-slot name lookup and returns the
runtime font pointer. `0x0044b0a0(name)` performs the same lookup, releases
the font's image/texture objects, and clears the slot. This makes the slot
table and lifetime contract explicit rather than treating `.FNT` as a static
blob.

The front-end/game consumer at `0x0046a770` loads `s2tricks.fnt`, looks it up,
and copies the runtime record fields at `+0x04` through `+0x4a` into the text
rendering globals (`DAT_005619e0` onward), including packed dimensions and the
glyph table. `0x0046a8d0` repeats the same load/lookup/copy path after a level
load. Other menu and HUD code uses the same `Font_Find` boundary.

The glyph image prefix used by this path is also bounded: `0x00456d10`
allocates/initializes a 0x5c-byte object, `0x00456e10` writes image width at
`+0x40`, image height at `+0x42`, slice dimensions at `+0x4e/+0x50`, maps
4/8/16-bpp input to the image bpp mode at `+0x58`, and stores the font's
packed atlas word at `+0x5a`. The texture decoder at `0x004d98d0` receives the
glyph data and its entry width/height before the image object is built. This
proves the FNT glyph -> image -> PC texture ownership edge without assigning
the Direct3D texture implementation's internal fields.

The proven path is therefore:

```text
S2TRICKS.FNT / menu FNT
  -> 0x0044ada0 file open/read
  -> 0x0044aea0 / 0x00449cd0 runtime font + glyph objects
  -> DAT_0056161c[slot], DAT_0056163c names
  -> 0x0044b000 lookup
  -> text-rendering dimensions/glyph consumer
```

Limits: the exact palette upload and per-glyph texture coordinate semantics
remain shared graphics-library details; the asset loader, runtime allocation,
slot ownership, and consumer boundary are independently supported.
