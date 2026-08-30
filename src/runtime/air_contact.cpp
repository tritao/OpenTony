#include "air_contact.hpp"

#include <cstdint>

namespace opentony::runtime {

StandardAirContactDisposition classify_standard_air_contact(
    const PositionCollisionHit& hit,
    std::int32_t raw_state,
    bool jump_held,
    std::uint32_t jump_inactive_counter,
    std::int32_t current_frame,
    StandardAirContactInput input) noexcept {
    (void)jump_held;
    (void)jump_inactive_counter;
    // These are the exact source expressions from FUN_0048ea80. Recompute
    // them from the packed face fields so synthetic/native hit producers do
    // not need to manufacture a second copy of the decoded globals.
    const std::uint16_t surface_flags =
        static_cast<std::uint16_t>(hit.raw_collision_word >> 16U);
    const std::uint32_t material_flags = surface_flags & 0x0040U;
    const std::uint32_t material_flags_transient =
        (~hit.raw_collision_word >> 24U) & 1U;
    const std::uint32_t material_flags_secondary =
        (~hit.raw_collision_word >> 23U) & 1U;
    const std::uint32_t material_flags_contact =
        (hit.face_flags & 0x0080U) != 0 ? 0x80U : 0U;

    const std::int64_t surface_age =
        static_cast<std::int64_t>(current_frame)
        - static_cast<std::int64_t>(input.last_surface_frame);
    // FUN_00497f40 rejects the hit at each of these branches.  The previous
    // native implementation accidentally treated the reject conditions as
    // an acceptance gate, which made a non-material air contact land in the
    // native replay even though retail retained the in-air state.  Keep the
    // branch polarity explicit: every condition below is required to survive
    // the retail rejection path.
    if (material_flags_transient != 0
        && material_flags != 0
        && material_flags_contact == 0) {
        return StandardAirContactDisposition::None;
    }
    if (material_flags_secondary == 0
        || input.prephysics_blocked
        || surface_age <= 0x28) {
        return StandardAirContactDisposition::None;
    }
    if (material_flags_transient == 0
        && (material_flags == 0 || raw_state != 3)) {
        return StandardAirContactDisposition::None;
    }
    return material_flags != 0
        ? StandardAirContactDisposition::SurfaceRecovery
        : StandardAirContactDisposition::Landing;
}

bool accepts_standard_air_contact(
    const PositionCollisionHit& hit,
    std::int32_t raw_state,
    bool jump_held,
    std::uint32_t jump_inactive_counter,
    std::int32_t current_frame,
    StandardAirContactInput input) noexcept {
    return classify_standard_air_contact(
        hit,
        raw_state,
        jump_held,
        jump_inactive_counter,
        current_frame,
        input) != StandardAirContactDisposition::None;
}

} // namespace opentony::runtime
