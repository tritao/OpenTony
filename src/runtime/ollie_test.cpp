#include "ollie.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    using opentony::runtime::OllieImpulseInput;
    using opentony::runtime::compute_ollie_vertical_impulse;

    OllieImpulseInput input{};
    input.random.fifth = 100;
    const auto low = compute_ollie_vertical_impulse(input);
    CHECK(!low.high_slope_branch);

    input.slope_metric = 0x9c4;
    const auto high = compute_ollie_vertical_impulse(input);
    CHECK(high.high_slope_branch);
    CHECK(high.delta_y != low.delta_y);

    input = {};
    input.horizontal_speed_metric = 0x28000;
    input.height_delta_metric = 701;
    const auto adjusted = compute_ollie_vertical_impulse(input);
    CHECK(adjusted.speed_adjustment_applied);
    CHECK(adjusted.adjusted_height_delta == 700);

    input = {};
    input.random.fifth = 100;
    input.wallie = true;
    const auto wallie = compute_ollie_vertical_impulse(input);
    CHECK(wallie.delta_y == low.delta_y - 0xf000);
    std::cout << "Ollie tests passed\n";
}
