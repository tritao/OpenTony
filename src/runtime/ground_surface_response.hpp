#pragma once

#include <cstdint>

namespace opentony::runtime {

// Causal service results consumed by FUN_0049c060. The second cap value is
// read only when the first speed-limit draw caps the current response.
struct GroundSurfaceResponseInput final {
    std::int32_t cap_random{};
    std::int32_t capped_response_random{};
    std::int32_t target_random{};
    std::int32_t denominator_random{};
    bool capped_response_random_available{};
};

// Deterministic part of retail FUN_0048f5f0. The numeric value at +0x2cc8 is
// owned by an adjacent animation/action service; this boundary only needs its
// established zero/nonzero predicate.
struct GroundSurfaceResponseResetInput final {
    std::int32_t physics_state{};
    std::int32_t action_context_2cc8{};
    std::int32_t game_mode{};
};

struct GroundSurfaceResponseResetResult final {
    bool applied{};
    bool clear_spin_phase_2e80{};
    bool clear_auxiliary_list_3214{};

    friend bool operator==(
        const GroundSurfaceResponseResetResult&,
        const GroundSurfaceResponseResetResult&) = default;
};

// Pure PC reconstruction of FUN_0048f5f0's branch predicate and mode gate.
[[nodiscard]] GroundSurfaceResponseResetResult
compute_ground_surface_response_reset(
    const GroundSurfaceResponseResetInput& input) noexcept;

// Pure PC reconstruction of FUN_0049e430. The countdown is distinct from
// +0x2d90: it advances an action phase and its nonzero post-update value is
// later consumed by FUN_00496360 as a clear gate.
struct GroundSurfaceResponsePhaseInput final {
    std::int32_t countdown_2c88{};
    std::int32_t phase_2c8c{};
    std::int32_t rate_2c90{};
    std::int32_t frame_scale_q8{0x100};
};

struct GroundSurfaceResponsePhaseResult final {
    std::int32_t countdown_before{};
    std::int32_t countdown_after{};
    std::int32_t phase_before{};
    std::int32_t phase_after{};
    std::int32_t rate_after{};
    bool phase_boundary_crossed{};
    bool completed{};

    friend bool operator==(
        const GroundSurfaceResponsePhaseResult&,
        const GroundSurfaceResponsePhaseResult&) = default;
};

[[nodiscard]] GroundSurfaceResponsePhaseResult
compute_ground_surface_response_phase(
    const GroundSurfaceResponsePhaseInput& input) noexcept;

// Inputs owned by the caller of FUN_00496360. The clear flags are the raw
// +0x2c88/+0x2e80/+0x2c80 gates after their frame/control producers have run;
// `phase_refresh_blocked_2c84` is the raw post-wrap refresh gate.
// `response_correction_units` is the +0x3124 result from the preceding
// FUN_0049c060 call, not a new random input.
struct GroundSurfaceResponseStepInput final {
    std::int32_t physics_state{};
    std::int32_t ground_update_state{};
    std::int32_t turn_accumulator_q12{};
    std::int32_t surface_response_timer{};
    std::int32_t profile_value{};
    std::int32_t modifier_value{};
    std::int32_t frame_scale_q8{0x100};
    std::int32_t response_correction_units{};
    bool special_turn_inputs_available{};
    bool clear_timer_2c88{};
    bool clear_timer_2e80{};
    bool clear_timer_2c80{};
    bool phase_refresh_blocked_2c84{};
    bool action_bank_10_nonzero{};
    bool action_bank_20_nonzero{};
    // Existing replay service observations identify frames in which the
    // retail 0x0049c060 call actually returned. This is a temporary bridge
    // for the not-yet-owned +0x2d90 producer, not a new recording channel.
    bool response_call_observed{};
};

struct GroundSurfaceResponseStepResult final {
    bool special_ground_phase{};
    bool response_call_requested{};
    bool response_input_missing{};
    bool response_correction_consumed{};
    std::int32_t timer_before{};
    std::int32_t timer_after{};
    std::int32_t turn_units_before_response{};
    std::int32_t response_correction_units{};
    std::int32_t turn_units{};
    std::int32_t angle12{};
    bool orientation_written{};
    bool phase_accumulator_written{};
    bool phase_accumulator_wrapped{};
    bool phase_refresh_service_requested{};

    friend bool operator==(
        const GroundSurfaceResponseStepResult&,
        const GroundSurfaceResponseStepResult&) = default;
};

[[nodiscard]] constexpr bool uses_special_ground_surface_response_phase(
    std::int32_t physics_state,
    std::int32_t ground_update_state) noexcept {
    // FUN_00496360's bVar1 is state 2, or state 1 with +0x30c4 nonzero.
    return physics_state == 2
        || (physics_state == 1 && ground_update_state != 0);
}

// Pure control-flow reconstruction of FUN_00496360. It computes the turn
// units and timer stores around FUN_0049c060/FUN_0049b500; the PlayerState
// method supplies the service result and performs persistent writes.
[[nodiscard]] GroundSurfaceResponseStepResult
compute_ground_surface_response_step(
    const GroundSurfaceResponseStepInput& input) noexcept;

} // namespace opentony::runtime
