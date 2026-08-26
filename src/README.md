# Native reconstruction

Do not choose a graphics/runtime framework yet. The initial project is about recovering and validating game behavior. Add the native engine only after we have a small subsystem with original-game traces to compare against.

A likely first vertical slice is player movement / ollie state with no production renderer requirement.

## Ground movement reference core

The Ground movement/orientation session now has a small native reference core
in `ground_movement.hpp` / `ground_movement.cpp`. It is deliberately limited
to the recovered state-0 path: Left/Right turn accumulation, Q12 Y-matrix and
basis updates, the fixed-point position add at `0x004967b6`, and the
`0x0049f0e5`/`0x00496060` candidate boundary. Surface queries and collision
fallbacks are supplied through a resolver because their geometry policy is not
yet recovered as a single portable rule.

The core does not invent a world heading: seed `GroundState::orientation`
from the captured player matrix. The Warehouse baseline is the observed
diagonal `-4096` Q12 frame; `MatrixQ12::identity()` remains the neutral math
fixture used by the focused tests.

Build and run its focused test without selecting a runtime or graphics
framework:

```bash
cmake -S src -B build/native-ground -DBUILD_TESTING=ON
cmake --build build/native-ground
ctest --test-dir build/native-ground --output-on-failure
```
