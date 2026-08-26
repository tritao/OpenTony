#pragma once

#include "level_scene_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace opentony::trg {

struct LevelRenderFaceSnapshot {
    std::size_t entity{};
    std::size_t object_index{CommandPointRuntime::npos};
    std::size_t model_index{CommandPointRuntime::npos};
    std::size_t model_face_index{};
    std::uint16_t flags{};
    std::uint16_t surface_flags{};
    std::uint32_t raw_collision_word{};
    std::uint32_t texture_index{};
    bool has_texture{};
    std::uint8_t vertex_count{};
    std::uint8_t uv_count{};
    std::array<std::array<std::int16_t, 3>, 4> local_vertices{};
    std::array<std::int16_t, 3> normal{};
    std::array<std::array<std::uint16_t, 2>, 4> uv{};
};

// One renderer-facing entity record. Position remains in the PSX object's
// fixed-point representation; face vertices remain in the model's raw
// signed-short units. The backend owns object transforms and projection.
struct LevelRenderEntitySnapshot {
    std::size_t entity{};
    LevelSceneEntityKind kind{LevelSceneEntityKind::StaticScene};
    std::size_t source_node{CommandPointRuntime::npos};
    std::size_t psx_object_index{CommandPointRuntime::npos};
    std::size_t model_index{CommandPointRuntime::npos};
    std::uint32_t model_name{};
    std::array<std::int32_t, 3> object_position{};
    std::array<std::uint16_t, 3> orientation{};
    bool has_orientation{};
    std::uint32_t asset_flags{};
    std::uint16_t gameplay_flags{};
    bool active{true};
    bool suspended{};
    bool alive{true};
    bool killed{};
    bool visible_commanded{};
    std::size_t first_face{CommandPointRuntime::npos};
    std::size_t face_count{};
};

// Renderer-independent copy of the confirmed game-owned object/model
// submission boundary. It contains no camera, FOV, rasterizer, or texture
// upload policy; those remain backend responsibilities.
class LevelRenderSnapshot final {
public:
    static LevelRenderSnapshot build(
        const LevelSceneRegistry& scene,
        const assets::PsxArchive& archive);

    [[nodiscard]] const std::vector<LevelRenderEntitySnapshot>& entities() const noexcept {
        return entities_;
    }
    [[nodiscard]] const std::vector<LevelRenderFaceSnapshot>& faces() const noexcept {
        return faces_;
    }

private:
    std::vector<LevelRenderEntitySnapshot> entities_;
    std::vector<LevelRenderFaceSnapshot> faces_;
};

} // namespace opentony::trg
