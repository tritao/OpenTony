# Text metadata and credits runtime paths

Status: confirmed `CDPARKS.TXT` label-table load and credits/music text parse
into runtime record objects
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004329d0`, `0x00433160`, `0x00426430`, `0x004266e0`,
`0x00426c00`, `0x00426c80`, `0x00426e60`, `0x00426f50`, `0x00427080`,
`0x00426b50`, `0x00426bc0`, `0x00472ef0`

The `.TXT` family is not uniformly legacy. Three files have direct PC
runtime consumers: `CDPARKS.TXT` supplies a fixed park-label table, while
`MUSIC.TXT` and `CREDITS.TXT` feed the credits/music presentation system.

## CDPARKS label table

`CDPARKS.TXT` is 875 bytes with SHA-256
`84d1947965ac2dfd7cbf753d2379d0563c67702c0ebd5005c3bb193911711c95`.
It contains exactly 50 CRLF records of the form:

```text
numeric index<TAB>$display label<CR><LF>
```

For example, the first record is `0\\t$Up. Down. Repeat.`. The loader
`0x00433160` performs the complete game-owned path:

```text
CDPARKS.TXT
  -> 0x00449030 open
  -> 0x0046f490 allocate/read wrapper
  -> 0x00449230 + 0x00449660 synchronization
  -> scan each $, store pointer after $, replace CR with NUL
```

The source buffer is retained at owner `+0x78`; the 50 borrowed label pointers
are stored at `+0x7c` with four-byte stride. The numeric prefix and tab are not
copied into the runtime labels. There is no per-line allocation.

`0x004329d0` is the consumer-side constructor for the corresponding park menu
mode. It initializes a 0..49 item-index array, invokes the loader, allocates a
menu wrapper through `0x00472ef0`, supplies the 50-item count and the
`ingame1.fnt` font, and passes the label-pointer table. The menu wrapper
copies font metrics, allocates one menu item object per index, and stores each
created item in its own pointer table. This proves the text-to-runtime-object
handoff; the later `.PRK` geometry path is separate from these display labels.

## MUSIC.TXT and CREDITS.TXT

`MUSIC.TXT` is 5,887 bytes in the extracted corpus with SHA-256
`8b30800fbe9af7ee7993bf84c612dfc2d648fcf94ad643df950072806053021e5`;
`CREDITS.TXT` is 8,921 bytes with SHA-256
`72571b318bbf761c16377923dbae3764f7a6707dee74f155ff9661b6af0d0bcc`.
The parser's record walk independently produces 401 records from `MUSIC.TXT`
(371 ordinary lines, 15 bitmap directives, and 15 `@M` markers) and 716 from
`CREDITS.TXT` (697 ordinary lines, 18 bitmap directives, and one `@M` marker).
Those counts stop at each file's `#` record terminator.

`0x00426430` selects `credits.txt` for mode 0 or `music.txt` for mode 1,
loads `s2font1.fnt`/`s2font2.fnt` and the credit bitmap resources, then calls
`0x004266e0`. That parser opens the selected text with `0x00449030`, allocates
and reads it through `0x0046f490`, and walks up to 1000 records until the `#`
terminator.

The line parser at `0x00426c00` copies each ordinary line into a 0x31-byte
working buffer at manager `+0xfb0`. It recognizes these control records. The
tag scanner is exact about both case and payload boundaries:

```text
@B / @b + 3 bytes  -> record type 1; the line payload begins after the tag
@F / @f + 4 bytes  -> keep text-record type 0 and switch to the alternate font source
@M / @m + 5 bytes  -> record type 2; digits at tag offsets 2 and 4
```

At the start of each record, `0x00426c00` resets the type to 0 and the active
source pointer to the s2font2-derived descriptor at `0x00560d08`.
`@F` switches that pointer to the s2font1-derived descriptor at
`0x00560e58`; `@B` changes only the type and leaves the line payload for the
bitmap constructor. The parser stores the active type at `+0xfe4`, the active
source pointer at `+0xfe8`, and the two `@M` digits at `+0xfec`/`+0xff0`. A
`#` at the start of the next record terminates the stream.

For each parsed record, `0x004266e0` allocates and publishes one runtime
object through the manager's pointer array at `manager + 0x04`. The observed
record families are:

```text
type 0 -> 0x44-byte record; text copied at +0x0c..+0x3f, metric/size at +0x08, active font source at +0x40
type 1 -> 0x10-byte record; image object at +0x0c, metric/size at +0x08
type 2 -> 0x18-byte record; @M parameters at +0x0c/+0x10, +0x14 cleared
```

The type-1 path is a direct image handoff. For a source line such as the
first `CREDITS.TXT` record `@B thcredit.bmp`, the parser skips the three-byte
`@B ` prefix, passes the remaining `thcredit.bmp` line to
`0x00426f50`, and that constructor calls the normal bitmap loader
`0x00457420`. The returned image object is retained at record `+0x0c`; the
temporary line buffer is not the runtime image storage.

The type-0 path passes both the copied text line and the active font-source
descriptor to `0x00426e60`, which stores the source pointer at record `+0x40`.
Thus `@F` is a font-selection modifier for the following text record, while
`@B` is a bitmap record with its own image object. This resolves the formerly
open `@F`/`@B` payload ownership without assigning names to the lower font or
image class internals.

The manager publishes the record count at `+0xfa4`, accumulated stream size at
`+0xfa8`, a current offset at `+0x00`, and a timing origin at `+0xfac`.
The source text buffer is freed after parsing. `0x00426b50` advances the
current offset from the frame clock, and `0x00426bc0` calls every record's
virtual method at vtable offset `+0x04` with `current_offset >> 2`. That is
the downstream animation/render/update consumer for the parsed text records.

The direct runtime paths are therefore:

```text
CDPARKS.TXT -> file buffer + 50 borrowed labels -> park menu items

MUSIC.TXT/CREDITS.TXT
  -> file buffer -> tag/line parser
  -> 0x44/0x10/0x18-byte runtime record objects
  -> frame-clock offset -> per-record virtual update/render calls
```

The exact meaning of all credits record vtable methods remains open. The
`@F`/`@B` payload ownership is resolved above: `@F` selects the active font
source for a text record, while `@B` creates a bitmap record retaining the
image object returned by the bitmap loader. These files should not be grouped
with the unreferenced tool/debug text corpus.

The native counterparts in `src/assets/text_asset.*` preserve the same
ownership boundary: `ParkLabelTable` materializes the 50 `$`-prefixed labels,
while `PresentationTextAsset` parses the bounded text/bitmap/marker record
stream and owns each line after the source buffer can be released. Real
`CDPARKS.TXT`, `CREDITS.TXT`, and `MUSIC.TXT` counts are covered by the native
fixture.
