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

## Next format: CD.HED/CD.WAD

The reference project identifies the PC `CD.HED/CD.WAD` pair as a container used by the game, with a variable-length NUL-terminated filename followed by 4-byte alignment, an offset, and a size. The current extracted installer contains `ALL.PKR` but no HED/WAD pair, so the next evidence source should be an installed game directory or a runtime file-open trace.

Record observations with build identity, addresses, and evidence confidence. Prefer links into `re/evidence/` for claims that should survive refactors.
