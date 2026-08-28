#include "velocity_projection.hpp"

namespace opentony::runtime {
VelocityProjectionResult project_velocity_preserving_magnitude(
    const FixedPosition& velocity,
    const FixedPosition& collision_normal) {
    VelocityProjectionResult result;
    // FUN_004f5f90 scales the squared sum by 1/0x1000 before the integer
    // square root. The older local helper took the root of the raw squared
    // integer sum, which preserved direction but changed the retail ratio.
    result.original_magnitude_q12 = retail_vector_magnitude_q12(velocity);
    result.velocity = velocity;
    static_cast<void>(remove_normal_component(
        result.velocity,
        collision_normal));
    result.projected_magnitude_q12 = retail_vector_magnitude_q12(
        result.velocity);

    // Retail forms both scales as (magnitude << 6) >> 8 before multiplying
    // and dividing the projected vector. For non-negative magnitudes this is
    // exactly integer division by four, with truncation toward zero.
    const std::int32_t original_scale = result.original_magnitude_q12 / 4;
    const std::int32_t projected_scale = result.projected_magnitude_q12 / 4;
    if (projected_scale > 0) {
        for (std::size_t index = 0; index < result.velocity.size(); ++index) {
            result.velocity[index] = static_cast<std::int32_t>(
                (static_cast<std::int64_t>(result.velocity[index])
                    * original_scale)
                / projected_scale);
        }
        result.rescaled = true;
    }
    return result;
}

} // namespace opentony::runtime
