# PRE resource manager runtime path

Status: confirmed PRE container load, slot table, embedded-resource lookup,
and downstream font-consumer handoff
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x004a8ee0`, `0x004a9000`, `0x004a90f0`, `0x004a9110`, `0x004a9410`, `0x004a9480`, `0x004a9330`

PRE is a separate runtime resource family from the PSX scene geometry. It is
used for front-end/UI packages such as `levelsel.pre` and `panel.pre`, and the
same manager is available to other resource users. The loader keeps the PRE
container in memory and returns pointers into its embedded payloads.

## Manager allocation and layout

`0x004a8ee0`, identified by the source string as `H:\\TonyHawk\\Pc2\\PRE.cpp`,
allocates `0x144` bytes through the game's allocator and initializes the PRE
manager with `0x004a9000`. The recovered layout is:

```text
+0x00             virtual-function table / manager header
+0x04..+0x40      16 loaded PRE payload pointers, 4 bytes each
+0x44..+0x143     16 case-preserving names, 16 bytes each
```

The manager pointer is `DAT_0056b834`. `0x004a9000` zeroes all 16 payload
slots and initializes the fixed-size name table. A failed or exhausted slot
is rejected by the `out_of_PREManager_slots` assertion.

## Disk-to-memory load

`0x004a90f0(name, flags)` is the public load wrapper. It calls
`0x004a9110(manager, name, flags)` and then synchronizes through
`0x00449660`.

`0x004a9110` performs the actual path:

```text
PRE name
  -> 0x00449030 game-owned file open
  -> 0x0046f490 allocation using the file size and flags
  -> 0x00449230 file load/synchronization start
  -> manager payload slot + 16-byte name slot
```

The input filename is copied to the manager's name table after the load. The
runtime file I/O layer therefore exposes the entire PRE container as one
loaded buffer; the manager does not parse the embedded record table during
load.

The front-end call sites independently show this loader in normal use:

```text
level-select state -> 0x004a90f0("levelsel.pre", 0)
level-load setup   -> 0x004a90f0("panel.pre", 1)
```

`0x004a9330` unloads by case-insensitive name, overwrites the matching 16-byte
name slot with the empty-name template, frees the corresponding loaded buffer
through `0x0046f4d0`, and clears its pointer slot. If no slot matches, it
asserts `PRE_file_is_not_loaded` after scanning all 16 entries. The normal
panel and level-select flows call the unload wrapper after leaving those
states.

## Embedded resource lookup

`0x004a9410(manager, embedded_name, out_size)` copies at most the caller's
short-name input into a 16-byte local, lowercases ASCII capitals in that
copy, and scans the 16 loaded PRE slots. For each loaded buffer it calls
`0x004a9480`. The manager's stored container names are compared
case-insensitively by the separate unload path at `0x004a9330`.

`0x004a9480` parses the in-memory PRE layout conservatively and returns a
pointer into the buffer when a name matches. Its observed record walk is:

```text
u32 file_count at buffer +0x00
for each entry:
    NUL-terminated name
    4-byte alignment after the name (including its NUL)
    u32 payload_size
    payload bytes
    4-byte alignment before the next entry

The alignment is performed independently for every name and payload. In
other words, the next entry is computed as
`entry + align4(strlen(name) + 1) + 4 + align4(payload_size)`; no fixed-size
record header is inserted between entries.
```

It writes the matched payload size to the caller's output pointer and returns
the payload address immediately after the size word. This matches OpenTony's
offline `PreArchive` parser and is the disk-to-runtime bridge for individual
BMP/FNT/other resources nested inside PRE.

The runtime lookup contract is therefore:

```text
PRE file on disk
  -> one loaded container buffer
  -> 0x004a9480 name/size/payload walk
  -> embedded resource pointer consumed by the UI/asset subsystem
```

The shared file boundary independently proves one concrete embedded consumer.
`0x0044ada0` (`Font_Load`) requests an FNT name through `0x00449030`; when the
PRE manager finds it, the returned payload pointer and size go through the
normal allocation/read/synchronization sequence and into `0x0044aea0`, which
constructs the runtime font before the temporary copy is freed. This closes
the embedded PRE path through a runtime object, not merely through a raw
pointer lookup:

```text
levelsel.pre / panel.pre
  -> 0x004a9480 embedded FNT payload
  -> 0x0044ada0 Font_Load
  -> 0x0044aea0 runtime font construction
  -> font/text renderer
```

The same lookup is independently exercised by the panel bitmap initializer
`0x0048ad50` (`Panel_LoadRuntimeSprites`). Its static call sites pass
`s2switch.bmp` and `s2icon8.bmp` to the normal image constructor
`0x00457420`; that constructor calls `0x00449030`, whose short-name fast path
calls `0x004a9410` before falling back to disk. Both names are present in the
offline `PANEL.PRE` table:

```text
PANEL.PRE / s2switch.bmp  payload offset 0x4620  size 1444
PANEL.PRE / s2icon8.bmp   payload offset 0x4bd4  size 1008
```

The panel call site supplies 64x64 realization dimensions and retains each
returned image object in the panel's sprite slots. This is a concrete
embedded-BMP bridge, distinct from the FNT path:

```text
PANEL.PRE
  -> 0x004a9480 embedded name/size/payload walk
  -> 0x00457420 image constructor
  -> panel sprite handle/slot
  -> panel renderer
```

## Confidence and limits

- `confirmed`: manager allocation size, 16-slot/16-name layout, file-open and
  allocation path, case-insensitive embedded-resource search, payload pointer
  return, unload behavior, the FNT payload-to-runtime-font consumer path, and
  the panel embedded-BMP-to-image-object path.
- `observed`: `levelsel.pre` and `panel.pre` call sites and the fixed-size
  manager table.
- `inferred`: which individual embedded BMP/FNT entries are consumed by each
  front-end widget and whether every PRE user shares the same manager instance.

This completes the shared PRE container path. The animation `SK2ANIM.PSX`
consumer is documented separately in [animation-runtime.md](animation-runtime.md):
its payload is parsed into animation tables rather than treated as a UI PRE
entry.
