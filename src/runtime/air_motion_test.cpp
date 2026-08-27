#include "air_motion.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    using opentony::runtime::AirGravityConfig;
    using opentony::runtime::FixedPosition;
    using opentony::runtime::apply_air_gravity;

    const auto falling = apply_air_gravity({100, -1000, 300});
    CHECK(falling.gravity_applied);
    CHECK(!falling.terminal_clamped);
    CHECK(falling.velocity == FixedPosition({100, -1500, 300}));

    // Equality is intentionally not the terminal branch in retail:
    // -0xe0c - 500 == -0x1000.
    const auto at_threshold = apply_air_gravity({100, -0xe0c, 300});
    CHECK(at_threshold.gravity_applied);
    CHECK(at_threshold.velocity == FixedPosition({100, -0x1000, 300}));

    const auto terminal = apply_air_gravity({100, -0xe0d, 300});
    CHECK(!terminal.gravity_applied);
    CHECK(terminal.terminal_clamped);
    CHECK(terminal.horizontal_reset);
    CHECK(terminal.velocity == FixedPosition({0, -0x1000, 0}));

    const auto custom = apply_air_gravity(
        {10, -20, 30},
        AirGravityConfig{4, -100, -50, false});
    CHECK(custom.velocity == FixedPosition({10, -24, 30}));

    const auto basis = opentony::runtime::rebuild_air_motion_basis(
        opentony::runtime::RetailBasis{
            {0, 0, 0x1000},
            {0x1000, 0, 0},
            {0, 0x1000, 0},
        },
        {0, 0x1000, 0});
    CHECK(basis.direction == FixedPosition({0, 0x1000, 0}));
    CHECK(basis.basis.at_3100 == FixedPosition({-0x1000, 0, 0}));
    CHECK(basis.basis.at_310c == FixedPosition({0, 0x1000, 0}));
    CHECK(basis.basis.at_30f4 == FixedPosition({0, 0, 0x1000}));

    using opentony::runtime::AirSpeedConfig;
    using opentony::runtime::compute_air_speed_scalar;
    CHECK(compute_air_speed_scalar(
        AirSpeedConfig{100, 0, 0, 0, false, false, false}) == 13000);
    CHECK(compute_air_speed_scalar(
        AirSpeedConfig{100, 0, 0, 0, true, true, true}) == 2437);
    CHECK(compute_air_speed_scalar(
        AirSpeedConfig{100, 0, 2, 250, false, false, false}) == 6500);

    const opentony::runtime::RetailBasis forward_basis{
        {0, 0, 0x1000},
        {0x1000, 0, 0},
        {0, 0x1000, 0},
    };
    const auto up = opentony::runtime::apply_air_direction_input(
        {100, 200, 300},
        forward_basis,
        true,
        false,
        opentony::runtime::AirDirectionInputConfig{100, 150});
    CHECK(up.delta == FixedPosition({0, 0, 150}));
    CHECK(up.motion_correction == FixedPosition({100, 200, 150}));
    CHECK(up.up_applied);
    CHECK(!up.down_applied);

    const auto down = opentony::runtime::apply_air_direction_input(
        {100, 200, 300},
        forward_basis,
        false,
        true,
        opentony::runtime::AirDirectionInputConfig{100, 150});
    CHECK(down.motion_correction == FixedPosition({100, 200, 450}));

    const auto opposing = opentony::runtime::apply_air_direction_input(
        {100, 200, 300},
        forward_basis,
        true,
        true,
        opentony::runtime::AirDirectionInputConfig{100, 150});
    CHECK(opposing.motion_correction == FixedPosition({100, 200, 300}));

    const auto control = opentony::runtime::apply_air_action_control(
        {3200, 0, 0},
        {0, 0, 0},
        forward_basis,
        opentony::runtime::AirActionControlConfig{
            1000, true, true, false, false, false, false});
    CHECK(control.applied);
    CHECK(control.motion_correction == FixedPosition({-100, 1800, 0}));

    std::cout << "Air motion tests passed\n";
}
