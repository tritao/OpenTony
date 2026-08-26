#pragma once

#include "animation_cursor.hpp"

#include <cstdint>

namespace opentony::runtime {

// State inputs read by retail FUN_00492f20. The animation cursor consumes
// the resulting state/frame elsewhere; this module only owns the verified
// steering-to-frame bookkeeping.
struct GroundAnimationInput final {
    std::int32_t turn_mirror{}; // skater +0x3148
    std::int8_t vertical_lean{}; // skater +0x31a2, copied from profile +0x148
    bool wide_turn_profile{}; // profile +0xb0 selects the larger limit
    bool blocked_or_special{}; // skater +0x2dd8 != 0
    bool alternate_mode{}; // skater flags +0xd8 bit 1
    std::uint16_t animation_state{}; // skater +0xf6
    std::int16_t animation_frame{}; // skater +0xf4
};

enum class GroundAnimationBranch : std::uint8_t {
    None,
    GroundTurn,
    SpecialTurn,
};

struct GroundAnimationResult final {
    bool changed = false;
    bool completed = false;
    std::uint16_t animation_state{};
    std::int16_t animation_frame{};
    std::int16_t target_frame{};
    GroundAnimationBranch branch = GroundAnimationBranch::None;
};

// Conservative state/frame reconstruction of FUN_00492f20. It intentionally
// omits FUN_00490450's animation asset/event side effects and FUN_00496280's
// completion callback.
[[nodiscard]] GroundAnimationResult update_ground_animation(
    const GroundAnimationInput& input) noexcept;

} // namespace opentony::runtime
