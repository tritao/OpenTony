#pragma once

#include "psx_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace opentony::assets {

// Conservative view of the bits read by the retail face-mask helper around
// FUN_0048ea80. Names intentionally describe storage/predicate state rather
// than assigning unresolved gameplay meanings.
struct PsxCollisionMaskView {
    std::uint16_t normal_index{};
    std::uint16_t surface_flags{};
    bool surface_bit_6{};
    bool surface_bit_7_clear{};
    bool surface_bit_8_clear{};
    std::uint8_t raw_type_bits_9_12{};
    bool face_flag_80{};

    friend bool operator==(
        const PsxCollisionMaskView&,
        const PsxCollisionMaskView&) = default;
};

// Query-time face-word policy recovered from FUN_004660b0/FUN_00462a20.
// `accept_mask` retains the retail OR-mask spelling: every bit clear in it
// must be present in the raw face word. The unresolved global masks remain
// caller-owned so a gameplay mode can supply the values it observed.
struct PsxCollisionQueryOptions {
    bool apply_retail_face_filter{false};
    bool apply_retail_plane_test{false};
    std::uint32_t reject_mask{};
    std::uint32_t accept_mask{0xffffffffU};
    bool include_trigger_faces{false};
};

// Inputs used by the retail startup code around FUN_004660b0 when it builds
// DAT_00567a60/DAT_00567a68. The offsets are deliberately retained because
// their gameplay meanings are not proven yet.
struct RetailCollisionFilterInputs {
    bool dat_00567c84{};
    bool dat_00567c7c{};
    bool dat_00567c78{};
    bool dat_00567c74{};
    bool dat_00567c80{};
};

// Builds the exact face-word portion of the retail query policy. Trigger-face
// inclusion is not folded into this helper: retail decides it from a separate
// per-query object field, represented by PsxCollisionQueryOptions' caller-owned
// `include_trigger_faces` flag.
[[nodiscard]] PsxCollisionQueryOptions make_retail_collision_query_options(
    RetailCollisionFilterInputs inputs,
    bool apply_retail_plane_test = false) noexcept;

[[nodiscard]] bool accepts_retail_collision_face(
    std::uint32_t raw_collision_word,
    const PsxCollisionQueryOptions& options) noexcept;

[[nodiscard]] PsxCollisionMaskView decode_collision_mask(
    std::uint32_t raw_collision_word,
    std::uint16_t face_flags) noexcept;

struct PsxCollisionFace {
    std::size_t object_index{};
    std::size_t model_index{};
    std::size_t model_face_index{};
    // The static PC query keeps face geometry/model normals in model space,
    // then 0x00463d50 rotates the winning normal through the object's
    // +0x14/+0x16/+0x18 collision-angle shorts.  Retain those source angles
    // with the flattened face so the legacy adapter can publish the same
    // result contract as the recovered scene adapter.
    std::array<std::int16_t, 3> collision_angles{};
    std::array<std::array<std::int32_t, 3>, 4> vertices{};
    std::array<std::int16_t, 3> normal{};
    std::uint16_t face_flags{};
    std::uint16_t surface_flags{};
    std::uint32_t raw_collision_word{};
    std::uint8_t vertex_count{};
};

struct PsxCollisionCell {
    std::uint32_t unknown_1{};
    std::uint32_t unknown_2{};
    std::vector<std::size_t> faces;
};

struct PsxCollisionGrid {
    std::size_t source_blockmap{};
    std::array<std::int32_t, 4> bounds{};
    std::array<std::uint16_t, 2> cell_counts{};
    std::vector<PsxCollisionCell> cells;
};

struct PsxCollisionHit {
    std::size_t face_index{};
    std::size_t object_index{};
    std::size_t model_index{};
    std::size_t model_face_index{};
    std::uint32_t hit_parameter_q14{}; // retail q+0x8c, 0..0x4000
    double fraction{};
    std::array<std::int32_t, 3> position{};
    std::array<std::int16_t, 3> normal{};
    std::uint16_t face_flags{};
    std::uint16_t surface_flags{};
    std::uint32_t raw_collision_word{}; // source face +0x0c

    [[nodiscard]] PsxCollisionMaskView collision_mask() const noexcept {
        return decode_collision_mask(raw_collision_word, face_flags);
    }
};

// Renderer- and physics-policy-independent collision geometry. It reproduces
// the PSX blockmap's object selection and world-space fixed-point placement;
// callers decide how face flags and surface words affect skater movement or
// raycasts. In particular, face flag 0x80 is retained as metadata rather than
// being treated as an inferred geometry-exclusion bit.
class PsxCollisionWorld final {
public:
    static PsxCollisionWorld build(const PsxArchive& archive);

    [[nodiscard]] const std::vector<PsxCollisionFace>& faces() const noexcept {
        return faces_;
    }
    [[nodiscard]] const std::vector<PsxCollisionGrid>& grids() const noexcept {
        return grids_;
    }
    [[nodiscard]] std::size_t referenced_object_count() const noexcept {
        return referenced_object_count_;
    }
    [[nodiscard]] std::size_t face_count() const noexcept { return faces_.size(); }

    // Returns broad-phase face IDs whose blockmap cells overlap the fixed-12
    // X/Z query box. A negative radius is rejected rather than normalized.
    [[nodiscard]] std::vector<std::size_t> candidate_faces(
        std::array<std::int32_t, 3> center,
        std::int32_t radius) const;

    // Conservative geometric segment test over the blockmap candidates. This
    // reports the nearest triangle/quad intersection but does not apply
    // surface response, slope limits, or skater-radius rules.
    [[nodiscard]] std::optional<PsxCollisionHit> trace_segment(
        std::array<std::int32_t, 3> start,
        std::array<std::int32_t, 3> end,
        const PsxCollisionQueryOptions& options = {}) const;

private:
    std::vector<PsxCollisionFace> faces_;
    std::vector<PsxCollisionGrid> grids_;
    std::size_t referenced_object_count_{};

    [[nodiscard]] static std::optional<std::size_t> cell_axis(
        std::int32_t value,
        std::int32_t minimum,
        std::int32_t maximum,
        std::uint16_t count) noexcept;
    [[nodiscard]] static bool segment_triangle(
        const std::array<double, 3>& start,
        const std::array<double, 3>& direction,
        const std::array<double, 3>& a,
        const std::array<double, 3>& b,
        const std::array<double, 3>& c,
        double& fraction) noexcept;
};

} // namespace opentony::assets
