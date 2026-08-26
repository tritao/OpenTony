#include "ground_animation.hpp"

#include <algorithm>
#include <cstdint>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int16_t target_for_turn(
    std::int32_t turn,
    std::int32_t limit) noexcept {
    const std::int64_t magnitude = turn < 0
        ? -static_cast<std::int64_t>(turn)
        : static_cast<std::int64_t>(turn);
    const std::int64_t raw = (magnitude * 0x16) / limit;
    return static_cast<std::int16_t>(std::min<std::int64_t>(raw, 0x16));
}

[[nodiscard]] bool ground_state(std::uint16_t state) noexcept {
    return state == 0 || (state >= 6 && state <= 10);
}

[[nodiscard]] bool special_state(std::uint16_t state) noexcept {
    return state >= 6 && state <= 10;
}

} // namespace

GroundAnimationResult update_ground_animation(
    const GroundAnimationInput& input) noexcept {
    GroundAnimationResult result{
        false,
        false,
        input.animation_state,
        input.animation_frame,
        input.animation_frame,
        GroundAnimationBranch::None,
    };
    if (input.turn_mirror == 0) {
        return result;
    }

    const std::int32_t limit =
        input.wide_turn_profile || input.vertical_lean > 0x1e
        ? 0x5a000
        : 0x2d000;

    if (!input.blocked_or_special && ground_state(input.animation_state)) {
        const std::int16_t target = target_for_turn(input.turn_mirror, limit);
        const bool positive_turn = input.turn_mirror >= 0;
        const std::uint16_t desired_state = input.alternate_mode
            ? (positive_turn ? 6 : 7)
            : (positive_turn ? 7 : 6);
        std::int16_t current_frame = input.animation_frame;
        if (input.animation_state != desired_state) {
            current_frame = 0;
        }
        result.animation_state = desired_state;
        result.animation_frame = approach_animation_frame(
            current_frame,
            target);
        result.target_frame = target;
        result.changed = result.animation_state != input.animation_state
            || result.animation_frame != input.animation_frame;
        result.completed = result.animation_frame == target;
        result.branch = GroundAnimationBranch::GroundTurn;
        return result;
    }

    if (input.blocked_or_special && special_state(input.animation_state)) {
        const bool negative_turn = input.turn_mirror < 0;
        const bool state_nine = input.alternate_mode
            ? !negative_turn
            : negative_turn;
        const std::uint16_t desired_state = state_nine ? 9 : 10;
        const std::int16_t target = static_cast<std::int16_t>(
            state_nine ? 0xf : 0xc);
        std::int16_t current_frame = input.animation_frame;
        if (input.animation_state != desired_state) {
            current_frame = 0;
        }
        result.animation_state = desired_state;
        result.animation_frame = approach_animation_frame(
            current_frame,
            target);
        result.target_frame = target;
        result.changed = result.animation_state != input.animation_state
            || result.animation_frame != input.animation_frame;
        result.completed = result.animation_frame == target;
        result.branch = GroundAnimationBranch::SpecialTurn;
        return result;
    }

    return result;
}

} // namespace opentony::runtime
