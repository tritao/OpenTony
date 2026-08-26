# Reconstruction slices

Slices are small committed work-unit manifests for vertical reverse-engineering
work. They define scope, artifacts, open questions, and completion criteria;
they do not replace evidence or matching/native status.

The normal loop is:

```bash
tony slice list
tony slice claim ID
tony slice show ID
tony ghidra gaps --slice ID
# inspect, recover types, model/test, match, and implement native behavior
tony verify --all
pytest -q
tony slice release ID
```

Claims live under ignored `build/slices/leases/`. They coordinate parallel
sessions but are not evidence and are never committed. Codex sessions use
`CODEX_SESSION_ID`; other environments can pass `--owner` or set
`TONY_SLICE_OWNER`. Replacing or releasing another owner's claim requires an
explicit `--force`.

Use `shared.functions` only when concurrent ownership is intentional. Avoid
turning slices into a dependency solver, issue tracker, or branch manager.
