# Native function progress

`functions.yml` maps retail function addresses to explicit native reconstruction
progress. It does not claim byte matching; exact implementation status remains
in `match/manifest.yml`.

Allowed native states are:

```text
unmodeled -> modeled -> tested -> trace-validated -> integrated
```

Every entry must match a tracked function name/address and reference existing
source, test, and evidence files. `tested` and later states require tests;
`trace-validated` and `integrated` also require runtime evidence. Validate with:

```bash
tony native verify
```

Keep mappings conservative. A partial portable boundary may be `modeled` even
when related tests exist if those tests do not cover the complete retail
function contract.
