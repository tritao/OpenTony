# Recovered types

Keep type layouts in small YAML files once offsets are evidence-backed. Do not create speculative giant structs just to make the decompiler prettier.

Suggested record:

```yaml
name: CSkater
size: null
confidence: provisional
fields:
  - offset: 0x30
    name: position_x
    type: f32
    confidence: observed
    evidence:
      - re/evidence/structures/cskater-position.md
```

`tony ghidra rebuild` does not yet apply these type files; symbol application is the first implemented slice. Type application should be added only after the schema has real THPS2 data to validate against.
