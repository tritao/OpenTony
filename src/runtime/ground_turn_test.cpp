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
    CHECK(lean_target.policy_changed);

    // +0x31a1 is a signed byte and the branch changes at the inclusive
    // magnitude boundary: 0x19 releases toward zero, while 0x1a targets the
    // analog lean value.
    const auto just_inside_lean = GroundTurn::update(
        0x4000,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, 0x19});
    CHECK(just_inside_lean.accumulator == 0x3000);
    CHECK(!just_inside_lean.policy_changed);

    const auto analog_boundary = GroundTurn::update(
        0,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, 0x1a});
    CHECK(analog_boundary.accumulator == 0x548b);
    CHECK(analog_boundary.policy_changed);

    const auto negative_lean = GroundTurn::update(
        0,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, -0x40});
    CHECK(negative_lean.accumulator == -0x7800);
    CHECK(negative_lean.mirror == negative_lean.accumulator);

    const auto analog_overshoot = GroundTurn::update(
        0x15000,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, 0x40});
    CHECK(analog_overshoot.accumulator == 0x16800);
    CHECK(analog_overshoot.policy_changed);

    const auto analog_settled = GroundTurn::update(
        0x16800,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x2d000, 2, 0x40});
    CHECK(analog_settled.accumulator == 0x16800);
    CHECK(!analog_settled.policy_changed);

    // The retail flag records branch selection, not whether clamping changed
    // the accumulator. Repeated input at the positive cap still sets +2e7c.
    const auto capped = GroundTurn::update(
        0x5a000,
        false,
        true,
        GroundTurnConfig{0x100, 0, 0x5a000});
    CHECK(capped.accumulator == 0x5a000);
    CHECK(capped.policy_changed);

    // The sign-corrected SAR 7 is observable for a non-canonical test limit;
    // -0x1a targets -0x340 rather than flooring to -0x341.
    const auto negative_target_boundary = GroundTurn::update(
        0,
        false,
        false,
        GroundTurnConfig{0x100, 0, 0x1001, 2, -0x1a});
    CHECK(negative_target_boundary.accumulator == -0x340);

    const auto clamped = GroundTurn::update(
        0x5a000,
        false,
        true,
        GroundTurnConfig{0x100, 0, 0x5a000});
    CHECK(clamped.accumulator == 0x5a000);
    CHECK(clamped.wide_profile);
    CHECK(clamped.policy_changed);
    CHECK(!clamped.response_normalized);
    CHECK(left.policy_changed);
    CHECK(GroundTurn::angle12(-0x3c00, 0x100) == 0xffc);
    CHECK(GroundTurn::angle12(0x78000, 0x100) == 0x78);

    std::cout << "Ground turn tests passed\n";
}
