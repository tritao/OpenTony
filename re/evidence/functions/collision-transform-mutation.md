# Collision linked-transform calibration

Status: runtime-confirmed for the `0x0200` scale branch on controlled inputs;
natural non-identity linked-object factors remain unobserved

Build: `f2c7ca7cbc31abd8f748bd4afdc1e30aa1a6700ce91893b618450fd16172669c`

## Hypothesis and discriminators

The object tail words at `+0x28`, `+0x2a`, and `+0x2c` are signed Q12 scale
factors consumed by `0x004f5540`. Competing explanations were:

- the factors scale independent matrix columns in place;
- the helper only copies or replaces diagonal entries; or
- the tail is unrelated to the matrix and belongs only to culling/metadata.

The distinguishing observation is the matrix after the helper, including
negative and signed-short boundary factors, while checking that the object
tail is restored before the caller resumes.

## Observations

The reversible `tony-collision-transform-mutate` probe filtered full-word flag
`0x0200` records, replaced the three tail words at helper entry, captured the
matrix at return, then restored both the object tail and destination matrix.
The two bounded captures produced 24 completed calls:

| Capture | Injected Q12 words | Calls | Return site | Representative matrix result |
|---|---:|---:|---|---|
| `build/debug/sessions/collision-runtime-20260826-5/collision.trace.ndjson` | `[2048, -4096, 6144]` | 12 | `0x00461c64` | `[2048,0,0,0,-4096,0,0,0,6144]` |
| `build/debug/sessions/collision-runtime-20260826-6/collision.trace.ndjson` | `[32767, -32768, 1]` | 12 | `0x00461c64` | `[32767,0,0,0,-32768,0,0,0,1]` |

The live records had flags `0x0202` and original factors `[4096, 4096,
4096]`. Across identity and zero-column matrices, the output groups were:

- `+0x28` → matrix words `0, 3, 6`;
- `+0x2a` → matrix words `1, 4, 7`;
- `+0x2c` → matrix words `2, 5, 8`.

The max/min run preserves the negative sign and accepts the signed-short
boundary words. Because the observed matrices are identity or zero-column
matrices, this run does not independently distinguish overflow clamping from
wraparound for an out-of-range product; that saturation behavior remains
supported by the static instruction evidence. Every event restored the three
original tail words. The post-fix capture
`build/debug/sessions/collision-runtime-20260827-1/collision.trace.ndjson`
added three calls with `[6144, -2048, 8192]`; each recorded an exact
`matrix_after_restore` equal to `matrix_before`, and the unit test verifies the
same restoration contract. This confirms that the tail is transform input, not
merely cull metadata, and that the normal path is independently column-scaled
in Q12. The exact non-identity product rounding remains a static
instruction-level result until a naturally non-identity matrix is captured.

These calls all returned to the shared render/model-transform caller
`0x00461c64`, not the dynamic collision return sites `0x00464067` or
`0x0046409a`. Therefore this run calibrates the helper contract but does not
yet prove that a naturally loaded linked collision object carries non-identity
scale. The raw traces remain under `build/` and are not evidence artifacts.
