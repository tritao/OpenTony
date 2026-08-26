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

Each glyph object is allocated through the normal game heap and receives the
decoded colour data and a texture/image setup call. The extra entry is a
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
