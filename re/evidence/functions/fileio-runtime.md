# Runtime file/archive handoff

Status: confirmed game-owned open, PRE fast path, PKR2 backend validation, abstract handle, package record copy, and loaded-buffer handoff
Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`
Addresses: `0x00449030`, `0x00449230`, `0x00449660`, `0x0044ada0`, `0x004a9410`, `0x004a9480`, `0x004e75b0`, `0x004e79f0`, `0x004e7ad0`, `0x004e7bc0`, `0x004e9150`, `0x004e9200`, `0x004e92c0`, `0x004e9390`, `0x004e9480`, `0x004e9570`, `0x004e9730`, `0x004e9940`, `0x005013c8`, `0x0050229f`, `0x00502358`, `0x00502387`

The asset path has two runtime entry modes. A short name may resolve directly
to an embedded PRE payload; otherwise the game opens the resource through the
generic PC backend and the spooler copies it into a game-owned allocation.

## Game-owned open

`0x00449030` is the first game-owned open/size routine after the lower file
layer. Its `fileio.cpp` assertions show the following sequence:

1. reset the current-load state;
2. if the PRE manager is active and the name is short enough, query
   `0x004a9410` and return the embedded payload pointer/size directly;
3. otherwise copy the requested path into the file-I/O state, open it through
   `0x004e75b0`, seek to the end and back through `0x004e79f0`, and return the
   resource size; and
4. leave the active handle/state for `0x00449230` to consume.

This is why the Warehouse evidence shows the same game-owned open boundary for
`SKWARE_T.TRG` and `SKWARE.PSX`, even though the source may be an extracted PKR
file, a packaged backend entry, or an embedded PRE member.

The PRE branch is a real downstream asset path, not just a lookup optimization.
`0x0044ada0` (`Font_Load`) calls `0x00449030`, treats the returned pointer and
size as an FNT image, allocates a destination, schedules the copy through
`0x00449230`, synchronizes with `0x00449660`, constructs the font through
`0x0044aea0`, and frees the temporary buffer. Thus a font requested after
`levelsel.pre` or `panel.pre` can be served directly from the loaded PRE
container while using the same font runtime constructor as a standalone file.

## Backend handle contract

`0x004e75b0` allocates a backend slot, normalizes the requested path, and
selects the direct/package access mode. `0x004e79f0` implements the matching
seek operation. The backend keeps a per-slot position and size for ordinary
files; package-backed slots delegate seek/position to the package object. The
resource handle is therefore an abstract slot, not necessarily a Win32 file
descriptor.

`0x004e7ad0` is the bounded read helper used by the bitmap and asset loaders;
`0x004e7bc0` closes/releases the backend resource. Their use below
`0x00449030` is separate from the game allocation wrapper at `0x0046f490`.

## PKR2 package boundary

The package-backed branch reaches `0x004e9940`. Its exact-build code opens the
package stream, reads a 16-byte package header through the backend read helper,
and rejects the resource unless the first word is the little-endian `PKR2`
magic (`0x32524b50`). It then walks the package directory using runtime tables
with observed 0x28-byte and 0x30-byte record strides, matching the requested
path before returning an abstract package resource.

OpenTony's offline `ALL.PKR` reader independently identifies the same records:

```text
header        PKR2, version 1, 21 directories, 3771 files
directory     0x28 bytes: 32-byte path + absolute table offset + count
file record   0x30 bytes: 32-byte name + 0xfffffffe marker + offset + size + size
```

The runtime stride match is therefore a format/loader cross-check, while the
runtime lookup still remains responsible for the package's path/hash policy.

The package resource uses separate seek/read helpers (`0x0050229f` and
`0x00502358`) behind the same `0x004e79f0`/`0x004e7ad0` contract. The package
lookup copies the matching 0x30-byte file record, allocates its `+0x2c` size,
seeks to `+0x24`, and reads that many bytes. `0x004e9730` is the entry-payload
dispatch: the observed `0xfffffffe` marker returns the read buffer unchanged;
other markers select a generic transform table. This matches the offline
archive, where the level records use the raw marker and duplicate size words.

Concrete archive coordinates are independently known for the Warehouse files:

```text
ALL.PKR / data/SKWARE.PSX
  file record: 0x30-byte record, marker 0xfffffffe
  payload:     absolute 0x05b992b1, size 0x20034 (131124)

