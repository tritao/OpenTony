#pragma once

#include "position_commit.hpp"

#include <cstdint>

namespace opentony::runtime {

// Constants from the PC retail grounded bounce/recovery helpers.  These are
// fixed-point/world-space contracts, not replay-tuned thresholds.
inline constexpr std::int32_t kGroundedBounceProbeHeight = 0x1e;
inline constexpr std::int32_t kGroundedBounceNormalOffset = 1;
inline constexpr std::int32_t kGroundedBounceDefaultDistance = 0x0a;
inline constexpr std::int32_t kGroundedBounceBlockedDistance = 0x46;
inline constexpr std::int32_t kGroundedBounceResponseBiasQ12 = 0xcd;
inline constexpr std::int32_t kGroundedBounceDefaultHeading = 0x19;
inline constexpr std::int32_t kGroundedBounceAlternateHeading = 200;
inline constexpr std::int32_t kGroundedBounceOrthogonalLimitQ12 = 0x400;

[[nodiscard]] constexpr std::int32_t grounded_bounce_probe_distance(
    bool control_blocked) noexcept {
    return control_blocked
        ? kGroundedBounceBlockedDistance
        : kGroundedBounceDefaultDistance;
}

[[nodiscard]] constexpr bool accepts_grounded_bounce_normal(
    std::int32_t normal_alignment_q12) noexcept {
    return normal_alignment_q12 > -kGroundedBounceOrthogonalLimitQ12
        && normal_alignment_q12 < kGroundedBounceOrthogonalLimitQ12;
}

struct GroundedBounceProbeGeometry final {
    FixedPosition first_start{};
    FixedPosition first_end{};
};

// Build the two lines used by one 0x004957c0 call.  Retail uses the live
// position for the first line, but the position restored from +0xbc for both
// endpoints of the verification line.
[[nodiscard]] GroundedBounceProbeGeometry make_grounded_bounce_probe_geometry(
    const FixedPosition& live_position,
    const FixedPosition& right_basis,
    const FixedPosition& up_basis,
    std::int32_t direction,
    std::int32_t distance) noexcept;

[[nodiscard]] FixedPosition make_grounded_bounce_verification_start(
    const FixedPosition& previous_position,
    const FixedPosition& hit_normal) noexcept;

[[nodiscard]] FixedPosition make_grounded_bounce_verification_end(
    const FixedPosition& previous_position,
    const FixedPosition& hit_normal,
    std::int32_t direction,
    std::int32_t distance) noexcept;

// FUN_00496550's state-0 support tail calls FUN_004956f0 before publishing a
// response or recovery basis when the selected support face is not eligible
// for the ordinary grounded path.  The predicate is expressed in terms of
// the decoded face bits and the three live player vectors used by the PC
// branch; it is deliberately independent of a surface/material number.
[[nodiscard]] bool support_hit_requests_ground_exit(
    const PositionCollisionHit& hit,
    const FixedPosition& recovery_target_normal,
    const FixedPosition& current_forward,
    const FixedPosition& collision_response) noexcept;

} // namespace opentony::runtime
