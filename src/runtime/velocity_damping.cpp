#include "velocity_damping.hpp"

#include "fixed_math.hpp"

#include <algorithm>
#include <limits>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t saturating_i32(std::int64_t value) noexcept {
    if (value > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t trunc_shift(
    std::int32_t value,
    unsigned shift) noexcept {
    if (value >= 0) {
        return value >> shift;
    }
    const std::int64_t magnitude = -static_cast<std::int64_t>(value);
    return -static_cast<std::int32_t>(magnitude >> shift);
}

[[nodiscard]] std::int32_t speed_metric(
    std::int32_t magnitude) noexcept {
    return saturating_i32(static_cast<std::int64_t>(magnitude) * 0x40);
}

[[nodiscard]] std::int32_t random_threshold(
    std::int32_t roll,
    std::int32_t offset) noexcept {
    return saturating_i32(
        (static_cast<std::int64_t>(roll) + offset) * 0x2d000 / 0x118);
}

} // namespace

VelocityDampingResult VelocityDamping::apply(
    const VelocityDampingInput& input) noexcept {
    VelocityDampingResult result;
    result.velocity = input.velocity;
    result.magnitude_q12 = input.magnitude_q12 >= 0
        ? input.magnitude_q12
        : retail_vector_magnitude_q12(result.velocity);
    result.speed_metric = speed_metric(result.magnitude_q12);

    const std::int32_t rescale_limit =
        random_threshold(input.rescale_roll, 500);
    if (rescale_limit < result.speed_metric &&
        (static_cast<std::uint32_t>(result.speed_metric) & 0xfffff000U) != 0U) {
        const std::int32_t denominator = result.speed_metric >> 12;
        const std::int32_t target = rescale_limit >> 12;
        if (denominator != 0) {
            for (std::size_t index = 0; index < result.velocity.size(); ++index) {
                result.velocity[index] = saturating_i32(
                    static_cast<std::int64_t>(target) * result.velocity[index]
                    / denominator);
            }
            result.rescaled = true;
            result.magnitude_q12 = retail_vector_magnitude_q12(result.velocity);
            result.speed_metric = speed_metric(result.magnitude_q12);
        }
    }

    const std::int32_t decay_limit =
        random_threshold(input.decay_roll, 0x186);
    if (decay_limit < result.speed_metric) {
        for (std::size_t index = 0; index < result.velocity.size(); ++index) {
            const std::int32_t random_component =
                input.decay_component_outputs_available
                ? input.decay_component_outputs[index]
                : saturating_i32(
                    static_cast<std::int64_t>(100)
                    * result.velocity[index] / 0x1000);
            result.velocity[index] = saturating_i32(
                static_cast<std::int64_t>(result.velocity[index]) -
                (static_cast<std::int64_t>(random_component)
                 * input.frame_scale_q8 >> 8));
        }
        result.randomized_decay = true;
    }

    if (input.apply_idle_decay) {
        const std::int32_t speed = result.speed_metric < 0
            ? -result.speed_metric
            : result.speed_metric;
        if (speed < 0x10000) {
            for (std::int32_t& value : result.velocity) {
                value = saturating_i32(
                    static_cast<std::int64_t>(value) - trunc_shift(value, 5));
            }
            result.fine_decay = true;
        }
        if (speed < 0x2000) {
            for (std::int32_t& value : result.velocity) {
                value = saturating_i32(
                    static_cast<std::int64_t>(value) - trunc_shift(value, 2));
                if (value < 0x10 && value > -0x10) {
                    value = 0;
                }
            }
            result.coarse_decay = true;
        }
    }
    return result;
}

} // namespace opentony::runtime
