# Legacy and unconnected asset families

Status: corpus signatures recorded; no PC disk-to-runtime consumer proven for
these families
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

The extracted archive contains several one-off or console/toolchain formats
that are easy to overinterpret because they have structured-looking headers.
The executable was searched for case-insensitive filename strings, literal
magic strings, and direct references to the extracted names. None of the
families below produced a PC loader boundary comparable to PRE, PSX, TRG, FNT,
WAV, replay, `TRICKS.BIN`, or `CRETEX.BIN`.

The `.TXT` extension is split rather than uniformly legacy. `CDPARKS.TXT`,
`MUSIC.TXT`, and `CREDITS.TXT` have direct PC consumers and are documented in
[text-runtime.md](text-runtime.md). The remaining extracted text files are
tool/debug or notes data; no runtime filename/open boundary was found for
them.

The 19 unconnected text files are `1P.TXT`, `2P.TXT`, `ALLTEXT.TXT`,
`C++START.TXT`, `CHEAT.TXT`, `CREDITS-OLD.TXT`, `DUPS.TXT`, `EDITEDMAP.TXT`,
`FLOW.TXT`, `LOADING.TXT`, `LOST.TXT`, `MEMSTUFF.TXT`, `MEMUSE.TXT`,
`NYMEM.TXT`, `SCORE.TXT`, `SEEKS.TXT`, `SKED.TXT`, `STARTUP.TXT`, and
`STATCOV.TXT`. The executable's `pkrfile.txt` occurrence belongs to a command
line/help diagnostic (`/l` lists PKR contents); it is not a game asset open.

## Unconnected corpus formats

| File | Size | Leading bytes / content | Current classification |
|---|---:|---|---|
| `CREATESELECT.BIN` | 3752 | little-endian MIPS code (`addiu sp,sp,-0x18` at offset 0) plus PSX editor strings | console executable/data blob; no PC filename xref |
| `MAINMENU.BIN` | 14548 | little-endian MIPS code/data (`lui` instruction at offset 0) | console executable/data blob; no PC filename xref |
| `EDMOD.BIN` | 101984 | little-endian MIPS code/data (`lui` instruction at offset 0) | console editor/module blob; no PC filename xref |
| `AUTOLOAD.BIN`, `CAREERSELECT.BIN`, `CREATEEDITOR.BIN`, `LEVELSELECT2.BIN`, `MULTISELECT.BIN` | 2332–23096 | little-endian MIPS entry bytes (`27 bd ff e8` in file order) | console/frontend module blobs; no PC filename xref |
| `CARSEL.BIN`, `CREDITS.BIN`, `CREEDIT.BIN`, `CRESAVE.BIN`, `CRESEL.BIN`, `EQUIPSEL.BIN`, `HANDICAP.BIN`, `LEVELSEL.BIN`, `MULTISEL.BIN`, `OPTIONS.BIN`, `PLAYSEL.BIN`, `STATEDIT.BIN`, `TRICKSEL.BIN` | 10492–46924 | big-endian MIPS entry bytes (`00 00 02 3c` in file order) | console/frontend/editor module blobs; no PC filename xref |
| `TEST.BIN` | 0 | empty | empty test placeholder |
| `XXXX.BIN` | 8544 | non-MIPS data header (`54 00 54 02 ...`) | unidentified console/tool blob; no PC filename or magic xref |
| `SKATE.SEQ` | 6298 | `70 51 45 53` (`pQES`-like) | structured legacy/console sequence; no PC filename xref |
| `SKATE2.SBL` | 35221 | `42 44 42 30` (`BDB0`) | one-off binary block/list; no PC filename or magic xref |
| `.SFX` set (25 files) | 0–1044 each | small fixed records, often PSX-style instruction/data words; 22 non-empty | console/build-time sound metadata; no `.sfx` PC open |
| `SKNY.TST` | 408904 | `04 00 02 00 ...` | test/tool data; no PC filename or magic xref |
| `SYMBOLS.TDF` | 31865 | CRLF text beginning with generated-file comments | tool/debug symbol text |
| `SKATE2.TAG` | 378993 | source/debug text beginning `c:\psx\sk2\...` | console build/source metadata |
| `TRICKS.TS` | 228 | semicolon-comment text | tool/input metadata |
| `ANSI.NT` | 79 | text containing `%systemroot%` | installer/OS configuration text |

