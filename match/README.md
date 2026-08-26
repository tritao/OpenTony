# Byte-preserving split

This tree contains the tracked manifest and modular NASM sources for rebuilding
the recorded `THawk2.exe` without a linker. Original and assembled binary
fragments are generated locally and ignored by Git.

Bootstrap the split after extracting the recorded executable:

```bash
tony split init
tony split rebuild
tony split verify
```

`init` creates coarse 64 KiB modules by default. Each raw module uses `incbin`
for its exact original range. Modules can then be subdivided and replaced with
real assembly while the byte-identity invariant remains enforced.
