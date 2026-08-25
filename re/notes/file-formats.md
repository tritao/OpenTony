# File Formats

## PKR2 (`ALL.PKR`)

Observed in the extracted PC disc at `SETUP/data/ALL.PKR`.

- Magic: ASCII `PKR2` at offset `0x00`.
- Header: four little-endian `u32` values at `0x00`: magic, version (`1`), directory count (`21`), and file count (`3771`).
- Directory records: 40 bytes each, beginning at `0x10`: a 32-byte NUL-terminated path, an absolute file-record-table offset, and a file count.
- File records: 48 bytes each: a 32-byte NUL-terminated name, marker `0xFFFFFFFE`, absolute payload offset, payload size, and the same payload size again.
- Payload offsets are absolute offsets into the PKR file. The local archive has 3,771 files and is 197,886,331 bytes.

`tony assets inspect-pkr` validates these bounds and reports the directory table. `tony assets extract-pkr` writes a generated file tree and a manifest under `build/` without modifying the source archive.

The implementation was cross-checked against [JayFoxRox/thps2-tools](https://github.com/JayFoxRox/thps2-tools), especially its `extract-pkr.py` layout. The reference tool treats the file marker and duplicated size as invariants; OpenTony also validates archive bounds, duplicate paths, and extraction path safety.

## PRE

Observed in the files extracted from `ALL.PKR`, including `CREATE.PRE`, `LEVEL.PRE`, and `MAINMENU.PRE`.

- Header: one little-endian `u32` file count at offset `0x00`.
- Each record starts with a NUL-terminated filename, padded to a 4-byte boundary.
- A little-endian `u32` payload size follows the padding, then the payload bytes follow inline immediately; the next record is 4-byte aligned after the payload as well.
- There is no fixed magic; the file count and record bounds are the current format checks.

`tony assets inspect-pre` reports the embedded entries and `tony assets extract-pre` restores them under `build/`. This exposes the individual BMP/FNT/other resources that are otherwise nested inside PKR.

## TRG (`*_T.TRG`)

Observed in 32 files extracted from `ALL.PKR`.

- Header: ASCII `_TRG` at offset `0x00`, followed by little-endian version `2` and a little-endian node count.
- Node table: `node_count` little-endian absolute offsets immediately after the 12-byte header. Offsets are strictly increasing and bound each node to the next offset, or to end-of-file for the final node.
- Each node begins with a little-endian `u16` node type. The current inspector names types documented by the reference disassembler, but leaves payloads opaque.

`tony assets inspect-trg` reports the node-type histogram; add `--nodes` for offsets and node sizes. `tony assets inventory` summarizes the extracted tree by extension. Full TRG command/script disassembly remains a separate step because node payloads have type-specific structures and known THPS1/THPS2 variants.

## PSX

The extracted PC PKR contains 282 `.PSX` files, all using the observed version `4` / marker `2` header. The header stores an absolute tag offset and object count; 36-byte object records are followed by a model count and absolute model offsets. Model headers contain 16-bit vertex/normal/face counts, followed by packed 8-byte vertices and normals and variable-length faces. Object XYZ positions and model-name hashes are fixed-point scene metadata; geometry and texture payloads are decoded only during extraction. The `0x0000000A` blockmap tag starts with `<iiiiHH>` X/Z bounds and cell counts, followed by per-cell `unknown_1`, `unknown_2`, an object-reference count, object indices, and a zero terminator. This matches the [PSX format notes](https://gist.github.com/iamgreaser/b54531e41d77b69d7d13391deb0ac6a5).

Across the local corpus this parses 29,232 models, 1,878 texture headers, 45 blockmaps, and 90,553 blockmap object references with no structural failures. Use `tony assets inspect-psx`; add `--models`, `--textures`, or `--tags` for detailed tables. `tony assets extract-psx` exports version-4 models as OBJ, a translated scene OBJ containing every object placement, indexed/RGB textures as PPM, `blockmap.json`, and collision geometry as `collision.obj` with surface-flag materials. The model, object, and texture structures follow [JayFoxRox/thps2-tools' converter](https://github.com/JayFoxRox/thps2-tools/blob/master/convert-psx.py); OBJ UVs are normalized against the source texture dimensions with the PSX top-left V origin flipped for standard Wavefront viewers. Texture payloads that do not fit their declared aligned dimensions are reported in the manifest and skipped.

## CD.HET/CD.HEP/CD.HED/CD.WAD

The extracted PKR contains all four files under `data/`. The PC variant uses two variable-length filename tables: each record is a NUL-terminated ASCII filename, 4-byte alignment padding, a little-endian `u32` offset, and a little-endian `u32` size. Both tables end with `0xffffffff`; `CD.HET` has 1,531 records and `CD.HEP` has 1,083.

`CD.HED` contains 1,531 little-endian 12-byte records (`filename hash`, `offset`, `size`) followed by four zero bytes. The filename hash matches the non-reflected CRC routine used by [JayFoxRox/thps2-tools' HED/WAD extractor](https://github.com/JayFoxRox/thps2-tools/blob/master/extract-hed-wad.py), and every `CD.HET` name/hash/offset/size tuple correlates in the local extraction. However, the HED `offset:size` ranges overlap for 1,530 of 1,531 entries, so the size field cannot be treated as a direct raw-WAD byte count for this PC variant.

The associated `CD.WAD` is 1,601,840 bytes but is entirely zero-filled; the ISO filesystem contains no separate CD.WAD beside `ALL.PKR`, and the executable's visible resource strings point to PKR/ZLIB handling rather than this table family. The extracted PKR already contains many HED assets as ordinary decoded files, including exact-size matches for 1,164 HED entries before nested PRE extraction and 1,197 after extracting all 19 PRE containers. This makes the local HED/WAD set metadata-complete but payload-incomplete or legacy/unused for the PC runtime; it must not be treated as a valid raw asset archive yet.

`tony assets inspect-hed` records these correlations, overlaps, and bounds. `tony assets extract-hed` deliberately refuses the current WAD and overlapping tables until a non-zero payload source and the PC variant's decompression semantics are recovered; `--allow-zero-wad` is available only for explicit forensic extraction of synthetic/partial inputs.

Record observations with build identity, addresses, and evidence confidence. Prefer links into `re/evidence/` for claims that should survive refactors.
