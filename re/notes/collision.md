# Collision

Status: evidence recorded

See [collision/query evidence](../evidence/functions/collision.md) for the
static call graph, tentative query-record layout, and grounded/airborne
runtime probes.

The evidence directory also contains a small C++20 reference layer and
compile-only fixture for the recovered fixed-point query math. It intentionally
stops before the unresolved level-zone/model implementation.

The current strongest model is a swept-line query over a level zone/block
structure. Exact level-file serialization and several field meanings remain
open; this session deliberately stopped at the query interface and hit result.
