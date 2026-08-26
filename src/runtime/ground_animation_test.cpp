#include "ground_animation.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    using opentony::runtime::GroundAnimationBranch;
    using opentony::runtime::GroundAnimationInput;
    using opentony::runtime::approach_animation_frame;
    using opentony::runtime::update_ground_animation;

    CHECK(approach_animation_frame(0, 22) == 5);
    CHECK(approach_animation_frame(10, 22) == 13);
    CHECK(approach_animation_frame(19, 22) == 20);
    CHECK(approach_animation_frame(21, 22) == 22);
    CHECK(approach_animation_frame(22, 0) == 17);

    GroundAnimationInput ground{};
    ground.turn_mirror = 0x2d000;
    const auto first = update_ground_animation(ground);
    CHECK(first.changed);
    CHECK(first.branch == GroundAnimationBranch::GroundTurn);
    CHECK(first.animation_state == 7);
    CHECK(first.target_frame == 22);
    CHECK(first.animation_frame == 5);

    ground.animation_state = first.animation_state;
    ground.animation_frame = first.animation_frame;
    const auto second = update_ground_animation(ground);
    CHECK(second.animation_frame == 10);

    GroundAnimationInput wide{};
    wide.turn_mirror = 0x2d000;
    wide.wide_turn_profile = true;
    const auto wide_result = update_ground_animation(wide);
    CHECK(wide_result.target_frame == 11);

    GroundAnimationInput special{};
    special.turn_mirror = -0x2d000;
    special.blocked_or_special = true;
    special.animation_state = 6;
    const auto special_result = update_ground_animation(special);
    CHECK(special_result.branch == GroundAnimationBranch::SpecialTurn);
    CHECK(special_result.animation_state == 9);
    CHECK(special_result.target_frame == 15);
    CHECK(special_result.animation_frame == 5);

    std::cout << "Ground animation tests passed\n";
}
