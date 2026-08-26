#include "ground_motion_threshold.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    using opentony::runtime::GroundMotionThresholdInput;
    using opentony::runtime::update_ground_motion_threshold;

    const auto decayed = update_ground_motion_threshold(
        0x2d000,
        GroundMotionThresholdInput{0, false});
    CHECK(decayed.sampled_target == (0xaa * 0x2d000) / 0x118);
    CHECK(decayed.threshold == 0x2cfff);
    CHECK(decayed.changed);

    const auto held = update_ground_motion_threshold(
        0x100,
        GroundMotionThresholdInput{0x7f, false});
    CHECK(held.threshold == 0x100);
    CHECK(!held.changed);

    const auto special = update_ground_motion_threshold(
        0,
        GroundMotionThresholdInput{0, true});
    CHECK(special.threshold == (0xdc * 0x2d000) / 0x118);
    CHECK(special.changed);

    std::cout << "Ground motion threshold tests passed\n";
}
