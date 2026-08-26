#include "air_motion.hpp"

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t arithmetic_shift_12(std::int64_t value) noexcept {
    if (value >= 0) {
        return static_cast<std::int32_t>(value >> 12);
    }
    return static_cast<std::int32_t>(-(((-value) + 0xfff) >> 12));
}

} // namespace

AirGravityResult apply_air_gravity(
    const FixedPosition& velocity,
    AirGravityConfig config) noexcept {
    AirGravityResult result;
    result.velocity = velocity;

    // FUN_00497df0 first checks the pre-update vertical component. A value
    // below -0xe0c takes the terminal branch; equality still receives the
    // -500 update and reaches -0x1000 with the retail defaults.
    if (result.velocity[1] < config.terminal_test_y) {
        result.velocity[1] = config.terminal_y;
        result.terminal_clamped = true;
        if (config.reset_horizontal_at_terminal) {
            result.velocity[0] = 0;
            result.velocity[2] = 0;
            result.horizontal_reset = true;
        }
        return result;
    }

    result.velocity[1] -= config.gravity_per_tick;
    result.gravity_applied = true;
    return result;
}

AirMotionBasisResult rebuild_air_motion_basis(
    const RetailBasis& current_basis,
    const FixedPosition& air_motion) noexcept {
    const FixedPosition direction = q12_normalize(air_motion);
    const FixedPosition right = q12_normalize(
        q12_cross(current_basis.at_30f4, direction));
    const FixedPosition forward = q12_cross(direction, right);
    return AirMotionBasisResult{
        direction,
        RetailBasis{forward, right, direction},
    };
}

std::int32_t compute_air_speed_scalar(AirSpeedConfig config) noexcept {
    std::int64_t stat_product =
        static_cast<std::int64_t>(config.stat_value) * 13000;
    if (config.game_mode == 9) {
        stat_product = (stat_product / 100) * 100;
    }
    std::int64_t scalar = stat_product / 100;

    if (config.physics_state == 2) {
        scalar =
            ((500 - static_cast<std::int64_t>(config.state_two_random))
                * scalar * 0x14) / 10000;
    }
    if (config.modifier_c) {
        scalar = (scalar * 0x96) / 100;
    }
    if (config.modifier_e) {
        scalar = (scalar * 0x32) / 100;
    }
    if (config.global_slowdown) {
        scalar = (scalar * 0x32) / 200;
    }
    return static_cast<std::int32_t>(scalar);
}

AirDirectionInputResult apply_air_direction_input(
    const FixedPosition& motion_correction,
    const RetailBasis& basis,
    bool up,
    bool down,
    AirDirectionInputConfig config) noexcept {
    const std::int64_t scaled_speed =
        (static_cast<std::int64_t>(config.speed_scalar)
            * config.scale_percent) / 100;
    const FixedPosition delta{
        arithmetic_shift_12(
            static_cast<std::int64_t>(basis.at_30f4[0]) * scaled_speed),
        arithmetic_shift_12(
            static_cast<std::int64_t>(basis.at_30f4[1]) * scaled_speed),
        arithmetic_shift_12(
            static_cast<std::int64_t>(basis.at_30f4[2]) * scaled_speed),
    };

    FixedPosition result = motion_correction;
    // Retail executes both branches if both records are active: Up subtracts
    // first and Down adds second, so opposing inputs cancel exactly.
    if (up) {
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] -= delta[index];
        }
    }
    if (down) {
        for (std::size_t index = 0; index < result.size(); ++index) {
            result[index] += delta[index];
        }
    }
    return AirDirectionInputResult{delta, result, up, down};
}

} // namespace opentony::runtime
