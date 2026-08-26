#include "position_commit.hpp"

#include <array>

namespace opentony::runtime {

PositionCommitResult PositionCommitter::commit(
    FixedPosition current,
    FixedPosition desired,
    const PositionCollisionProbe& probe,
    bool bypass_collision) {
    if (bypass_collision || !probe) {
        return PositionCommitResult{desired, false, false, 0};
    }

    // The retail order is visible in the seven successive FUN_004624d0 /
    // FUN_00466090 calls surrounding FUN_00496060:
    // desired, old-X, old-Z, old-Y, old-Y+old-Z, old-X+old-Y,
    // old-X+old-Z, and finally old position.
    const std::array<FixedPosition, 7> candidates{
        desired,
        FixedPosition{current[0], desired[1], desired[2]},
        FixedPosition{desired[0], desired[1], current[2]},
        FixedPosition{desired[0], current[1], desired[2]},
        FixedPosition{desired[0], current[1], current[2]},
        FixedPosition{current[0], current[1], desired[2]},
        FixedPosition{current[0], desired[1], current[2]},
    };
    PositionCommitResult result{};
    result.position = current;
    for (const FixedPosition& candidate : candidates) {
        ++result.probes;
        if (!probe(candidate)) {
            result.position = candidate;
            result.collided = result.probes != 1;
            return result;
        }
    }
    result.collided = true;
    result.blocked = true;
    return result;
}

} // namespace opentony::runtime
