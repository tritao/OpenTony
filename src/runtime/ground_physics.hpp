#pragma once

#include "ground_brake.hpp"

#include <cstdint>

namespace opentony::runtime {

// Raw inputs read by retail FUN_0049df00. The object/profile predicates are
// intentionally caller-owned: the native collision layer can report the
// packed face metadata, but it must not guess which combination selects the
// retail bVar4 surface path.
struct GroundPhysicsInput final {
    FixedPosition response{};
    std::int32_t physics_state{};       // skater +0x30b8
    std::int32_t ground_update_state{}; // skater +0x2df8
    std::int32_t slope_normal_y_q12{};  // skater +0x312a, signed short
    std::int16_t animation_frame{};     // skater +0xf4
    std::uint16_t animation_state{};    // skater +0xf6
    std::int8_t vertical_lean{};        // skater +0x31a2
    std::int8_t horizontal_lean{};      // skater +0x31a1
    bool surface_allows_brake{};        // recovered bVar4 predicate
    bool physics_locked{};              // skater +0x2dd4
    bool state_blocked{};                // +0x29c8/+0x2dd0/+0x2f64 aggregate
    bool blocked_or_special{};          // skater +0x2dd8
    bool animation_ready{};             // skater +0x107
    std::int32_t frame_scale_q8{0x100};  // DAT_0056865c-equivalent scale
    // Raw CSuper animation fields read by the mode-1 transition. They remain
    // caller-owned so this movement boundary does not select an animation.
    std::uint8_t playback_endpoint{};     // skater +0x101 raw byte
    std::int16_t original_start_frame{};  // skater +0x114
};

// The mode-1 branch updates these animation-control bytes before handing the
// actual animation request to the animation service. Keep the values raw and
// preserve the retail byte/short truncation at this boundary.
struct GroundAnimationControlHandoff final {
    bool applied{};
    std::uint8_t animation_finished{};   // skater +0x107, cleared by retail
    std::int8_t playback_direction{};    // skater +0x100, written as 0xff
    std::uint8_t playback_endpoint{};    // skater +0x101, old +0x114 low byte
    std::int16_t original_start_frame{}; // skater +0x114, sign-extended +0x101

    friend bool operator==(
        const GroundAnimationControlHandoff&,
        const GroundAnimationControlHandoff&) = default;
};

enum class GroundPhysicsAction : std::uint8_t {
    None,
    ResetForNonGround,
    RequestGroundFromState7,
    StopAndRequestState7,
    EnterLowSpeedMode,
    EnterHighSpeedMode,
    AdvanceToAnimationMode,
    AdvanceToAnimationComplete,
    ResetToIdleMode,
};

struct GroundPhysicsResult final {
    FixedPosition response{};
    std::int32_t ground_update_state{};
    std::int32_t speed_metric{};
    std::int32_t speed_threshold{};
    bool response_decelerated{};
    bool response_stopped{};
    bool cooldown_written{};
    std::int32_t cooldown_value{};
    bool physics_state_requested{};
    std::int32_t requested_physics_state{};
    std::uint32_t requested_physics_reason{};
    bool animation_transition{};
    GroundPhysicsAction action{GroundPhysicsAction::None};
    GroundAnimationControlHandoff animation_handoff{};

    friend bool operator==(
        const GroundPhysicsResult&,
        const GroundPhysicsResult&) = default;
};

// Port of the stateful, non-animation side of retail FUN_0049df00. It owns
// the exact threshold/deceleration branch and +0x2df8 transitions, while
// animation/audio/script side effects remain explicit services.
[[nodiscard]] GroundPhysicsResult update_ground_physics(
    const GroundPhysicsInput& input) noexcept;

} // namespace opentony::runtime