The corresponding SHA-256 values are:

```text
CREATESELECT.BIN 045aed8e952bc78013577132de59ad89280d1a403afae00e8cc04c5be7a5416b
MAINMENU.BIN     e18d744650b223ee015634b14c339f3d69e6f3734a6e68900a947999b6af812f
EDMOD.BIN        b7b834faec47bc65af483db72db7f4167540616e0e9a3811a98e9ff22872fe5c
SKATE.SEQ   b3c5b302a821d6e04e2954532f7e01836cc7a1136fd17cbc7391b82bd7bd0c07
SKATE2.SBL  dbcee5cf0cfd7254080ab40865ec029022c4433061be2777b74e2563629493c9
SKNY.TST    fedf32f8a742e87804dff25eadbda101df944f7a0f22922919a5047fea901eef
SYMBOLS.TDF decfecaf33159458d1d13671d9c9dedb7c63e4b0798272d963dc2d23e9ef7190
SKATE2.TAG  b61b1e158d71dfb212abd76c81b50c6dcc547f1beb17a1ca8512a49d6d39dfd4
TRICKS.TS   79dedd9772ac6dc17cfc7d1504137477eebb5020c7a9434147b7f5f109213642
ANSI.NT      740c7ddbddfbf3cbecc1b5dc4d146048785a4c7fbfb700065a754757d2a33264
```

The remaining `.BIN` names above are grouped by entry signature rather than
given fabricated format names. `TRICKS.BIN` and `CRETEX.BIN` are the only two
`.BIN` files with proven PC consumers; the other 23 are either console/tool
blobs, empty placeholders, or still-unidentified data. The PC executable's
generic relocatable `.bin`/`.rel` helper is documented separately, but no
matching extracted `.REL` file or direct caller for these names has been
proven.

`NETPARK.PRK` is a separate executable string adjacent to the Windows saved-
park list (`GetWinSavedList` and `*.prk`). A direct reference to its string
address is absent from the exact PC image, so it is not evidence of another
custom-park loader. The proven runtime family remains the formatted
`park%d.prk` path in [custom-park-runtime.md](custom-park-runtime.md).

These are negative runtime results, not claims that the formats never had a
producer. A tool, console executable, or omitted resource table could still
consume them. They are outside the proven PC asset path until such a boundary
is found.

## CD/HED/WAD family

`re/notes/file-formats.md` records the separate `CD.HET`, `CD.HEP`, `CD.HED`,
and zero-filled `CD.WAD` observations. The PC executable's visible resource
path uses PKR2 and the common backend; the HED/WAD tables overlap and the WAD
has no payload. They are therefore metadata/legacy inputs, not an alternate
proven source for the runtime geometry objects.

## Search boundary

The negative result is bounded to the exact executable build above and the
extracted archive. It covers:

- no case-insensitive filename string for `SKATE.SEQ`, `SKATE2.SBL`,
  `SKNY.TST`, `SYMBOLS.TDF`, `SKATE2.TAG`, `TRICKS.TS`, or `ANSI.NT`;
- no case-insensitive filename string for `CREATESELECT.BIN`, `MAINMENU.BIN`,
  `EDMOD.BIN`, the other grouped `.BIN` names above, or any of the 25 `.SFX`
  files;
- no corresponding four-byte magic string for `pQES` or `BDB0` in the PC
  executable;
- no discovered caller that passes these names to `0x00449030` or the raw
  resource reader. The active PC sound path instead selects hardcoded VAB
  banks and constructs `audio/<inline-name>.wav` resources, as documented in
  [audio-runtime.md](audio-runtime.md).

The practical recreation rule is to implement these only when a new caller,
producer, or runtime consumer is independently located. Do not make them
silently feed the PRE/PSX scene loader.
