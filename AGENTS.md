# OpenTony agent instructions

## Matching reconstruction

Before changing `match/`, `re/`, or `src/`, read and follow
`docs/RECONSTRUCTION_WORKFLOW.md`. For matching work, also read
`match/POLICY.md`. These are the canonical sources; do not redefine their rules
elsewhere.

In particular, never mark a naked inline-assembly block as semantic `cpp`; use
the status required by `match/POLICY.md`.

Before committing matching work, run:

```bash
tony types verify
tony native verify
tony split rebuild
tony split verify
pytest -q
```

`tony split verify` must report a byte-identical rebuilt executable and rejects
misclassified `vc6_asm`/`cpp` sources.
