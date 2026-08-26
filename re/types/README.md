# Recovered retail types

These YAML files describe partial retail memory layouts for the repository's
single recorded PC build. Platform, pointer width, and endianness come from
`re/config/binaries.yml`; do not repeat identical ABI metadata in every file.
Native domain types remain C++ under `src/` and connect through explicit
adapters, as specified by `docs/RECONSTRUCTION_WORKFLOW.md`.

Keep one subsystem per simply named file such as `collision.yml` or
`player.yml`. Do not add a redundant `-runtime` suffix. Do not create
speculative giant structs merely to make the decompiler prettier.

Validate the complete corpus with:

```bash
tony types verify
```

The machine-readable shape is recorded in `re/types/schema.json`; the Python
validator additionally checks references, evidence paths, bounds, and overlaps.

## Layout example

```yaml
version: 1
types:
  - name: CSkater
    kind: fixed_layout
    size: null
    confidence: provisional
    fields:
      - offset: 0x08
        name: position_x
        type: q12_i32
        confidence: observed
        evidence:
          - re/evidence/functions/physics.md
```

## Canonical type grammar

Use `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `f32`, `f64`,
`char`, `q12_i32`, and `q16_i32` for scalar storage. Compound forms contain no
whitespace:

```text
pointer<T>
array<T,N>
sequence<T>
bytes<N>
```

`bytes` and `cstring` describe unknown-length opaque/string storage. A bare
identifier references another recovered type. Use `kind: variable_record` for
symbolic offsets and `kind: alias` with `target` when retaining an old name.
Overlapping fields must share an explicit `overlay_group`.

`tony ghidra rebuild` does not yet apply these layouts. Add generation only
when a matching or analysis consumer needs it; the validated YAML remains the
canonical evidence source.
