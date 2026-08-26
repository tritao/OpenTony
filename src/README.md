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
32-bit PC pointers. The dynamic branch's transformed-vertex preprocessing is
also exposed, while its unresolved floating-point precision helper remains
documented as a separate gap.

The standalone checks can be run with:

```text
g++ -std=c++20 -Wall -Wextra -Werror -pedantic -I. \
  src/collision/psx_scene_test.cpp -o /tmp/psx-scene-test
/tmp/psx-scene-test /path/to/SKHAN.PSX
```
