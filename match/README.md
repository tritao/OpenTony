# Byte-preserving split

This tree contains the tracked manifest and modular NASM sources for rebuilding
the recorded `THawk2.exe` without a linker. Original and assembled binary
fragments are generated locally and ignored by Git.

See `match/POLICY.md` for ownership, status, encoding, and subsystem rules.

Bootstrap the split after extracting the recorded executable:

```bash
tony split init
tony split rebuild
tony split verify
```

`init` creates coarse 64 KiB modules by default. Each raw module uses `incbin`
for its exact original range. Modules can then be subdivided and replaced with
real assembly while the byte-identity invariant remains enforced.

Generate readable address constants and split one coarse raw module with:

```bash
tony split symbols
tony split module 0x004ca9f0 0x004caa20
tony split compare 0x004ca9f0
```

Export Ghidra's complete `.text` ownership map and generate non-mutating,
function-aware split proposals with:

```bash
tony split coverage
tony split propose-modules
tony split propose-modules --safe-only --range 0x004c0000:0x004d0000
tony split accept-proposal Math_Vector3Add --dry-run
tony split accept-proposals --tracked-only --range 0x004c0000:0x004d0000 --dry-run
```
