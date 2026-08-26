#include "ground_turn.hpp"

#include "tests/test_check.hpp"
#include <cstdint>
#include <iostream>

int main() {
    using opentony::runtime::GroundTurn;
    using opentony::runtime::GroundTurnConfig;

    const auto left = GroundTurn::update(0, true, false);
    CHECK(left.accumulator == -0x3c00);
    CHECK(left.mirror == left.accumulator);
    CHECK(left.delta == -0x3c00);

    const auto right = GroundTurn::update(0, false, true);
    CHECK(right.accumulator == 0x3c00);
    const auto both = GroundTurn::update(0, true, true);
    CHECK(both.accumulator == -0x3c00);
    const auto released = GroundTurn::update(-0x4000, false, false);
    CHECK(released.accumulator == -0x3000);
    const auto released_to_zero = GroundTurn::update(-0x400, false, false);
    CHECK(released_to_zero.accumulator == 0);

    const auto alternate = GroundTurn::update(
        0,
        false,
        true,
        GroundTurnConfig{0x100, 1, 0xa0000});
    CHECK(alternate.accumulator == 0x7800);

    const auto extended_profile = GroundTurn::update(
        0,
        false,
        true,
        GroundTurnConfig{0x100, 2, 0xa0000});
    CHECK(extended_profile.accumulator == 0xb400);

    const auto lean_target = GroundTurn::update(
        0,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, 0x40});
    CHECK(lean_target.accumulator == 0x7800);

    const auto clamped = GroundTurn::update(
        0x5a000,
        false,
        true,
        GroundTurnConfig{0x100, 0, 0x5a000});
    CHECK(clamped.accumulator == 0x5a000);
    CHECK(clamped.wide_profile);
    CHECK(!clamped.policy_changed);
    CHECK(!clamped.response_normalized);
    CHECK(left.policy_changed);
    CHECK(GroundTurn::angle12(-0x3c00, 0x100) == 0xffc);
    CHECK(GroundTurn::angle12(0x78000, 0x100) == 0x78);

    std::cout << "Ground turn tests passed\n";
}