ALL.PKR / data/SKWARE_T.TRG
  file record: 0x30-byte record, marker 0xfffffffe
  payload:     absolute 0x05eb3a62, size 0x295e (10590)
```

The lower package stream is a 0x38-byte runtime object allocated by
`0x00507333` and opened/configured through `0x005071c3`. The read wrapper
`0x00502358` initializes and tears down that object around
`0x00502387`; seek follows the same pattern through `0x0050229f` and
`0x005022cb`. The independently supported stream fields are:

```text
+0x00 current input cursor
+0x04 remaining bytes in the current input buffer
+0x08 input buffer/base
+0x0c stream state flags
+0x10 underlying runtime file handle
+0x18 input/block transfer size
+0x1c auxiliary owned pointer, freed by stream cleanup when non-null
```

`0x00502387` multiplies element count by element size, uses the buffered cursor
path when the stream state has the observed `0x10c` bits, and otherwise
refills/decodes through the underlying handle while updating `+0x00`/`+0x04`.
The field names above are
deliberately operational; the individual flag bits and generic transform
families are not assigned C++ semantics yet.

### Non-raw package transforms

The generic transform dispatch is now statically resolved. `0x004e9730` first
handles the raw marker `0xfffffffe`; markers below that value fail. Markers
`0`, `1`, and `2` index a second dispatch table at `0x0054b414`, whose entries
are the inverse of the three package compressors:

```text
marker 0  -> 0x004e9390  BIBD/RLE decode: [u8 run_count][u8 value]
marker 1  -> 0x004e9480  WIBD/RLE decode: [u16 run_count][u8 value]
marker 2  -> 0x004e9570  ZLIB decode through 0x004f9d50
```

The matching compression table at `0x0054b408` is:

```text
marker 0  -> 0x004e9150  BIBD/RLE encode
marker 1  -> 0x004e9200  WIBD/RLE encode
marker 2  -> 0x004e92c0  ZLIB encode through 0x004f9d30
```

The RLE implementations are direct evidence for the byte order: the byte
decoder consumes a count byte followed by a value byte and fills the decoded
buffer, while the word-count decoder consumes a little-endian count word and a
value byte. The corresponding encoders split runs at their representable
maximums (`0xff` for the byte-count form and `0xffff` for the word-count form).
The ZLIB wrappers report the embedded `compressZLIB()`/`decompressZLIB()` error
strings and return the decoded buffer/size through the same entry-payload
boundary.

For a non-raw file record, the runtime preserves the two size words at `+0x28`
and `+0x2c`; the former is the bytes read from the package and the latter is
the companion size used when allocating/decoding the destination. Warehouse's
raw entries have equal values, so this distinction is not observable in the
level files themselves. The complete path is now:

```text
PKR2 file record (+0x20 marker, +0x24 archive offset, +0x28/+0x2c sizes)
  -> read stored bytes
  -> 0x004e9730
       raw marker: return stored buffer
       0/1: BIBD/WIBD decode
       2: ZLIB decode
  -> caller-owned resource buffer
```

A live startup capture of `TRICKS.BIN` reached the same package path and showed
the copied record in memory with marker `0xfffffffe`, absolute payload offset
`0x02f69c2c`, and both size words `0x8190`. That validates the runtime record
interpretation independently of the offline parser; the Warehouse offsets
above come from the same exact `ALL.PKR` table.

## Buffer load and synchronization

`0x00449230(destination)` validates that an open resource is active and either
copies a PRE-resolved payload directly or schedules a backend read into the
caller-provided allocation. It then starts the asynchronous spool state.

`0x00449660` is the synchronization wrapper. It services the spool state until
the active load completes, then returns to the parser. Warehouse therefore has
this complete lower path:

```text
SKWARE.PSX / SKWARE_T.TRG name
  -> 0x00449030 game-owned open/size
  -> 0x004e75b0 backend handle (or 0x004a9410 PRE payload)
  -> 0x0046f490 game allocation
  -> 0x00449230 read/schedule
  -> 0x00449660 synchronize
  -> PSX/TRG format parser
