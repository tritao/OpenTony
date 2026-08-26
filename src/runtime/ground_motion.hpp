#pragma once

#include "fixed_matrix.hpp"
#include "position_commit.hpp"

#include <cstdint>

namespace opentony::runtime {

// Inputs recovered from FUN_0049b010. PlayerPhysicsFrame fills the verified
// profile, animation, turn-policy, and speed fields from PlayerState; the
// unresolved local-profile/stat fields remain caller-owned.
struct GroundMotionInput final {
    // This is the outer (+0x2ccc + 0x10) || local profile gate.
    bool producer_enabled = false;
    // +0x2dd4/+0x2df8 suppress the whole producer.
    bool physics_locked = false;
    // The first branch is guarded by (+0x2e78 == 0 || +0x2e7c == 0).
    bool correction_gate_open = true;
    // The +0x2ccc + 0x10 slot is the configured GRAB action and selects the
    // stronger scale for animation states 2/3.
    bool strong_profile = false;
    // A nonzero local profile returns after the first branch and skips the
    // later ordinary profile branch.
    bool local_profile = false;
    // FUN_0049b010's explicit state-0 guard. State 2 is also excluded at the
    // individual correction writes.
    bool ordinary_ground_state = false;
    std::int16_t animation_state = 0;
    std::int16_t animation_frame = 0;
    // This is FUN_004f53b0(response) * 0x40, compared with +0x2dc8.
    std::int32_t response_speed_metric = 0;
    std::int32_t response_speed_threshold = 0;
    // The current +0x30f4 basis Y component used by the ordinary branch.
    std::int32_t forward_basis_y = 0;
    // B010 decrements +0x2f2c before its first correction branch. The
    // rearm conditions are still owned by the caller, but an active cooldown
    // must suppress that branch.
    bool cooldown_active = false;
    // When used through PlayerPhysicsFrame, derive verified animation,
    // physics-state, turn-gate, and speed fields from PlayerState.
    bool use_player_state = true;
};

enum class GroundMotionBranch : std::uint8_t {
    None,
    Animation2Or3,
    Animation5e,
    Ordinary,
};

struct GroundMotionResult final {
    bool applied = false;
    std::int32_t scale = 0;
    GroundMotionBranch branch = GroundMotionBranch::None;
    std::int32_t response_speed_metric = 0;
};

// Exact temporary-correction writes recovered from FUN_0049b010. This is a
// producer for +0x58/+0x5c/+0x60, not a direct position or velocity update;
// PlayerState::integrate_motion_correction() owns the later +4c handoff.
[[nodiscard]] GroundMotionResult apply_ground_motion(
    FixedPosition& motion_correction,
    const RetailBasis& basis,
    const GroundMotionInput& input) noexcept;

} // namespace opentony::runtime
