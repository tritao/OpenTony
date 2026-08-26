#include "ground_motion.hpp"
#include "player_state.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::GroundMotionBranch;
    using opentony::runtime::GroundMotionInput;
    using opentony::runtime::PlayerState;

    PlayerState player;
    player.set_collision_response({0x1000, 0, 0});
    assert(player.ground_motion_speed_metric() == 0x1000);
    player.set_collision_response({});
    GroundMotionInput ordinary{};
    ordinary.producer_enabled = true;
    ordinary.ordinary_ground_state = true;
    ordinary.response_speed_metric = 0x6000;
    ordinary.response_speed_threshold = 0x10000;
    ordinary.forward_basis_y = 0;
    const auto ordinary_result = player.apply_ground_motion(ordinary);
    assert(ordinary_result.applied);
    assert(ordinary_result.branch == GroundMotionBranch::Ordinary);
    assert(ordinary_result.scale == 1);
    assert(player.motion_correction() == FixedPosition({0, 0, -0x1000}));

    GroundMotionInput animation = ordinary;
    animation.animation_state = 3;
    animation.animation_frame = 11;
    animation.strong_profile = true;
    const auto animation_result = player.apply_ground_motion(animation);
    assert(animation_result.applied);
    assert(animation_result.branch == GroundMotionBranch::Animation2Or3);
    assert(animation_result.scale == 8);
    assert(player.motion_correction() == FixedPosition({0, 0, -0x8000}));

    GroundMotionInput blocked = ordinary;
    blocked.physics_locked = true;
    const auto blocked_result = player.apply_ground_motion(blocked);
    assert(!blocked_result.applied);
    assert(player.motion_correction() == FixedPosition({0, 0, -0x8000}));

    std::cout << "Ground motion tests passed\n";
}
