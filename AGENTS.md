# OpenTony agent instructions

## Reconstruction

Before changing `match/`, `re/`, or `src/`, read and follow the operational
quick start in `docs/SLICE_WORKFLOW.md`. Detailed policy lives in
`docs/RECONSTRUCTION_WORKFLOW.md`; exact matching status rules live in
`match/POLICY.md`. Do not redefine those rules elsewhere.

In particular, never mark a naked inline-assembly block as semantic `cpp`; use
the status required by `match/POLICY.md`.

Before committing matching work, run:

```bash
tony verify --all
pytest -q
```

`tony verify --all` must report a byte-identical rebuilt executable and reject
misclassified `vc6_asm`/`cpp` sources.
