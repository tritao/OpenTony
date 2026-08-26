#pragma once

#include "position_commit.hpp"
#include "../assets/psx_collision.hpp"

#include <optional>

namespace opentony::runtime {

// Adapter for the retail position-query shape: each candidate is tested as a
// fixed-point segment from the player's current position to that candidate.
// operator() remains an occupancy-compatible PositionCollisionProbe; hit() and
// query() expose the normal/material result for response policies.
class PsxPositionCollisionProbe final {
public:
    PsxPositionCollisionProbe(
        const assets::PsxCollisionWorld& world,
        FixedPosition start) noexcept
        : world_(world), start_(start) {}

    [[nodiscard]] bool operator()(const FixedPosition& candidate) const;

    [[nodiscard]] std::optional<assets::PsxCollisionHit> hit(
        const FixedPosition& candidate) const;

    [[nodiscard]] std::optional<PositionCollisionHit> query(
        const FixedPosition& candidate) const;

private:
    const assets::PsxCollisionWorld& world_;
    FixedPosition start_{};
};

} // namespace opentony::runtime
