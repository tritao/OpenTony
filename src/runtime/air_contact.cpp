#include "air_contact.hpp"

#include <cstdint>

namespace opentony::runtime {

bool accepts_standard_air_contact(
    const PositionCollisionHit& hit,
    std::int32_t raw_state,
    bool jump_held,
    std::uint32_t jump_inactive_counter,
    std::int32_t current_frame,
    StandardAirContactInput input) noexcept {
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
    const bool landing_gate =
        (material_flags_transient != 0
            && (material_flags == 0 || material_flags_contact == 0))
        || material_flags_secondary == 0
        || input.prephysics_blocked
        || (!jump_held && jump_inactive_counter > 0x13U)
        || surface_age < 0x29;
    if (!landing_gate) {
        return false;
    }
    return material_flags_transient != 0
        || (material_flags != 0 && raw_state == 3);
}

} // namespace opentony::runtime
