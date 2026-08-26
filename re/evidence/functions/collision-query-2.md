# Collision query helper

Status: observed

`0x004f4240` now has an exact VC6 reconstruction. It records the query
half-extents, rejects inverted or negative boxes, invokes the shared plane
test for the initial axis, and follows the original short-circuit axis paths
before returning the signed overlap result.

The packed global writes, near/short branch layout, and cleanup paths match
under `vc6-coff-text`.
