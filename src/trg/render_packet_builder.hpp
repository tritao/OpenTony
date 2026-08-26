#pragma once

#include "level_render_snapshot.hpp"
#include "../camera/camera_system.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace opentony::trg {

class RenderPacketError final : public std::runtime_error {
public:
    explicit RenderPacketError(const char* message)
        : std::runtime_error(message) {}
};

inline constexpr std::uint8_t kRenderPolygonPacketFormat = 0xb0;
inline constexpr std::size_t kRenderPolygonRecordSize = 0x30;

// The retail renderer's projection helper consumes a transformed working
// vertex, not a PSX face directly. Keep that handoff as a callback until the
// remaining viewport/FOV producer is calibrated against a live frame.
struct RenderViewVertexInput {
    std::array<std::int32_t, 3> position_q16{};
    std::uint32_t source_flags{};
};

struct RenderProjectedVertex {
    float x{};
    float y{};
    float z{};
    float reciprocal_depth{};
    std::uint32_t clip_flags{};
    float auxiliary{};
};

using RenderProjector =
    std::function<RenderProjectedVertex(const RenderViewVertexInput&)>;

struct RenderTextureDimensions {
    std::uint32_t width{};
    std::uint32_t height{};
};

using RenderTextureDimensionResolver = std::function<
    std::optional<RenderTextureDimensions>(std::size_t material_index,
                                           std::uint32_t material_checksum)>;

struct RenderWorkingVertex {
    RenderViewVertexInput input{};
    RenderProjectedVertex projected{};
};

struct RenderPolygonVertex {
    RenderProjectedVertex projected{};
    std::uint32_t color{};
    std::array<float, 2> uv{};
    std::array<std::uint16_t, 2> source_uv{};
    bool uv_normalized{};
};

struct RenderPolygonPacket {
    std::size_t entity{};
    std::size_t object_index{CommandPointRuntime::npos};
    std::size_t model_index{CommandPointRuntime::npos};
    std::size_t face_index{};
    std::uint8_t format{};
    std::uint16_t flags{};
    std::size_t material_index{CommandPointRuntime::npos};
    std::uint32_t material_checksum{};
    bool textured{};
    std::uint8_t vertex_count{};
    std::size_t working_vertex_offset{};
    std::vector<RenderPolygonVertex> vertices;
};

struct RenderPacketBuildOptions {
    std::uint32_t color{0xffffffffU};
    float half_texel{0.5F};
    RenderTextureDimensionResolver texture_dimensions;
};

struct RenderPacketBuildResult {
    std::vector<RenderWorkingVertex> working_vertices;
    std::vector<RenderPolygonPacket> polygons;
};

class RenderPacketBuilder final {
public:
    static RenderPolygonPacket build_face(
        const LevelRenderFaceSnapshot& face,
        const std::array<std::int32_t, 3>& object_position_q16,
        const camera::CameraStateRaw& camera,
        const RenderProjector& projector,
        const RenderPacketBuildOptions& options = {});

    static RenderPacketBuildResult build(
        const LevelRenderSnapshot& snapshot,
        const camera::CameraStateRaw& camera,
        const RenderProjector& projector,
        const RenderPacketBuildOptions& options = {});

    // The snapshot overload delegates here so deterministic tests can exercise
    // the renderer-facing traversal contract without loading a retail asset.
    // Input face order is the native adapter's submission order.
    static RenderPacketBuildResult build(
        std::span<const LevelRenderEntitySnapshot> entities,
        std::span<const LevelRenderFaceSnapshot> faces,
        const camera::CameraStateRaw& camera,
        const RenderProjector& projector,
        const RenderPacketBuildOptions& options = {});
};

} // namespace opentony::trg
