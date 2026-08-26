# Native reconstruction

Do not choose a graphics/runtime framework yet. The initial project is about recovering and validating game behavior. Add the native engine only after we have a small subsystem with original-game traces to compare against.

A likely first vertical slice is player movement / ollie state with no production renderer requirement.

The first native subsystem is now available at
[`src/collision/psx_scene.hpp`](collision/psx_scene.hpp). It decodes the
collision-relevant portion of a version-4 PSX scene, uses its blockmap as a
broad-phase candidate index, and calls the recovered fixed-point model/face
query. `query_with_metadata` retains the raw base/surface flags alongside the
contact, normal, distance, and parameter fields. The native object and face
identifiers are stable scene IDs/source offsets; they are not fabricated
32-bit PC pointers. The dynamic branch's transformed-vertex preprocessing,
projected-face gate, candidate-distance arithmetic, and signed-short
saturation are also exposed. `query_linked_objects` accepts a native view of
the recovered PC heap-node prefix and runs the exact flag gate, object-space
broad phase, recovered transformed-model pass, and dynamic face scan; callers
provide the resolved native model index and body identity because the
original stores a type-table model pair and a live pointer in `q+0x68`. The
`query_with_linked_objects` entry point composes that branch with the PSX
zone/blockmap path using the shared traveled-distance field. The
remaining gap is the PC heap-linked object's loader ownership and
level-to-heap serialization. The collision-facing linked-node element
stride, prefix, tail extent, and broad-phase arithmetic are documented and
tested in `re/evidence/collision_reference.hpp`. The evidence layer also
models the null-terminated per-cell object-head array and the recovered
forward/backward list-link offsets; PC loader allocation and serialization
remain outside this boundary.

The standalone checks can be run with:

```text
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I. \
  src/collision/psx_scene_test.cpp -o /tmp/psx-scene-test
/tmp/psx-scene-test /path/to/SKHAN.PSX
```
