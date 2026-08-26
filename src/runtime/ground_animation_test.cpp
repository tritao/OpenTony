#include "ground_animation.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::GroundAnimationBranch;
    using opentony::runtime::GroundAnimationInput;
    using opentony::runtime::approach_animation_frame;
    using opentony::runtime::update_ground_animation;

    assert(approach_animation_frame(0, 22) == 5);
    assert(approach_animation_frame(10, 22) == 13);
    assert(approach_animation_frame(19, 22) == 20);
    assert(approach_animation_frame(21, 22) == 22);
    assert(approach_animation_frame(22, 0) == 17);

    GroundAnimationInput ground{};
    ground.turn_mirror = 0x2d000;
    const auto first = update_ground_animation(ground);
    assert(first.changed);
    assert(first.branch == GroundAnimationBranch::GroundTurn);
    assert(first.animation_state == 7);
    assert(first.target_frame == 22);
    assert(first.animation_frame == 5);

    ground.animation_state = first.animation_state;
    ground.animation_frame = first.animation_frame;
    const auto second = update_ground_animation(ground);
    assert(second.animation_frame == 10);

    GroundAnimationInput wide{};
    wide.turn_mirror = 0x2d000;
    wide.wide_turn_profile = true;
    const auto wide_result = update_ground_animation(wide);
    assert(wide_result.target_frame == 11);

    GroundAnimationInput special{};
    special.turn_mirror = -0x2d000;
    special.blocked_or_special = true;
    special.animation_state = 6;
    const auto special_result = update_ground_animation(special);
    assert(special_result.branch == GroundAnimationBranch::SpecialTurn);
    assert(special_result.animation_state == 9);
    assert(special_result.target_frame == 15);
    assert(special_result.animation_frame == 5);

    std::cout << "Ground animation tests passed\n";
}
