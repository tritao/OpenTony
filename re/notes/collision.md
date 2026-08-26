# Collision

Status: evidence recorded

See [collision/query evidence](../evidence/functions/collision.md) for the
static call graph, tentative query-record layout, and grounded/airborne
runtime probes.

The evidence directory also contains a small C++20 reference layer and
compile-only fixture for the recovered fixed-point query math, model-face
walker, face filtering, and zone-grid traversal. The full level-file loader
and engine-owned cache allocation remain outside this layer.

The current strongest model is a swept-line query over a level zone/block
structure, with a linked transformed-model branch for dynamic objects. Exact
level-file serialization and several field meanings remain open, but the
query-record ABI, face geometry path, zone DDA, hit interpolation, and normal
finalization boundary are now constrained enough to serve as a native C++
reconstruction boundary.
