# Runtime helper cluster: asset-name resolution

Status: observed

Two adjacent helpers are now exact VC6 reconstructions:

- `0x004f6080` removes an existing extension and appends the requested suffix
  with the original CRT-style scans and bulk copies.
- `0x004f60d0` copies and normalizes an asset name, resolves it through the
  shared lookup path, and releases a successful result.

The string scans, copy direction, absolute literals, calls, and cleanup paths
match under `vc6-coff-text`.
