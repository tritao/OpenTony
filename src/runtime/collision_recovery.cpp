#include "collision_recovery.hpp"

#include "fixed_math.hpp"

#include <cstddef>
#include <cstdint>

namespace opentony::runtime {
namespace {

[[nodiscard]] FixedPosition add_scaled(
    const FixedPosition& position,
    const FixedPosition& axis,
    std::int64_t scale) noexcept {
    FixedPosition result = position;
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index] = static_cast<std::int32_t>(
            static_cast<std::int64_t>(result[index])
            + static_cast<std::int64_t>(axis[index]) * scale);
    }
    return result;
}

} // namespace

GroundedBounceProbeGeometry make_grounded_bounce_probe_geometry(
    const FixedPosition& live_position,
    const FixedPosition& right_basis,
    const FixedPosition& up_basis,
    std::int32_t direction,
    std::int32_t distance) noexcept {
    const FixedPosition first_start = add_scaled(
        live_position,
        up_basis,
        kGroundedBounceProbeHeight);
    const FixedPosition first_end = add_scaled(
        first_start,
        right_basis,
        static_cast<std::int64_t>(direction) * distance);
    return GroundedBounceProbeGeometry{first_start, first_end};
}

FixedPosition make_grounded_bounce_verification_start(
    const FixedPosition& previous_position,
    const FixedPosition& hit_normal) noexcept {
    return add_scaled(
        previous_position,
        hit_normal,
        kGroundedBounceNormalOffset);
}

FixedPosition make_grounded_bounce_verification_end(
    const FixedPosition& previous_position,
    const FixedPosition& hit_normal,
    std::int32_t direction,
    std::int32_t distance) noexcept {
    return add_scaled(
        previous_position,
        hit_normal,
        static_cast<std::int64_t>(direction) * distance);
}

bool support_hit_requests_ground_exit(
    const PositionCollisionHit& hit,
    const FixedPosition& recovery_target_normal,
    const FixedPosition& current_forward,
    const FixedPosition& collision_response) noexcept {
    if (hit.surface_bit_6) {
        return false;
    }

    // Inverse24 is (~raw_collision_word >> 24) & 1.  A set surface bit 8
    // reaches FUN_004956f0 directly; a clear bit 8 reaches the state-0
    // orientation/velocity gate below.
    if (!hit.surface_bit_8_clear) {
        return true;
    }

    const std::int32_t target_alignment = fixed_dot_q12(
        recovery_target_normal,
        hit.normal);
    const std::int32_t forward_alignment = fixed_dot_q12(
        current_forward,
        hit.normal);
    return target_alignment > 0
        && target_alignment < 3000
        && forward_alignment < 0
        && collision_response[1] < 1;
}

} // namespace opentony::runtime
