#include "input_state.hpp"

namespace opentony::runtime {
namespace {

[[nodiscard]] std::size_t movement_index(MovementAction action) noexcept {
    return static_cast<std::size_t>(action);
}

} // namespace

void InputState::begin_frame(
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis) noexcept {
    previous_action_mask_ = action_mask_;
    previous_effective_movement_mask_ = effective_movement_mask_;
    action_mask_ = action_mask;
    horizontal_axis_ = horizontal_axis;
    vertical_axis_ = vertical_axis;
    effective_movement_mask_ = action_mask & 0xf000U;
    if ((effective_movement_mask_ & movement_bit(MovementAction::Left)) == 0
        && horizontal_axis <= -0x29) {
        effective_movement_mask_ |= movement_bit(MovementAction::Left);
    }
    if ((effective_movement_mask_ & movement_bit(MovementAction::Right)) == 0
        && horizontal_axis >= 0x29) {
        effective_movement_mask_ |= movement_bit(MovementAction::Right);
    }
    if ((effective_movement_mask_ & movement_bit(MovementAction::Up)) == 0
        && vertical_axis <= -0x29) {
        effective_movement_mask_ |= movement_bit(MovementAction::Up);
    }
    if ((effective_movement_mask_ & movement_bit(MovementAction::Down)) == 0
        && vertical_axis >= 0x29) {
        effective_movement_mask_ |= movement_bit(MovementAction::Down);
    }
    for (std::size_t index = kHistoryDepth - 1; index > 0; --index) {
        history_[index] = history_[index - 1];
    }
    history_[0] = InputFrameSnapshot{
        action_mask_,
        effective_movement_mask_,
        horizontal_axis_,
        vertical_axis_,
    };
    for (std::size_t index = 0; index < 4; ++index) {
        const MovementAction action = static_cast<MovementAction>(index);
        ActionTransition& transition = movement_[index];
        const std::uint16_t bit = movement_bit(action);
        const bool previously_held = (previous_effective_movement_mask_ & bit) != 0;
        transition.held = (effective_movement_mask_ & bit) != 0;
        transition.pressed = transition.held && !previously_held;
        transition.released = !transition.held && previously_held;
        if (transition.pressed) {
            transition.press_latched = true;
            transition.activation_seen = true;
            transition.frames_since_press = 0;
        } else {
            ++transition.frames_since_press;
        }
        transition.held_frames = transition.held ? transition.held_frames + 1U : 0U;
        transition.inactive_frames = transition.held ? 0U : transition.inactive_frames + 1U;
        ++transition.updates;
    }
    for (std::size_t index = 0; index < 16; ++index) {
        const std::uint16_t bit = static_cast<std::uint16_t>(1U << index);
        ActionTransition& transition = actions_[index];
        const bool previously_held = (previous_action_mask_ & bit) != 0;
        transition.held = (action_mask_ & bit) != 0;
        transition.pressed = transition.held && !previously_held;
        transition.released = !transition.held && previously_held;
        if (transition.pressed) {
            transition.press_latched = true;
            transition.activation_seen = true;
            transition.frames_since_press = 0;
        } else {
            ++transition.frames_since_press;
        }
        transition.held_frames = transition.held ? transition.held_frames + 1U : 0U;
        transition.inactive_frames = transition.held ? 0U : transition.inactive_frames + 1U;
        ++transition.updates;
    }
}

const InputFrameSnapshot& InputState::history(std::size_t frames_ago) const noexcept {
    static const InputFrameSnapshot empty{};
    return frames_ago < kHistoryDepth ? history_[frames_ago] : empty;
}

std::uint16_t InputState::effective_mask(std::uint16_t bit) const noexcept {
    if ((bit & 0xf000U) != 0) {
        return effective_movement_mask_;
    }
    return action_mask_;
}

const ActionTransition& InputState::movement(MovementAction action) const noexcept {
    return movement_[movement_index(action)];
}

const ActionTransition& InputState::action(std::uint16_t bit) const noexcept {
    static const ActionTransition empty{};
    if (bit == 0 || (bit & static_cast<std::uint16_t>(bit - 1U)) != 0) {
        return empty;
    }
    std::size_t index = 0;
    std::uint16_t value = bit;
    while (value > 1) {
        value = static_cast<std::uint16_t>(value >> 1U);
        ++index;
    }
    return actions_[index];
}

void InputState::clear_press_latch(MovementAction action) noexcept {
    movement_[movement_index(action)].press_latched = false;
}

void InputState::clear_action_latch(std::uint16_t bit) noexcept {
    if (bit == 0 || (bit & static_cast<std::uint16_t>(bit - 1U)) != 0) {
        return;
    }
    std::size_t index = 0;
    std::uint16_t value = bit;
    while (value > 1) {
        value = static_cast<std::uint16_t>(value >> 1U);
        ++index;
    }
    actions_[index].press_latched = false;
}

} // namespace opentony::runtime
