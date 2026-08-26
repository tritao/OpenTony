#include "collision_response.hpp"

namespace opentony::runtime {

CollisionResponseResult apply_inward_response(
    FixedPosition& response,
    const FixedPosition& surface_delta,
    std::int32_t bias_q12) {
    const std::int32_t dot = fixed_dot_q12(response, surface_delta);
    if (dot >= 0) {
        return CollisionResponseResult{dot, false};
    }
    for (std::size_t index = 0; index < response.size(); ++index) {
        const std::int32_t removed = fixed_multiply_q12(
            surface_delta[index],
            dot);
        const std::int32_t bias = fixed_multiply_q12(
            surface_delta[index],
            bias_q12);
        response[index] = static_cast<std::int32_t>(
            static_cast<std::int64_t>(response[index])
            - removed
            + bias);
    }
    return CollisionResponseResult{dot, true};
}

} // namespace opentony::runtime
