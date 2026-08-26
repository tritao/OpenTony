# OpenTony agent instructions

## Matching reconstruction

Before changing `match/`, `re/`, or `src/`, read and follow
`docs/RECONSTRUCTION_WORKFLOW.md`. For matching work, also read
`match/POLICY.md`. These are the canonical sources; do not redefine their rules
elsewhere.

Before substantial reversing work, use `tony slice list`, claim the relevant
slice with `tony slice claim ID`, and follow the checklist from
`tony slice show ID`. Release it when the work session is finished. Slice
claims are local coordination state, not evidence.

In particular, never mark a naked inline-assembly block as semantic `cpp`; use
the status required by `match/POLICY.md`.

Before committing matching work, run:

```bash
tony verify --all
pytest -q
```

`tony verify --all` must report a byte-identical rebuilt executable and rejects
misclassified `vc6_asm`/`cpp` sources.
