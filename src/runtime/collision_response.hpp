#pragma once

#include "fixed_math.hpp"

namespace opentony::runtime {

struct CollisionResponseResult {
    std::int32_t dot{};
    bool adjusted{};
};

struct CollisionOrientationResult {
    std::int32_t forward_dot{};
    std::int32_t lateral_dot{};
    std::int32_t yaw_angle{};
    bool adjusted{};

    friend bool operator==(
        const CollisionOrientationResult&,
        const CollisionOrientationResult&) = default;
};

// Response stages recovered from FUN_0049bad0. If the response vector is
// moving into the supplied surface delta, retail removes that projection and
// adds the observed Q12 bias 0xcd along the same delta. The later orientation
// rewrite is exposed separately because its yaw value is caller-selected.
[[nodiscard]] CollisionResponseResult apply_inward_response(
    FixedPosition& response,
    const FixedPosition& surface_delta,
    std::int32_t bias_q12 = 0xcd);

} // namespace opentony::runtime
