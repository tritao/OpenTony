#pragma once

#include "fixed_matrix.hpp"
#include "position_commit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace opentony::runtime {

constexpr std::size_t kGroundMotionProfileTableSize = 8;

// Runtime profile objects at skater +0x244/+0x248 contribute their +0x10
// field to two source arrays. FUN_00487c30 materializes a boolean with an
// exact `field == 1` test; FUN_00413c10 then copies those arrays into the
// runtime tables at DAT_0056a3d8 and DAT_0056a3e0.
struct GroundMotionProfileRecords final {
    std::array<std::int32_t, kGroundMotionProfileTableSize>
        primary_field_10{};
    std::array<std::int32_t, kGroundMotionProfileTableSize>
        secondary_field_10{};
    std::size_t player_index{};
    std::int32_t mode{};
    std::uint32_t mode7_selector{};
};

// The profile/stat loader materializes eight values at DAT_0056a3d8. B010
// indexes them directly except in mode 7, where it XORs the player selector
// before the load.
struct GroundMotionProfileTable final {
    std::array<std::int32_t, kGroundMotionProfileTableSize> values{};
    std::array<std::int32_t, kGroundMotionProfileTableSize>
        secondary_values{};
    std::size_t player_index{};
    std::int32_t mode{};
    std::uint32_t mode7_selector{};

    [[nodiscard]] std::size_t selected_index() const noexcept {
        const std::uint32_t raw_index = static_cast<std::uint32_t>(
            player_index);
        const std::uint32_t selected = mode == 7
            ? raw_index ^ mode7_selector
            : raw_index;
        return selected < values.size()
            ? static_cast<std::size_t>(selected)
            : values.size();
    }

    [[nodiscard]] bool selected_value_nonzero() const noexcept {
        const std::size_t index = selected_index();
        return index < values.size() && values[index] != 0;
    }

    [[nodiscard]] bool selected_secondary_value_nonzero() const noexcept {
        const std::size_t index = selected_index();
        return index < secondary_values.size() && secondary_values[index] != 0;
    }
};

[[nodiscard]] GroundMotionProfileTable materialize_ground_motion_profile_table(
    const GroundMotionProfileRecords& records) noexcept;

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
    // The +0x2ccc + 0x10 slot selects the stronger scale for animation states
    // 2/3. Its higher-level action/profile name is intentionally unresolved.
    bool strong_profile = false;
    // The value loaded from DAT_0056a3d8[skater + 0x2cc4]. Retail permits the
    // first B010 profile branch when either this value or +0x2ccc+0x10 is
    // nonzero. Its table contents are populated by the profile/stat loader,
    // so this remains an explicit input rather than being guessed from the
    // directional action-table result.
    bool profile_table_value_nonzero = false;
    // FUN_0049b010's explicit state-0 guard. State 2 is also excluded at the
    // individual correction writes.
    bool ordinary_ground_state = false;
    std::int16_t animation_state = 0;
    std::int16_t animation_frame = 0;
    // This is FUN_004f53b0(response) * 0x40, compared with +0x2dc8.
    std::int32_t response_speed_metric = 0;
    std::int32_t response_speed_threshold = 0;
    // The value saved from the +0x3118 surface-response calculation and
    // compared with -0x4cc by B010's cooldown/rearm branch. It is not the
    // ordinary speed metric above.
    std::int32_t surface_response_metric = 0;
    // The current +0x30f4 basis Y component used by the ordinary branch.
    std::int32_t forward_basis_y = 0;
    // B010 decrements +0x2f2c before its first/rearm branch. The later
    // 0x0049b225 correction block is still reachable while this is active;
    // the caller retains ownership of the rearm side effects.
    bool cooldown_active = false;
    // When used through PlayerPhysicsFrame, derive verified animation,
    // physics-state, turn-gate, and speed fields from PlayerState.
    bool use_player_state = true;
    // Enable B010's state writes in addition to the transient +58
    // correction. The profile and surface inputs remain explicit.
    bool apply_control_side_effects = false;
    // +0x2dd8 is the special/transition block used by the rearm predicates.
    bool blocked_or_special = false;
    // When true, rearm_random_roll is the already-resolved FUN_0048f3a0(3)
    // value. No native RNG is implied by this flag.
    bool rearm_random_available = false;
    std::int32_t rearm_random_roll = 0;
    // The animation service sets +0x107 only after the terminal frame has
    // been reached. B010's state-1 handoff is gated by that completion bit,
    // not merely by the integer frame number.
    bool animation_finished = false;
    // +0x30a8 is the pending animation event marker consumed for states 2/3.
    bool pending_animation_event = false;
    // Skater +0x107 enables the early B010 animation-event side effect.
    // The event dispatcher is not reconstructed here; this flag lets a
    // caller replay the already-observed writer without deriving it from an
    // animation state alone.
    bool animation_event_enabled = false;
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
    bool cooldown_written = false;
    std::int32_t cooldown_value = 0;
    bool threshold_written = false;
    std::int32_t threshold_value = 0;
    bool pending_animation_event_written = false;
    bool pending_animation_event = false;
    bool animation_event_written = false;
    std::uint32_t animation_event_parameter = 0;
    std::uint32_t event_reason = 0;
    std::int32_t animation_speed = 0;
};

// Exact temporary-correction writes recovered from FUN_0049b010. This is a
// producer for +0x58/+0x5c/+0x60, not a direct position or velocity update;
// PlayerState::integrate_motion_correction() owns the later +4c handoff.
[[nodiscard]] GroundMotionResult apply_ground_motion(
    FixedPosition& motion_correction,
    const RetailBasis& basis,
    const GroundMotionInput& input) noexcept;

} // namespace opentony::runtime
