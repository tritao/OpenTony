#include "ground_physics.hpp"

#include "tests/test_check.hpp"
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
    CHECK(non_ground.ground_update_state == 0);
    CHECK(non_ground.action == GroundPhysicsAction::ResetForNonGround);
    CHECK(!non_ground.physics_state_requested);

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
    CHECK(state7_locked.ground_update_state == 0);
    CHECK(state7_locked.physics_state_requested);
    CHECK(state7_locked.requested_physics_state == 0);
    CHECK(state7_locked.requested_physics_reason == 0x2c0f);

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
    CHECK(state7_surface.physics_state_requested);
    CHECK(state7_surface.requested_physics_reason == 0x2c21);
    CHECK(state7_surface.ground_update_state == 0);
    CHECK(state7_surface.action == GroundPhysicsAction::ResetToIdleMode);

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
    CHECK(high_speed.response == FixedPosition({0x78000, 0, 0}));
    CHECK(high_speed.ground_update_state == 1);
    CHECK(high_speed.action == GroundPhysicsAction::EnterHighSpeedMode);
    CHECK(high_speed.cooldown_written);
    CHECK(high_speed.cooldown_value == 2);

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
    CHECK(low_speed.ground_update_state == 5);
    CHECK(low_speed.action == GroundPhysicsAction::EnterLowSpeedMode);

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
    CHECK(advance_three.ground_update_state == 3);
    CHECK(advance_three.action == GroundPhysicsAction::AdvanceToAnimationMode);
    CHECK(advance_three.cooldown_written);

    // FUN_0049df00 case 1 uses a strict comparison against
    // animation_frame * 0x1000 + slope_threshold. State 7 skips the
    // state-0 stop producer so this fixture isolates the mode-1 branch.
    GroundPhysicsInput mode_one_equal{
        FixedPosition{0xc000, 0, 0},
        7,
        1,
        -0x1000,
        2,
        0,
        0,
        0,
        true,
        false,
        false,
        false,
        false,
        0x100,
        0x82,
        0x1234,
    };
    const auto mode_one_equal_result =
        update_ground_physics(mode_one_equal);
    CHECK(mode_one_equal_result.speed_metric == 0xc000);
    CHECK(mode_one_equal_result.speed_threshold == 0xa000);
    CHECK(mode_one_equal_result.ground_update_state == 1);
    CHECK(!mode_one_equal_result.animation_handoff.applied);
    CHECK(mode_one_equal_result.cooldown_written);
    CHECK(mode_one_equal_result.cooldown_value == 2);

    mode_one_equal.response = FixedPosition{0xbfc0, 0, 0};
    const auto mode_one_below_result =
        update_ground_physics(mode_one_equal);
    CHECK(mode_one_below_result.speed_metric == 0xbfc0);
    CHECK(mode_one_below_result.ground_update_state == 3);
    CHECK(mode_one_below_result.action
        == GroundPhysicsAction::AdvanceToAnimationMode);
    CHECK(mode_one_below_result.animation_transition);
    CHECK(mode_one_below_result.animation_handoff.applied);
    CHECK(mode_one_below_result.animation_handoff.animation_finished == 0);
    CHECK(mode_one_below_result.animation_handoff.playback_direction == -1);
    CHECK(mode_one_below_result.animation_handoff.playback_endpoint == 0x34);
    CHECK(mode_one_below_result.animation_handoff.original_start_frame == -126);
    CHECK(mode_one_below_result.cooldown_written);
    CHECK(mode_one_below_result.cooldown_value == 2);

    // Surface failure takes the other side of the same OR predicate, still
    // performs the raw animation handoff, but leaves the audio/script service
    // disabled (animation_transition is false).
    mode_one_equal.response = FixedPosition{0x80000, 0, 0};
    mode_one_equal.surface_allows_brake = false;
    const auto mode_one_surface_result =
        update_ground_physics(mode_one_equal);
    CHECK(mode_one_surface_result.ground_update_state == 3);
    CHECK(!mode_one_surface_result.animation_transition);
    CHECK(mode_one_surface_result.animation_handoff.applied);
    CHECK(mode_one_surface_result.physics_state_requested);
    CHECK(mode_one_surface_result.requested_physics_reason == 0x2c21);

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
    CHECK(advance_four.ground_update_state == 4);
    CHECK(advance_four.action == GroundPhysicsAction::AdvanceToAnimationComplete);

    std::cout << "ground physics tests passed\n";
}
