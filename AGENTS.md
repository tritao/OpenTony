# OpenTony agent instructions

## Matching reconstruction

Before changing `match/`, read and follow `match/POLICY.md`. It is the canonical
source for module ownership, statuses, promotion criteria, exact encodings, and
progress reporting; do not redefine those rules elsewhere.

In particular, never mark a naked inline-assembly block as semantic `cpp`; use
the status required by `match/POLICY.md`.

Before committing matching work, run:

```bash
tony split rebuild
tony split verify
pytest -q
```

`tony split verify` must report a byte-identical rebuilt executable and rejects
misclassified `vc6_asm`/`cpp` sources.
