#include "velocity_projection.hpp"

#include <cstdint>
#include <limits>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::uint64_t integer_sqrt(std::uint64_t value) noexcept {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62U;
    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result;
}

[[nodiscard]] std::int32_t magnitude_q12(
    const FixedPosition& vector) noexcept {
    const auto square = [](std::int32_t value) {
        const std::int64_t wide = value;
        return static_cast<std::uint64_t>(wide * wide);
    };
    const std::uint64_t sum = square(vector[0])
        + square(vector[1])
        + square(vector[2]);
    const std::uint64_t root = integer_sqrt(sum);
    return root > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
        ? std::numeric_limits<std::int32_t>::max()
        : static_cast<std::int32_t>(root);
}

} // namespace

VelocityProjectionResult project_velocity_preserving_magnitude(
    const FixedPosition& velocity,
    const FixedPosition& collision_normal) {
    VelocityProjectionResult result;
    result.original_magnitude_q12 = magnitude_q12(velocity);
    result.velocity = velocity;
    static_cast<void>(remove_normal_component(
        result.velocity,
        collision_normal));
    result.projected_magnitude_q12 = magnitude_q12(result.velocity);

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
