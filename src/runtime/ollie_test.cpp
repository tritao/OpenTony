#include "ollie.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::OllieImpulseInput;
    using opentony::runtime::compute_ollie_vertical_impulse;

    OllieImpulseInput input{};
    input.random.fifth = 100;
    const auto low = compute_ollie_vertical_impulse(input);
    assert(!low.high_slope_branch);

    input.slope_metric = 0x9c4;
    const auto high = compute_ollie_vertical_impulse(input);
    assert(high.high_slope_branch);
    assert(high.delta_y != low.delta_y);

    input = {};
    input.horizontal_speed_metric = 0x28000;
    input.height_delta_metric = 701;
    const auto adjusted = compute_ollie_vertical_impulse(input);
    assert(adjusted.speed_adjustment_applied);
    assert(adjusted.adjusted_height_delta == 700);

    input = {};
    input.random.fifth = 100;
    input.wallie = true;
    const auto wallie = compute_ollie_vertical_impulse(input);
    assert(wallie.delta_y == low.delta_y - 0xf000);
    std::cout << "Ollie tests passed\n";
}
