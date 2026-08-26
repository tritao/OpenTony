#pragma once

#include "fixed_math.hpp"

#include <cstdint>

namespace opentony::runtime {

struct VelocityProjectionResult {
    FixedPosition velocity{};
    std::int32_t original_magnitude_q12{};
    std::int32_t projected_magnitude_q12{};
    bool rescaled{};

    friend bool operator==(
        const VelocityProjectionResult&,
        const VelocityProjectionResult&) = default;
};

// Reconstructs retail FUN_00490680. It removes the component parallel to the
// supplied collision normal with FUN_00490610, then restores the original
// vector magnitude when the projected vector is large enough to normalize.
// The caller decides which collision/state branches should use this helper.
[[nodiscard]] VelocityProjectionResult project_velocity_preserving_magnitude(
    const FixedPosition& velocity,
    const FixedPosition& collision_normal);

} // namespace opentony::runtime
