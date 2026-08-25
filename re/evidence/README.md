# Evidence and confidence

Use these labels consistently.

## `observed`

Directly seen in static or runtime data, but semantic interpretation may still be incomplete.

Example: instruction at address X writes four bytes to `[esi+0x30]` whenever the skater moves.

## `confirmed`

Meaning has been independently established by a controlled experiment, multiple independent observations, symbols/debug metadata, or a strong cross-build match.

Example: changing only the value at a recovered field changes X position exactly as predicted across repeat runs.

## `inferred`

Best current explanation supported by evidence but not yet independently proven.

Example: function is called while airborne, changes vertical velocity, and appears to implement gravity/vertical response.

## `provisional`

A working label chosen to make analysis navigable. It may be wrong.

Do not silently upgrade confidence. Evidence files should say what would falsify or strengthen the interpretation.

## Evidence record template

```markdown
# Subject

Status: inferred
Build: <sha256 or binary id>
Addresses: ...

## Observation

...

## Experiment

...

## Interpretation

...

## Open questions / falsifiers

...
```
