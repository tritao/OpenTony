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

// Causal random results consumed by the non-landing in-air normal-recovery
// branch. Geometry, position, response, state, and orientation remain native
// producers; only this external service boundary is supplied by replay.
struct AirNormalRecoveryInput final {
    std::int32_t gate_random{};
    std::int32_t x_random{};
    std::int32_t z_random{};
};

enum class StandardAirContactDisposition : std::uint8_t {
    None,
    Landing,
    SurfaceRecovery,
};

// Classify the accepted branch after the packed material/state gate. A
// surface-recovery contact is accepted by the in-air handler but returns
// through FUN_00497aa0, so it must not be treated as an ordinary landing.
[[nodiscard]] StandardAirContactDisposition classify_standard_air_contact(
    const PositionCollisionHit& hit,
    std::int32_t raw_state,
    bool jump_held,
    std::uint32_t jump_inactive_counter,
    std::int32_t current_frame,
    StandardAirContactInput input = {}) noexcept;

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
