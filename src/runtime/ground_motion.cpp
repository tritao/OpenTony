#include "ground_motion.hpp"

#include <cstdint>
#include <limits>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t scaled_basis(
    std::int32_t basis,
    std::int32_t scale) noexcept {
    const std::int64_t value = -static_cast<std::int64_t>(basis) * scale;
    if (value > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] GroundMotionResult write_basis_correction(
    FixedPosition& motion_correction,
    const FixedPosition& basis,
    std::int32_t scale,
    GroundMotionBranch branch,
    std::int32_t response_speed_metric) noexcept {
    for (std::size_t index = 0; index < motion_correction.size(); ++index) {
        motion_correction[index] = scaled_basis(basis[index], scale);
    }
    return GroundMotionResult{
        true,
        scale,
        branch,
        response_speed_metric,
    };
}

} // namespace

GroundMotionResult apply_ground_motion(
    FixedPosition& motion_correction,
    const RetailBasis& basis,
    const GroundMotionInput& input) noexcept {
    GroundMotionResult result{};
    result.response_speed_metric = input.response_speed_metric;
    if (!input.producer_enabled || input.physics_locked) {
        return result;
    }

    const bool below_threshold =
        input.response_speed_metric < input.response_speed_threshold;

    // This is the state-0 section of FUN_0049b010. The decompilation writes
    // the correction directly, so a caller should clear the transient field
    // once at frame start just as FUN_0049e680 does.
    if (input.ordinary_ground_state && input.correction_gate_open &&
        !input.cooldown_active &&
        below_threshold) {
        if ((input.animation_state == 2 || input.animation_state == 3) &&
            input.animation_frame > 10 && input.animation_frame < 0x10) {
            const std::int32_t scale = input.strong_profile ? 8 : 4;
            return write_basis_correction(
                motion_correction,
                basis.at_30f4,
                scale,
                GroundMotionBranch::Animation2Or3,
                input.response_speed_metric);
        }

        if (input.animation_state == 0x5e &&
            input.animation_frame > 0xf && input.animation_frame < 0x14) {
            return write_basis_correction(
                motion_correction,
                basis.at_30f4,
                8,
                GroundMotionBranch::Animation5e,
                input.response_speed_metric);
        }

        if ((input.response_speed_metric > 20000 ||
             input.forward_basis_y < 500)) {
            return write_basis_correction(
                motion_correction,
                basis.at_30f4,
                1,
                GroundMotionBranch::Ordinary,
                input.response_speed_metric);
        }
    }

    // The later profile branch is outside the explicit state-0 block, but it
    // still requires the profile flag and the same speed threshold. It is
    // skipped when the local profile path returned early in retail.
    if (!input.local_profile && input.strong_profile && below_threshold &&
        (input.animation_state == 2 || input.animation_state == 3) &&
        input.animation_frame > 10 &&
        input.animation_frame < 0x10) {
        return write_basis_correction(
            motion_correction,
            basis.at_30f4,
            8,
            GroundMotionBranch::Animation2Or3,
            input.response_speed_metric);
    }

    return result;
}

} // namespace opentony::runtime
