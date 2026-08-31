#include "ground_surface_response.hpp"

#include "fixed_math.hpp"

#include <cstdint>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t wrap32(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int32_t arithmetic_shift_right(
    std::int32_t value,
    unsigned amount) noexcept {
    if (amount == 0) {
        return value;
    }
    const std::uint32_t shifted =
        static_cast<std::uint32_t>(value) >> amount;
    if (value >= 0) {
        return static_cast<std::int32_t>(shifted);
    }
    return static_cast<std::int32_t>(
        shifted | (~std::uint32_t{0} << (32U - amount)));
}

[[nodiscard]] std::int32_t multiply32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return wrap32(static_cast<std::int64_t>(left) * right);
}

[[nodiscard]] std::int32_t divide32(
    std::int32_t numerator,
    std::int32_t denominator) noexcept {
    return denominator == 0
        ? 0
        : static_cast<std::int32_t>(
            static_cast<std::int64_t>(numerator) / denominator);
}

[[nodiscard]] std::int32_t compute_special_turn_units(
    const GroundSurfaceResponseStepInput& input) noexcept {
    if (!input.special_turn_inputs_available) {
        return 0;
    }
    const std::int32_t turn_units = arithmetic_shift_right(
        input.turn_accumulator_q12,
        12);
    const std::int32_t profile_plus_300 = wrap32(
        static_cast<std::int64_t>(input.profile_value) + 300);
    const std::int32_t profile_product = multiply32(
        multiply32(profile_plus_300, turn_units),
        25);
    const std::int32_t profile_term = divide32(profile_product, 10000);
    const std::int32_t modifier_term = wrap32(
        static_cast<std::int64_t>(input.modifier_value) * -9 + 100);
    return divide32(
        multiply32(profile_term, modifier_term),
        100);
}

} // namespace

GroundSurfaceResponseResetResult compute_ground_surface_response_reset(
    const GroundSurfaceResponseResetInput& input) noexcept {
    if (input.physics_state == 5 || input.action_context_2cc8 == 0) {
        return {};
    }
    return GroundSurfaceResponseResetResult{
        true,
        input.physics_state != 2,
        input.game_mode == 2 || input.game_mode == 4,
    };
}

GroundSurfaceResponsePhaseResult compute_ground_surface_response_phase(
    const GroundSurfaceResponsePhaseInput& input) noexcept {
    GroundSurfaceResponsePhaseResult result{};
    result.countdown_before = input.countdown_2c88;
    result.countdown_after = input.countdown_2c88;
    result.phase_before = input.phase_2c8c;
    result.phase_after = input.phase_2c8c;
    result.rate_after = input.rate_2c90;
    if (input.countdown_2c88 == 0) {
        return result;
    }

    const std::int32_t phase_delta = arithmetic_shift_right(
        multiply32(input.rate_2c90, input.frame_scale_q8),
        8);
    result.phase_after = wrap32(
        static_cast<std::int64_t>(input.phase_2c8c) + phase_delta);
    result.phase_boundary_crossed =
        ((static_cast<std::uint32_t>(result.phase_after)
          ^ static_cast<std::uint32_t>(input.phase_2c8c)) & 0x1000U) != 0;
    if (!result.phase_boundary_crossed) {
        return result;
    }

    result.countdown_after = input.countdown_2c88 - 1;
    if (result.countdown_after == 0) {
        result.phase_after = 0;
        result.rate_after = 0;
        result.completed = true;
    }
    return result;
}

GroundSurfaceResponseStepResult compute_ground_surface_response_step(
    const GroundSurfaceResponseStepInput& input) noexcept {
    GroundSurfaceResponseStepResult result{};
    result.special_ground_phase = uses_special_ground_surface_response_phase(
        input.physics_state,
        input.ground_update_state);
    result.timer_before = input.surface_response_timer;
    result.timer_after = input.surface_response_timer;

    const bool clear_timer = input.clear_timer_2c88
        || input.clear_timer_2e80
        || input.clear_timer_2c80;
    if (clear_timer) {
        result.timer_after = 0;
    }

    result.turn_units_before_response = result.special_ground_phase
        ? compute_special_turn_units(input)
        : arithmetic_shift_right(input.turn_accumulator_q12, 12);
    result.turn_units = result.turn_units_before_response;

    if (result.timer_after == 0 && !input.response_call_observed) {
        result.timer_after = 0;
        result.angle12 = fixed_scale_q8(
            result.turn_units,
            input.frame_scale_q8);
        return result;
    }
    if (clear_timer) {
        result.timer_after = 0;
        result.angle12 = fixed_scale_q8(
            result.turn_units,
            input.frame_scale_q8);
        return result;
    }

    if (result.turn_units == 0) {
        // Retail calls FUN_0049c060(0x28) only in this branch, then folds its
        // +0x3124 output into the turn units without changing +0x2d90.
        result.response_call_requested = true;
    } else {
        const bool action_bank_active = input.action_bank_10_nonzero
            || input.action_bank_20_nonzero;
        const std::int32_t decrement = action_bank_active ? 1 : 3;
        result.timer_after = input.surface_response_timer - decrement;
        if (result.timer_after < 0) {
            result.timer_after = 0;
        }
    }

    result.turn_units = wrap32(
        static_cast<std::int64_t>(result.turn_units)
        + input.response_correction_units);
    result.response_correction_units = input.response_correction_units;
    result.response_correction_consumed =
        input.response_correction_units != 0;
    result.angle12 = fixed_scale_q8(
        result.turn_units,
        input.frame_scale_q8);
    return result;
}

} // namespace opentony::runtime
