#pragma once

#include "position_commit.hpp"
#include "../assets/psx_collision.hpp"
#include "../collision/psx_scene.hpp"

#include <optional>
#include <span>

namespace opentony::runtime {

// Adapter for the retail position-query shape: each candidate is tested as a
// fixed-point segment from the player's current position to that candidate.
// operator() remains an occupancy-compatible PositionCollisionProbe; hit() and
// query() expose the normal/material result for response policies.
class PsxPositionCollisionProbe final {
public:
    PsxPositionCollisionProbe(
        const assets::PsxCollisionWorld& world,
        FixedPosition start,
        assets::PsxCollisionQueryOptions options = {}) noexcept
        : world_(world), start_(start), options_(options) {}

    [[nodiscard]] bool operator()(const FixedPosition& candidate) const;

    [[nodiscard]] std::optional<assets::PsxCollisionHit> hit(
        const FixedPosition& candidate) const;

    [[nodiscard]] std::optional<PositionCollisionHit> query(
        const FixedPosition& candidate) const;

private:
    const assets::PsxCollisionWorld& world_;
    FixedPosition start_{};
    assets::PsxCollisionQueryOptions options_{};
};

// Adapter for the evidence-backed fixed-point scene query. This deliberately
// lives beside the older asset-world adapter so callers can compare both
// implementations at the same physics frame boundary.
class PsxScenePositionCollisionProbe final {
public:
    PsxScenePositionCollisionProbe(
        const collision::PsxScene& scene,
        FixedPosition start,
        collision::CollisionFaceFilter filter = {},
        std::span<const collision::PsxLinkedCollisionObject> linked_objects = {}) noexcept
        : scene_(scene), start_(start), filter_(filter),
          linked_objects_(linked_objects) {}

    [[nodiscard]] bool operator()(const FixedPosition& candidate) const;

    [[nodiscard]] std::optional<PositionCollisionHit> query(
        const FixedPosition& candidate) const;

private:
    const collision::PsxScene& scene_;
    FixedPosition start_{};
    collision::CollisionFaceFilter filter_{};
    std::span<const collision::PsxLinkedCollisionObject> linked_objects_{};
};

} // namespace opentony::runtime
