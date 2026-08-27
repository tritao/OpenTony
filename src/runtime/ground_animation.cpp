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

void record_range_request(
    GroundAnimationResult& result,
    std::uint16_t animation,
    std::int16_t frame,
    bool completion_check) noexcept {
    result.request = GroundAnimationRequest{
        true,
        GroundAnimationRequestWrapper::Range,
        animation,
        frame,
        frame,
        -1,
        true,
        completion_check,
    };
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
        {},
    };
    const std::int32_t limit =
        input.wide_turn_profile || input.vertical_lean > 0x1e
        ? 0x5a000
        : 0x2d000;

    if (input.turn_mirror != 0
        && !input.blocked_or_special
        && ground_state(input.animation_state)) {
        const std::int16_t target = target_for_turn(input.turn_mirror, limit);
        const bool positive_turn = input.turn_mirror >= 0;
        const std::uint16_t desired_state = input.alternate_mode
            ? (positive_turn ? 6 : 7)
            : (positive_turn ? 7 : 6);
        std::int16_t current_frame = input.animation_frame;
        if (input.animation_state != desired_state) {
            current_frame = 0;
        }
        // The alternate positive branch has one extra pre-approach increment
        // at 0x004931d1. It is observable only when the seated frame is below
        // the target; the following approach helper still applies 5/3/1.
        if (input.alternate_mode && positive_turn && current_frame < target) {
            ++current_frame;
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
        record_range_request(
            result,
            desired_state,
            result.animation_frame,
            false);
        return result;
    }

    if (input.turn_mirror != 0
        && input.blocked_or_special
        && special_state(input.animation_state)) {
        const bool negative_turn = input.turn_mirror < 0;
        const bool state_nine = input.alternate_mode
            ? !negative_turn
            : negative_turn;
        const std::uint16_t desired_state = state_nine ? 9 : 10;
        const std::int16_t target = static_cast<std::int16_t>(
            state_nine ? 0xf : 0xc);
        std::int16_t current_frame = input.animation_frame;
        // When the selector changes the ordinary turn state into its
        // crouched counterpart, retail seats the target frame before calling
        // the approach helper. This makes the first request complete at once.
        const std::uint16_t source_state = state_nine ? 6 : 7;
        if (input.animation_state == source_state) {
            current_frame = target;
        } else if (input.animation_state != desired_state) {
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
        record_range_request(
            result,
            desired_state,
            result.animation_frame,
            result.completed);
        return result;
    }

    // The selector's final stage is a release transition. It is reached even
    // when the transition gate is set, because both turn branches above are
    // gated by a nonzero steering mirror.
    if (input.turn_mirror == 0) {
        if (input.animation_state == 6 || input.animation_state == 7) {
            result.animation_state = 0;
            result.animation_frame = 0;
            result.changed = input.animation_state != 0
                || input.animation_frame != 0;
            result.request = GroundAnimationRequest{
                true,
                GroundAnimationRequestWrapper::Start,
                0,
                0,
                -1,
                -1,
                true,
                false,
            };
        } else if (input.animation_state == 9
                   || input.animation_state == 10) {
            result.animation_state = 8;
            result.animation_frame = 0x13;
            result.changed = input.animation_state != 8
                || input.animation_frame != 0x13;
            result.request = GroundAnimationRequest{
                true,
                GroundAnimationRequestWrapper::Full,
                8,
                0x13,
                0x1a,
                0x13,
                true,
                false,
            };
        }
    }

    return result;
}

AnimationRequestResult apply_ground_animation_request(
    AnimationCursor& cursor,
    AnimationTableView table,
    const GroundAnimationRequest& request) noexcept {
    if (!request.issued) {
        return {};
    }
    if (request.resets_rate) {
        cursor.rate = 0x10000;
    }
    return cursor.request(
        table,
        request.animation,
        request.start,
        request.end,
        request.alternate);
}

} // namespace opentony::runtime
