#include "ground_motion_threshold.hpp"

#include <cstdint>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t sample_target(
    std::int32_t roll,
    std::int32_t offset) noexcept {
    return static_cast<std::int32_t>(
        (static_cast<std::int64_t>(roll) + offset) * 0x2d000 / 0x118);
}

} // namespace

GroundMotionThresholdResult update_ground_motion_threshold(
    std::int32_t current_threshold,
    const GroundMotionThresholdInput& input) noexcept {
    GroundMotionThresholdResult result{
        current_threshold,
        current_threshold,
        0,
        false,
    };
    if (input.blocked_or_special) {
        result.sampled_target = sample_target(input.random_roll, 0xdc);
        result.threshold = result.sampled_target;
        result.changed = result.threshold != result.previous_threshold;
        return result;
    }

    result.sampled_target = sample_target(input.random_roll, 0xaa);
    if (result.sampled_target < current_threshold) {
        result.threshold = current_threshold - 1;
        result.changed = true;
    }
    return result;
}

} // namespace opentony::runtime
