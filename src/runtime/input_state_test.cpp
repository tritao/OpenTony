#include "input_state.hpp"
#include "ollie.hpp"

#include "tests/test_check.hpp"
#include <cstdint>
#include <iostream>

int main() {
    opentony::runtime::InputState state;
    state.begin_frame(0);
    CHECK(state.movement(opentony::runtime::MovementAction::Left).inactive_frames == 1);
    CHECK(state.movement(opentony::runtime::MovementAction::Left).updates == 1);

    state.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left));
    const auto& first = state.movement(opentony::runtime::MovementAction::Left);
    CHECK(first.held);
    CHECK(first.pressed);
    CHECK(first.press_latched);
    CHECK(first.activation_seen);
    CHECK(!first.released);
    CHECK(first.held_frames == 1);
    CHECK(first.frames_since_press == 0);

    state.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left));
    const auto& held = state.movement(opentony::runtime::MovementAction::Left);
    CHECK(held.held);
    CHECK(!held.pressed);
    CHECK(held.held_frames == 2);
    CHECK(held.frames_since_press == 1);

    state.begin_frame(0);
    const auto& released = state.movement(opentony::runtime::MovementAction::Left);
    CHECK(!released.held);
    CHECK(released.released);
    CHECK(released.inactive_frames == 1);
    CHECK(released.press_latched);
    state.clear_press_latch(opentony::runtime::MovementAction::Left);
    CHECK(!state.movement(opentony::runtime::MovementAction::Left).press_latched);
    CHECK(opentony::runtime::movement_bit(opentony::runtime::MovementAction::Right) == 0x2000);
    CHECK(opentony::runtime::movement_bit(opentony::runtime::MovementAction::Up) == 0x1000);
    CHECK(opentony::runtime::movement_bit(opentony::runtime::MovementAction::Down) == 0x4000);

    state.begin_frame(0, static_cast<std::int8_t>(-0x29), 0);
    CHECK(state.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    CHECK(state.pressed(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    state.begin_frame(0, static_cast<std::int8_t>(-0x28), 0);
    CHECK(!state.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    state.begin_frame(0, static_cast<std::int8_t>(0x29), 0);
    CHECK(state.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Right)));
    CHECK(state.horizontal_axis() == 0x29);
    CHECK(state.vertical_axis() == 0);
    CHECK(state.history(0).action_mask == 0);
    CHECK(state.history(0).effective_movement_mask == 0x2000);
    CHECK(state.history(0).horizontal_axis == 0x29);
    CHECK(state.history(0).vertical_axis == 0);
    CHECK(state.history(1).effective_movement_mask == 0);
    state.begin_frame(0x0200);
    CHECK(state.history(0).action_mask == 0x0200);
    CHECK(state.history(1).effective_movement_mask == 0x2000);

    state.begin_frame(opentony::runtime::kKickActionBit |
                      opentony::runtime::kJumpActionBit);
    const auto& kick = state.action(opentony::runtime::kKickActionBit);
    CHECK(kick.held);
    CHECK(kick.pressed);
    CHECK(kick.press_latched);
    CHECK(kick.held_frames == 1);
    state.begin_frame(opentony::runtime::kKickActionBit |
                      opentony::runtime::kJumpActionBit);
    CHECK(state.action(opentony::runtime::kKickActionBit).held_frames == 2);
    state.begin_frame(0);
    CHECK(state.action(opentony::runtime::kKickActionBit).released);
    CHECK(state.action(opentony::runtime::kKickActionBit).press_latched);
    state.clear_action_latch(opentony::runtime::kKickActionBit);
    CHECK(!state.action(opentony::runtime::kKickActionBit).press_latched);
    std::cout << "Input state tests passed\n";
}
