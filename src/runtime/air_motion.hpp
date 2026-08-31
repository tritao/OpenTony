#pragma once

#include "fixed_math.hpp"
#include "fixed_matrix.hpp"

#include <cstdint>

namespace opentony::runtime {

struct AirGravityConfig {
    // These are the literal values recovered from FUN_00497df0. The caller
    // can override them for a different retail mode while keeping the
    // integer operation and clamp ordering intact.
    std::int32_t gravity_per_tick = 500;
    std::int32_t terminal_y = -0x1000;
    std::int32_t terminal_test_y = -0xe0c;
    bool reset_horizontal_at_terminal = true;
};

struct AirGravityResult {
    FixedPosition velocity{};
    bool gravity_applied{};
    bool terminal_clamped{};
    bool horizontal_reset{};

    friend bool operator==(
        const AirGravityResult&,
        const AirGravityResult&) = default;
};

struct AirMotionBasisResult {
    FixedPosition direction{};
    RetailBasis basis{};

    friend bool operator==(
        const AirMotionBasisResult&,
        const AirMotionBasisResult&) = default;
};

struct AirSpeedConfig {
    // Skater +0x29f4, copied as a signed short by retail. The remaining
    // values are the mode/state inputs read while FUN_0049e680 prepares
    // +0x2dac for the in-air handler.
    std::int32_t stat_value{};
    std::int32_t game_mode{};
    std::int32_t physics_state{};
    // Raw result of FUN_0048f3a0(1) for state 2. Keeping the RNG result
    // caller-supplied makes the producer deterministic without replacing
    // retail's random stream.
    std::int32_t state_two_random{};
    bool modifier_c{};
    bool modifier_e{};
    bool global_slowdown{};
};

struct AirDirectionInputConfig {
    // Integer movement/stat scalar written to player +0x2dac by the retail
    // air-speed producer below.
    std::int32_t speed_scalar{};
    std::int32_t scale_percent{150};
};

struct AirDirectionInputResult {
    FixedPosition delta{};
    FixedPosition motion_correction{};
    bool up_applied{};
    bool down_applied{};

    friend bool operator==(
        const AirDirectionInputResult&,
        const AirDirectionInputResult&) = default;
};

// The first control block in retail Skater_DoPhysicsInAir (0x00497f40)
// writes the temporary +0x58 acceleration before the shared position step.
// The scalar and gate belong to the surrounding player/stat service, so keep
// them explicit while retaining the recovered action ordering here.
struct AirActionControlConfig {
    std::int32_t gravity_acceleration{}; // player +0x2dac
    bool control_enabled{}; // DAT_0056b7f0
    bool kick_held{}; // action record +0x30
    bool up_held{}; // action record +0xa0
    bool down_held{}; // action record +0xb0
    bool spin_left_held{}; // action record +0x40
    bool spin_right_held{}; // action record +0x60
};

struct AirActionControlResult {
    FixedPosition motion_correction{};
    bool applied{};

    friend bool operator==(
        const AirActionControlResult&,
        const AirActionControlResult&) = default;
};

// Inputs consumed by the later ordinary state-1 turn block in retail
// FUN_00497f40. `profile_value` is the result of FUN_0048f3a0(4), while
// `modifier_value` is +0x306c when +0x2858 is zero and otherwise comes from
// FUN_0048cb60(+0x2858 - 1). The Warehouse path takes the former branch.
struct AirOrientationTurnConfig {
    std::int32_t profile_value{};
    std::int32_t modifier_value{};
    std::int32_t frame_scale_q8{0x100};
    // FUN_00416980(10) selects whether the producer applies the Q8 frame
    // scale after its integer angle calculation. Warehouse returns zero.
    bool scale_with_frame{true};
};

[[nodiscard]] AirActionControlResult apply_air_action_control(
    const FixedPosition& velocity,
    const FixedPosition& motion_correction,
    const RetailBasis& basis,
    AirActionControlConfig config) noexcept;

// Reconstructs the ordinary state-1 angle producer in FUN_00497f40. The
// returned angle is the positive producer value; the matrix writer applies
// its right-multiplied rotation with the opposite signed angle.
[[nodiscard]] std::int32_t compute_air_orientation_turn_angle(
    std::int32_t turn_accumulator_q12,
    AirOrientationTurnConfig config) noexcept;

// Reconstructs the scalar part of retail FUN_00497df0. The basis operation
// below mirrors its subsequent FUN_004e2ff0/FUN_00465f60 handoff.
[[nodiscard]] AirGravityResult apply_air_gravity(
    const FixedPosition& velocity,
    AirGravityConfig config = {}) noexcept;

// Rebuilds the orientation columns from a normalized air direction. Retail
// calls this after gravity: right = normalize(forward x air), then forward =
// air x right, and copies [right, air, forward] into the skater matrix.
[[nodiscard]] AirMotionBasisResult rebuild_air_motion_basis(
    const RetailBasis& current_basis,
    const FixedPosition& air_motion) noexcept;

// Reconstructs the +0x2dac writer in FUN_0049e680. The state-two random
// value and mode flags are explicit inputs because they belong to the retail
// RNG/profile services rather than fixed-point arithmetic.
[[nodiscard]] std::int32_t compute_air_speed_scalar(
    AirSpeedConfig config) noexcept;

// Reconstructs the confirmed Up/Down portion of Skater_DoPhysicsInAir. The
// current +30f4 basis vector is scaled by (speed * 150) / 100, shifted by
// twelve with x86 SAR semantics, then subtracted for Up and added for Down
// to the temporary +58 motion correction.
[[nodiscard]] AirDirectionInputResult apply_air_direction_input(
    const FixedPosition& motion_correction,
    const RetailBasis& basis,
    bool up,
    bool down,
    AirDirectionInputConfig config = {}) noexcept;

} // namespace opentony::runtime
