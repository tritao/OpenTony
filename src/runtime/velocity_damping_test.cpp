#include "velocity_damping.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::VelocityDamping;
    using opentony::runtime::VelocityDampingInput;

    VelocityDampingInput small{};
    small.velocity = {30, -30, 20};
    small.rescale_roll = 0;
    small.decay_roll = 0;
    const auto small_result = VelocityDamping::apply(small);
    assert(small_result.fine_decay);
    assert(small_result.coarse_decay);
    assert(small_result.velocity == FixedPosition({23, -23, 0}));

    VelocityDampingInput large{};
    large.velocity = {4096, 0, 0};
    large.rescale_roll = -400;
    large.decay_roll = -500;
    const auto large_result = VelocityDamping::apply(large);
    // Retail's dot helper scales the squared Q12 vector by 1/4096 before
    // sqrt: a 0x1000 component has magnitude 0x40 and speed metric 0x1000.
    assert(!large_result.rescaled);
    assert(large_result.randomized_decay);
    assert(large_result.velocity[0] < 4096);

    std::cout << "Velocity damping tests passed\n";
}
