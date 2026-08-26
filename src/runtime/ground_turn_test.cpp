#include "ground_turn.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    using opentony::runtime::GroundTurn;
    using opentony::runtime::GroundTurnConfig;

    const auto left = GroundTurn::update(0, true, false);
    assert(left.accumulator == -0x3c00);
    assert(left.mirror == left.accumulator);
    assert(left.delta == -0x3c00);

    const auto right = GroundTurn::update(0, false, true);
    assert(right.accumulator == 0x3c00);
    const auto both = GroundTurn::update(0, true, true);
    assert(both.accumulator == -0x3c00);
    const auto released = GroundTurn::update(-0x4000, false, false);
    assert(released.accumulator == -0x3000);
    const auto released_to_zero = GroundTurn::update(-0x400, false, false);
    assert(released_to_zero.accumulator == 0);

    const auto alternate = GroundTurn::update(
        0,
        false,
        true,
        GroundTurnConfig{0x100, 1, 0xa0000});
    assert(alternate.accumulator == 0x7800);

    const auto extended_profile = GroundTurn::update(
        0,
        false,
        true,
        GroundTurnConfig{0x100, 2, 0xa0000});
    assert(extended_profile.accumulator == 0xb400);

    const auto lean_target = GroundTurn::update(
        0,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, 0x40});
    assert(lean_target.accumulator == 0x7800);

    const auto clamped = GroundTurn::update(
        0x5a000,
        false,
        true,
        GroundTurnConfig{0x100, 0, 0x5a000});
    assert(clamped.accumulator == 0x5a000);
    assert(clamped.wide_profile);
    assert(!clamped.policy_changed);
    assert(!clamped.response_normalized);
    assert(left.policy_changed);
    assert(GroundTurn::angle12(-0x3c00, 0x100) == 0xffc);
    assert(GroundTurn::angle12(0x78000, 0x100) == 0x78);

    std::cout << "Ground turn tests passed\n";
}
