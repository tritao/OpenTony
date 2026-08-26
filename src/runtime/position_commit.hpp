#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

namespace opentony::runtime {

using FixedPosition = std::array<std::int32_t, 3>;
using PositionCollisionProbe = std::function<bool(const FixedPosition&)>;

// Stable collision metadata shared by the native physics boundary. The PSX
// adapter converts its asset-specific hit record into this representation;
// gameplay code can then consume normals/materials without depending on the
// asset parser.
struct PositionCollisionHit {
    std::size_t face_index{};
    std::size_t object_index{};
    std::size_t model_index{};
    std::size_t model_face_index{};
    std::uint32_t hit_parameter_q14{};
    FixedPosition position{};
    FixedPosition normal{};
    std::uint16_t face_flags{};
    std::uint16_t surface_flags{};
    std::uint32_t raw_collision_word{};
    bool surface_bit_6{};
    bool surface_bit_7_clear{};
    bool surface_bit_8_clear{};
    std::uint8_t raw_type_bits_9_12{};
    bool face_flag_80{};

    friend bool operator==(
        const PositionCollisionHit&,
        const PositionCollisionHit&) = default;
};

// FUN_00497f40's first ground-vs-non-ground contact split compares the
// collision normal's Y short against 0xccd (strictly greater). Keep this
// predicate independent from trigger/material policy, which is owned by the
// collision query and its face-mask options.
inline constexpr std::int32_t kRetailGroundContactNormalYQ12 = 0xccd;

[[nodiscard]] bool accepts_retail_ground_contact(
    const PositionCollisionHit& hit) noexcept;

using PositionCollisionQuery = std::function<std::optional<PositionCollisionHit>(
    const FixedPosition& start,
    const FixedPosition& end)>;

struct PositionCommitResult {
    FixedPosition position{};
    bool collided{};
    bool blocked{};
    std::uint8_t probes{};

    friend bool operator==(
        const PositionCommitResult&,
        const PositionCommitResult&) = default;
};

// Faithful control-flow model of retail FUN_00496060 (the shared position
// commit path). Collision representation and response policy are injected;
// this class only preserves the observed axis-fallback ordering.
class PositionCommitter final {
public:
    [[nodiscard]] static PositionCommitResult commit(
        FixedPosition current,
        FixedPosition desired,
        const PositionCollisionProbe& probe,
        bool bypass_collision = false);
};

} // namespace opentony::runtime
