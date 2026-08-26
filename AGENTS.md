# OpenTony agent instructions

## Matching reconstruction

Before changing `match/`, read `match/POLICY.md` and preserve its byte-identity
and ownership rules.

Classify modules by their implementation body, not their filename, compiler,
or build strategy:

- `raw`: original bytes preserved with `incbin`.
- `hybrid`: reviewed source plus a preserved raw region.
- `asm`: reviewed standalone assembly or data directives.
- `vc6_asm`: VC6 `__declspec(naked)` inline assembly, including bodies that
  use `__asm` or `_emit`. A `.cpp` container does not make this semantic C++.
- `cpp`: genuine higher-level C or C++ with no naked inline assembly.

Never mark a naked assembly block as `cpp`. When converting `vc6_asm` to
`cpp`, replace the assembly body with typed higher-level code and retain the
module's exact-byte comparison strategy.

Before committing matching work, run:

```bash
tony split rebuild
tony split verify
pytest -q
```

`tony split verify` must report a byte-identical rebuilt executable and rejects
misclassified `vc6_asm`/`cpp` sources.
