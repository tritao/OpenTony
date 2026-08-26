# Collision

Status: asset geometry and one position-commit boundary recovered; response
semantics pending.

The PSX `0x0000000A` blockmap provides fixed-point X/Z bounds, a grid, and
object references. The native `PsxCollisionWorld` reproduces that selection,
places model vertices at `object_position + (vertex * 0x1000)` as in the
retail face-cache builder, skips faces with the converter's
observed `0x80` non-collision bit, and keeps surface flags and source indices.
It supports cell broad-phase queries and conservative segment/triangle/quad
intersections. This is geometry evidence, not yet proof of the game's exact
skater collision policy.

Retail `FUN_00496060` (`re/evidence/functions/physics.md`) is a shared position
commit path. Its decompilation performs a collision probe for the desired
position and then tries seven axis-preserving combinations in a fixed order,
finally retaining the old position if all candidates collide. The native
`PositionCommitter` preserves that control flow behind an injected probe.

The retail probe path is now better constrained. `FUN_004624d0` prepares a
fixed-point segment query from the old endpoint to the candidate endpoint,
including an endpoint AABB and a 12-bit fixed-point direction basis.
`FUN_00466090` is only a wrapper around the grid walker `FUN_004660b0`; the
walker visits the PSX `0x0000000A` blockmap cells and sends their object lists
to `FUN_004638d0`. `FUN_00462a20` then tests each triangle/quad, keeps the
nearest hit, and records the object, face, hit point, normal, surface flags,
and travel distance. `FUN_00463d50` consumes the hit and publishes the shared
normal/orientation result. The native `PsxCollisionHit` and
`PsxPositionCollisionProbe` now expose the same useful geometry boundary,
including the packed source face word at `+0x0c`;
they intentionally stop before the unresolved skater response policy.
The face-side arithmetic now also mirrors retail's preliminary `>>12`
quantization of query endpoints and face coordinates before testing, while
the winning `0..0x4000` parameter is applied to the original fixed-point
segment for contact reconstruction.

The packed face word now has a conservative `PsxCollisionMaskView`. It exposes
the exact predicates recovered around `FUN_0048ea80`: surface-word bit `6`,
the inverse predicates for surface-word bits `7` and `8`, bits `9..12` as a
four-bit raw value, and face flag `0x80`. These are carried through the native
hit record without naming them as ground, rail, trigger, or platform types.

The global face-word masks are now executable without inventing their
meanings. Retail initialization around `FUN_004660b0` (`0x00466109`) starts
`DAT_00567a60` at zero, assigns `0x400000` when `DAT_00567c84` is set, toggles
that bit when `DAT_00567c7c` is set, and toggles `0x200000` when
`DAT_00567c78` is clear. It starts `DAT_00567a68` at `0xffffffff`, changes it
to `0xffefffff` when `DAT_00567c74` is set, and toggles `0x20000` when
`DAT_00567c80` is set. `make_retail_collision_query_options` preserves those
offset-named inputs and the native face predicate applies the resulting masks;
the unresolved startup producers and the separate per-query trigger-face bit
remain caller-owned.

The native frame boundary now also has a metadata-producing query form. It
reuses the query for each `FUN_00496060` axis-fallback candidate, records the
first hit, widens the PSX Q12 normal into the runtime fixed-point type, and can
apply the confirmed `FUN_00490610` velocity projection before a caller's
surface-specific callback. This is the first executable hit-to-velocity path;
the inward bias, radius, slope, and material policies remain separate.

The shared `0x0049bad0` response is now available as a separate opt-in frame
hook. When a caller supplies the retail hit policy, it removes a negative
normal projection from `+0x4c/+0x50/+0x54` and adds the observed Q12 bias
`0xcd` along the hit delta. It is not applied to every hit by default because
the retail ground, air, and special-state callers branch differently before
this producer.

Still required for faithful physics: identify the meanings of
surface-normal/slope tests, player radius, velocity/friction, rail/platform
handling, and the exact `surface_flags` interpretation. The reusable
`VelocityProjection` helper now covers the recovered `FUN_00490680` normal
removal plus speed restoration, but its caller-specific branch selection is
still unresolved. The current segment
intersection remains conservative: it is an asset-derived occupancy query,
not a claim that every retail back-face, trigger-zone, or response filter is
fully reproduced.
