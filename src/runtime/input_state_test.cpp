#include "input_state.hpp"
#include "ollie.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    opentony::runtime::InputState state;
    state.begin_frame(0);
    assert(state.movement(opentony::runtime::MovementAction::Left).inactive_frames == 1);
    assert(state.movement(opentony::runtime::MovementAction::Left).updates == 1);

    state.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left));
    const auto& first = state.movement(opentony::runtime::MovementAction::Left);
    assert(first.held);
    assert(first.pressed);
    assert(first.press_latched);
    assert(first.activation_seen);
    assert(!first.released);
    assert(first.held_frames == 1);
    assert(first.frames_since_press == 0);

    state.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left));
    const auto& held = state.movement(opentony::runtime::MovementAction::Left);
    assert(held.held);
    assert(!held.pressed);
    assert(held.held_frames == 2);
    assert(held.frames_since_press == 1);

    state.begin_frame(0);
    const auto& released = state.movement(opentony::runtime::MovementAction::Left);
    assert(!released.held);
    assert(released.released);
    assert(released.inactive_frames == 1);
    assert(released.press_latched);
    state.clear_press_latch(opentony::runtime::MovementAction::Left);
    assert(!state.movement(opentony::runtime::MovementAction::Left).press_latched);
    assert(opentony::runtime::movement_bit(opentony::runtime::MovementAction::Right) == 0x2000);
    assert(opentony::runtime::movement_bit(opentony::runtime::MovementAction::Up) == 0x1000);
    assert(opentony::runtime::movement_bit(opentony::runtime::MovementAction::Down) == 0x4000);

    state.begin_frame(0, static_cast<std::int8_t>(-0x29), 0);
    assert(state.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    assert(state.pressed(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    state.begin_frame(0, static_cast<std::int8_t>(-0x28), 0);
    assert(!state.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    state.begin_frame(0, static_cast<std::int8_t>(0x29), 0);
    assert(state.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Right)));
    assert(state.horizontal_axis() == 0x29);
    assert(state.vertical_axis() == 0);
    assert(state.history(0).action_mask == 0);
    assert(state.history(0).effective_movement_mask == 0x2000);
    assert(state.history(0).horizontal_axis == 0x29);
    assert(state.history(0).vertical_axis == 0);
    assert(state.history(1).effective_movement_mask == 0);
    state.begin_frame(0x0200);
    assert(state.history(0).action_mask == 0x0200);
    assert(state.history(1).effective_movement_mask == 0x2000);

    state.begin_frame(opentony::runtime::kKickActionBit |
                      opentony::runtime::kJumpActionBit);
    const auto& kick = state.action(opentony::runtime::kKickActionBit);
    assert(kick.held);
    assert(kick.pressed);
    assert(kick.press_latched);
    assert(kick.held_frames == 1);
    state.begin_frame(opentony::runtime::kKickActionBit |
                      opentony::runtime::kJumpActionBit);
    assert(state.action(opentony::runtime::kKickActionBit).held_frames == 2);
    state.begin_frame(0);
    assert(state.action(opentony::runtime::kKickActionBit).released);
    assert(state.action(opentony::runtime::kKickActionBit).press_latched);
    state.clear_action_latch(opentony::runtime::kKickActionBit);
    assert(!state.action(opentony::runtime::kKickActionBit).press_latched);
    std::cout << "Input state tests passed\n";
}