```

The backend package format, raw level-record path, and all three generic
transform families are now separated. The PRE bypass remains independent of
the package stream.

## Native recreation boundary

`src/assets/pkr_asset.*` now implements the bounded package backend. It retains
the path-bearing 40-byte directory records and 48-byte file records, validates
absolute payload bounds and duplicated size words, and implements the confirmed
raw/BIBD/WIBD/ZLIB marker dispatch. A real `ALL.PKR` fixture validates 21
directories, 3,771 entries, and the direct package-to-format bridge:

```text
PkrArchive::load(ALL.PKR)
  -> decode("data/SKWARE.PSX")
  -> PsxArchive::parse(...)
  -> 252 objects / 288 models
```

`LevelRuntime` also exposes this as a package-backed constructor. It decodes
the requested `data/SKWARE_T.TRG` and `data/SKWARE.PSX` entries into owned
format images before building the same trigger, scene-object, model, and
collision state as the extracted-file constructor. The native integration
fixture therefore exercises both the package boundary and the runtime object
boundary in one path.

The decoded bytes are caller-owned, matching the game's allocation/read
handoff rather than retaining a pointer into the package image.

The common handle and load boundary is now represented by
`src/assets/resource_runtime.*`. `ResourceBackend` owns reusable abstract
slots with a cursor, bounded seek, partial read, exact-read failure, and
explicit close. Its direct mode owns a file image; its package mode resolves
one `PkrArchive` record and owns the decoded payload, so neither mode exposes
a backend pointer to a format parser. `ResourceStream` is the native adapter
for the observed package-stream calls: it delegates seek/read/close and
checks `element_count * element_size` before consuming the requested span.

`ResourceLoader` closes any previous active load, applies the observed
short-name PRE lookup (`< 0x10` bytes and no path separator), and otherwise
opens the configured direct/package backend. `load()` copies the PRE span or
reads the backend into caller storage; `synchronize()` completes the native
immediate adapter and releases the active handle. `load_owned()` is therefore
the portable equivalent of the open → allocate → schedule → sync handoff:
the returned bytes survive both backend close and PRE unload, and can be
passed directly to `FntRuntimeFont::parse` or another format parser.
The package-backed `LevelRuntime` constructor now uses this same loader for
its TRG and PSX entries before constructing the trigger and scene runtime
objects, keeping the common file boundary in front of those existing
consumers.

The regression at `src/assets/resource_runtime_test.cpp` exercises a raw
PKR2→FNT construction, stream tail seek and element reads, PRE payload copy
followed by container unload, direct-file fallback, handle reuse/close, and
overflow/out-of-range failure cleanup. It intentionally does not assign the
retail allocator's count prefix, asynchronous spool states, backend slot
capacity, or exact path-normalization rules.

## Confidence and limits

- `confirmed`: the first game-owned open, PRE short-name lookup, backend handle
  path, PKR2 signature check, package-directory walk, 0x30-byte entry copy,
  raw-marker dispatch, size/seek sequence, allocation/read/synchronize
  boundary, and the distinction between an abstract backend slot and a Win32
  descriptor.
- `observed`: the 0x38-byte package stream allocation and the operational
  cursor/buffer/flags/handle/size fields used by its seek/read/cleanup code.
- `observed`: the Warehouse file names and the shared path used by both TRG and
  PSX loaders.
- `confirmed`: marker dispatch and BIBD/WIBD/ZLIB transform entry points; the
  exact error-cleanup behavior for every asynchronous failure case remains
  open.
- `native-tested`: direct/PKR abstract handle ownership, PRE-vs-backend load
  selection, caller-owned synchronization output, and bounded stream reads;
  these are portable adapters around the confirmed boundaries, not claims of
  byte-identical allocator or spooler behavior.
