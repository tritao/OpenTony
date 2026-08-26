#include "ground_motion_threshold.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::GroundMotionThresholdInput;
    using opentony::runtime::update_ground_motion_threshold;

    const auto decayed = update_ground_motion_threshold(
        0x2d000,
        GroundMotionThresholdInput{0, false});
    assert(decayed.sampled_target == (0xaa * 0x2d000) / 0x118);
    assert(decayed.threshold == 0x2cfff);
    assert(decayed.changed);

    const auto held = update_ground_motion_threshold(
        0x100,
        GroundMotionThresholdInput{0x7f, false});
    assert(held.threshold == 0x100);
    assert(!held.changed);

    const auto special = update_ground_motion_threshold(
        0,
        GroundMotionThresholdInput{0, true});
    assert(special.threshold == (0xdc * 0x2d000) / 0x118);
    assert(special.changed);

    std::cout << "Ground motion threshold tests passed\n";
}
