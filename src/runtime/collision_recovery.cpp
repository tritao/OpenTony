#include "collision_recovery.hpp"

#include "fixed_math.hpp"
#include "player_state.hpp"

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

OuterFloorRecoveryResult apply_outer_floor_recovery(
    PlayerState& player,
    const FixedPosition& query_position,
    const PositionCollisionQuery& query,
    bool restart_at_start,
    const std::function<void(const PositionCollisionHit&)>&
        on_external_service) {
    OuterFloorRecoveryResult result{};
    if (player.outer_floor_recovery_blocked() || !query) {
        result.gated = true;
        return result;
    }

    const auto y_offset = [&query_position](std::int32_t delta) {
        FixedPosition point = query_position;
        point[1] = static_cast<std::int32_t>(
            static_cast<std::int64_t>(point[1]) + delta);
        return point;
    };
    const std::optional<PositionCollisionHit> upward_hit = query(
        query_position,
        y_offset(0x1f40000));
    result.upward_hit = upward_hit.has_value();
    if (upward_hit.has_value()) {
        player.set_outer_floor_distance(static_cast<std::int32_t>(
            static_cast<std::int64_t>(upward_hit->position[1])
            - player.position()[1]
            - 0x1e000));

        const std::int64_t absolute_hit_y = upward_hit->position[1] < 0
            ? -static_cast<std::int64_t>(upward_hit->position[1])
            : static_cast<std::int64_t>(upward_hit->position[1]);
        if (restart_at_start && absolute_hit_y < 6000) {
            FixedPosition restart_start = y_offset(-0x7d0000);
            const FixedPosition restart_end = query_position;
            std::optional<PositionCollisionHit> restart_hit = query(
                restart_start,
                restart_end);
            result.restart_probe_hit = restart_hit.has_value();
            if (restart_hit.has_value()
                && (restart_hit->raw_collision_word & 0x40000U) != 0) {
                restart_start[1] = static_cast<std::int32_t>(
                    static_cast<std::int64_t>(restart_hit->position[1])
                    + 0x6000);
                restart_hit = query(restart_start, restart_end);
                if (!restart_hit.has_value()
                    || (restart_hit->raw_collision_word & 0x40000U) != 0) {
                    result.restart_probe_rejected = true;
                    restart_hit.reset();
                }
            }
            if (restart_hit.has_value()) {
                const FixedPosition reference =
                    player.outer_floor_reference_position();
                player.set_position(reference);
                player.set_previous_position_only(reference);
                player.apply_ground_response_yaw(0x4b0);
                result.position_changed = player.position() != query_position;
                result.response_yaw_applied = true;
                player.set_outer_floor_reference_position(player.position());
                return result;
            }
        }
        if (on_external_service) {
            on_external_service(*upward_hit);
            result.external_service_requested = true;
        }
        player.set_outer_floor_reference_position(player.position());
        return result;
    }

    const std::optional<PositionCollisionHit> short_recovery_hit = query(
        y_offset(-0x12c000),
        y_offset(0x64000));
    result.short_recovery_hit = short_recovery_hit.has_value();
    if (short_recovery_hit.has_value()) {
        const FixedPosition recovered{
            short_recovery_hit->position[0],
            static_cast<std::int32_t>(
                static_cast<std::int64_t>(short_recovery_hit->position[1])
                - 0x1e000),
            short_recovery_hit->position[2],
        };
        player.set_position(recovered);
        player.set_previous_position_only(recovered);
        result.position_changed = recovered != query_position;
        player.set_outer_floor_reference_position(player.position());
        return result;
    }

    const FixedPosition original_position = player.position();
    const FixedPosition original_previous = player.previous_position();
    const FixedPosition reference = player.outer_floor_reference_position();

    FixedPosition x_start = query_position;
    x_start[0] = reference[0];
    FixedPosition x_end = x_start;
    x_end[1] = static_cast<std::int32_t>(
        static_cast<std::int64_t>(x_end[1]) + 0x190000);
    // The retail helper exposes the trial X immediately before querying it.
    player.set_position(FixedPosition{
        reference[0], query_position[1], query_position[2]});
    player.set_previous_position_only(FixedPosition{
        reference[0], original_previous[1], original_previous[2]});
    const std::optional<PositionCollisionHit> x_hit = query(x_start, x_end);
    result.x_recovery_hit = x_hit.has_value();
    if (!x_hit.has_value()) {
        player.set_position(original_position);
        player.set_previous_position_only(original_previous);

        FixedPosition z_start = query_position;
        z_start[2] = reference[2];
        FixedPosition z_end = z_start;
        z_end[1] = static_cast<std::int32_t>(
            static_cast<std::int64_t>(z_end[1]) + 0x190000);
        // As above, this store is observable by a re-entrant query/service.
        player.set_position(FixedPosition{
            original_position[0], query_position[1], reference[2]});
        player.set_previous_position_only(FixedPosition{
            original_previous[0], original_previous[1], reference[2]});
        const std::optional<PositionCollisionHit> z_hit = query(z_start, z_end);
        result.z_recovery_hit = z_hit.has_value();
        if (!z_hit.has_value()) {
            const std::int32_t response_half =
                -player.collision_response()[2] / 2;
            player.set_position(FixedPosition{
                reference[0], query_position[1], reference[2]});
            player.set_previous_position_only(player.position());
            FixedPosition response = player.collision_response();
            response[0] = response_half;
            response[2] = response_half / 2;
            player.set_collision_response(response);
            result.position_changed = player.position() != original_position;
        }
    }
    player.set_outer_floor_reference_position(player.position());
    return result;
}

} // namespace opentony::runtime
