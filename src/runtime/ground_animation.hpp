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

// The selector reaches one of the four fixed-arity request wrappers after it
// has chosen/seated the next pose. Keep the wrapper's integer arguments here;
// RunAnim then performs the ordinary cursor-field initialization.
enum class GroundAnimationRequestWrapper : std::uint8_t {
    None,
    Start, // FUN_004903f0 / (anim, start, -1, -1)
    Range, // FUN_00490450 / (anim, start, end, -1)
    Full,  // FUN_00490480 / (anim, start, end, alternate)
};

struct GroundAnimationRequest final {
    bool issued = false;
    GroundAnimationRequestWrapper wrapper = GroundAnimationRequestWrapper::None;
    std::uint16_t animation{};
    std::int32_t start = -1;
    std::int32_t end = -1;
    std::int32_t alternate = -1;
    // Every selector wrapper writes +0x108 before entering RunAnim.
    bool resets_rate = false;
    // Special-turn completion invokes FUN_00496280 after RunAnim when the
    // selected pose reaches its target. The service itself stays caller-owned.
    bool completion_check = false;
};

struct GroundAnimationResult final {
    bool changed = false;
    bool completed = false;
    std::uint16_t animation_state{};
    std::int16_t animation_frame{};
    std::int16_t target_frame{};
    GroundAnimationBranch branch = GroundAnimationBranch::None;
    GroundAnimationRequest request{};
};

// State/frame and request reconstruction of FUN_00492f20. Animation asset
// lookup remains in AnimationCursor/asset-runtime; FUN_004902e0's unrelated
// skater preflight and FUN_00496280's velocity/event service remain seams.
[[nodiscard]] GroundAnimationResult update_ground_animation(
    const GroundAnimationInput& input) noexcept;

// Apply one selector-produced request through the same wrapper contract:
// reset +0x108 to 0x10000, then enter RunAnim with the recorded arguments.
[[nodiscard]] AnimationRequestResult apply_ground_animation_request(
    AnimationCursor& cursor,
    AnimationTableView table,
    const GroundAnimationRequest& request) noexcept;

} // namespace opentony::runtime
