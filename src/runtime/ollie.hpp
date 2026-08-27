#pragma once

#include <cstdint>

namespace opentony::runtime {

// These are the action-record masks read by the recovered prephysics and
// in-air handlers. They are kept here as semantic labels rather than folded
// into a guessed controller mapping.
inline constexpr std::uint16_t kJumpActionBit = 0x0010;
inline constexpr std::uint16_t kKickActionBit = 0x0040;
inline constexpr std::uint32_t kOrdinaryLaunchReason = 0x245c;
inline constexpr std::uint32_t kAlternateLaunchReason = 0x2457;
inline constexpr std::uint32_t kLandingReason = 0x1fd6;

struct OllieImpulseRandom {
    std::int32_t first = 0;
    std::int32_t second = 0;
    std::int32_t third = 0;
    std::int32_t fourth = 0;
    std::int32_t fifth = 0;
};

struct OllieImpulseInput {
    std::int32_t charge = 0;                    // player +0x2de8
    std::int32_t slope_metric = 0;              // player +0x3110
    std::int32_t horizontal_speed_metric = 0;   // player +0x2f30
    std::int32_t height_delta_metric = 0;       // (+0x2f48 - +0x2f4c) >> 12
    bool wallie = false;                        // raw state 5
    OllieImpulseRandom random{};
    OllieImpulseRandom early_release_random{};
    bool early_release_random_available = false;
};

struct OllieImpulseResult {
    std::int32_t delta_y = 0;
    std::int32_t adjusted_height_delta = 0;
    bool high_slope_branch = false;
    bool speed_adjustment_applied = false;
};

// Exact integer producer recovered from retail 0x0049a280. Shared random
// state and all horizontal/animation effects remain explicit caller inputs.
[[nodiscard]] OllieImpulseResult compute_ollie_vertical_impulse(
    const OllieImpulseInput& input) noexcept;

struct OllieBookkeeping {
    std::int32_t charge = 0;                 // +0x2de8
    std::int32_t latched = 0;                // +0x2de0
    std::int32_t pending = 0;                // +0x2dd8
    std::int32_t in_progress = 0;            // +0x2ddc
    std::int32_t mode = 0;                   // +0x2db4
    std::int32_t mode_latched = 0;           // +0x2db8
    std::int32_t launch_charge = 0;          // +0x2dec
    std::int32_t launch_frame = 0;           // +0x2f34
    std::int32_t launch_count = 0;           // +0x303c
    std::int32_t early_release_count = 0;    // +0x3040
    std::int32_t animation_gate = 0;         // +0x2e30
    std::int32_t special_mode = 0;           // +0x2dd4
    std::int32_t latch_timestamp = 0;        // +0x2de4
    std::int32_t speed_metric = 0;           // +0x2f30
    std::int32_t wallie = 0;                 // +0x2df4

    friend bool operator==(
        const OllieBookkeeping&,
        const OllieBookkeeping&) = default;
};

struct OlliePrePhysicsInput {
    bool prephysics_blocked = false;         // player +0x2f64
    std::int32_t current_frame = -1;
    std::int32_t global_release_mode = 0;    // DAT_00568658
    bool force_cap = false;                  // DAT_0056a890
    OllieImpulseRandom charge_cap_random{};
    bool charge_cap_random_available = false;
    OllieImpulseRandom charge_cap_refresh_random{};
    bool charge_cap_refresh_random_available = false;
    OllieImpulseRandom early_release_random{};
    bool early_release_random_available = false;
    OllieImpulseInput impulse{};
};

enum class OlliePrePhysicsEvent : std::uint8_t {
    None,
    Charging,
    StaleLatchCleared,
    Cancelled,
    Launched,
};

struct OlliePrePhysicsResult {
    OlliePrePhysicsEvent event = OlliePrePhysicsEvent::None;
    std::int32_t charge = 0;
    std::int32_t cap = 0;
    bool capped = false;
    bool latch_set = false;
    bool pending_set = false;
    bool stale_latch_cleared = false;
    bool launch_consumed = false;
    bool state_requested = false;
    std::int32_t requested_state = 0;
    std::uint32_t request_reason = 0;

    friend bool operator==(
        const OlliePrePhysicsResult&,
        const OlliePrePhysicsResult&) = default;
};

} // namespace opentony::runtime
