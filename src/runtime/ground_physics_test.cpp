#include "ground_physics.hpp"

#include <cassert>
#include <iostream>

using opentony::runtime::FixedPosition;
using opentony::runtime::GroundPhysicsAction;
using opentony::runtime::GroundPhysicsInput;
using opentony::runtime::update_ground_physics;

int main() {
    const auto non_ground = update_ground_physics(GroundPhysicsInput{
        FixedPosition{100, 0, 0},
        3,
        1,
        -0x1000,
        0,
        0,
        0,
        0,
        true,
        false,
        false,
        false,
        false,
    });
    assert(non_ground.ground_update_state == 0);
    assert(non_ground.action == GroundPhysicsAction::ResetForNonGround);
    assert(!non_ground.physics_state_requested);

    const auto state7_locked = update_ground_physics(GroundPhysicsInput{
        {},
        7,
        4,
        -0x1000,
        0,
        0,
        0,
        0,
        true,
        false,
        true,
        false,
        false,
    });
    assert(state7_locked.ground_update_state == 0);
    assert(state7_locked.physics_state_requested);
    assert(state7_locked.requested_physics_state == 0);
    assert(state7_locked.requested_physics_reason == 0x2c0f);

    const auto state7_surface = update_ground_physics(GroundPhysicsInput{
        {},
        7,
        4,
        -0x1000,
        0,
        0,
        0,
        0,
        false,
        false,
        false,
        false,
        false,
    });
    assert(state7_surface.physics_state_requested);
    assert(state7_surface.requested_physics_reason == 0x2c21);
    assert(state7_surface.ground_update_state == 0);
    assert(state7_surface.action == GroundPhysicsAction::ResetToIdleMode);

    const auto high_speed = update_ground_physics(GroundPhysicsInput{
        FixedPosition{0x80000, 0, 0},
        0,
        0,
        -0x1000,
        0,
        0,
        0,
        0,
        true,
        false,
        false,
        false,
        false,
    });
    assert(high_speed.response == FixedPosition({0x78000, 0, 0}));
    assert(high_speed.ground_update_state == 1);
    assert(high_speed.action == GroundPhysicsAction::EnterHighSpeedMode);
    assert(high_speed.cooldown_written);
    assert(high_speed.cooldown_value == 2);

    const auto low_speed = update_ground_physics(GroundPhysicsInput{
        FixedPosition{0x10000, 0, 0},
        0,
        0,
        -0x1000,
        0,
        0,
        0,
        0,
        true,
        false,
        false,
        false,
        false,
    });
    assert(low_speed.ground_update_state == 5);
    assert(low_speed.action == GroundPhysicsAction::EnterLowSpeedMode);

    const auto advance_three = update_ground_physics(GroundPhysicsInput{
        {},
        7,
        1,
        -0x1000,
        0,
        0,
        0,
        0,
        true,
        false,
        false,
        false,
        false,
    });
    assert(advance_three.ground_update_state == 3);
    assert(advance_three.action == GroundPhysicsAction::AdvanceToAnimationMode);
    assert(advance_three.cooldown_written);

    const auto advance_four = update_ground_physics(GroundPhysicsInput{
        {},
        7,
        3,
        -0x1000,
        0,
        0,
        0,
        0,
        true,
        false,
        false,
        false,
        true,
    });
    assert(advance_four.ground_update_state == 4);
    assert(advance_four.action == GroundPhysicsAction::AdvanceToAnimationComplete);

    std::cout << "ground physics tests passed\n";
}
