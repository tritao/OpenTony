#pragma once

#include "position_commit.hpp"

#include <cstdint>

namespace opentony::runtime {

// Inputs owned by the player/frame side of the retail predicate. Collision
// geometry and the surface decode are supplied by PositionCollisionHit.
struct StandardAirContactInput final {
    bool prephysics_blocked{};
    std::int32_t last_surface_frame{};
};

// Exact ordinary landing predicate observed after the in-air collision result
// and material decode. This does not classify walls, rails, or special
// contacts; callers choose those paths separately.
[[nodiscard]] bool accepts_standard_air_contact(
    const PositionCollisionHit& hit,
    std::int32_t raw_state,
    bool jump_held,
    std::uint32_t jump_inactive_counter,
    std::int32_t current_frame,
    StandardAirContactInput input = {}) noexcept;

} // namespace opentony::runtime
