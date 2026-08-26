#pragma once

#include <cstdint>

namespace opentony::runtime {

struct GroundMotionThresholdInput final {
    std::int32_t random_roll{}; // FUN_0048f3a0(3)
    bool blocked_or_special{}; // skater +0x2dd8 != 0
};

struct GroundMotionThresholdResult final {
    std::int32_t previous_threshold{};
    std::int32_t threshold{};
    std::int32_t sampled_target{};
    bool changed{};
};

// Post-dispatch +0x2dc8 writer from FUN_0049e680. The random/stat source is
// deliberately an input because the shared retail RNG is not yet modeled.
[[nodiscard]] GroundMotionThresholdResult update_ground_motion_threshold(
    std::int32_t current_threshold,
    const GroundMotionThresholdInput& input) noexcept;

} // namespace opentony::runtime
