#pragma once

#include "level_render_snapshot.hpp"
#include "../camera/camera_system.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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
inline constexpr std::size_t kRenderDepthBucketCount = 0x1000;
inline constexpr std::size_t kRenderNoPolygonIndex =
    std::numeric_limits<std::size_t>::max();

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
    // Retail stores the complete state word at record +0x08. In particular,
    // the forced-depth path sets bit 31, so this is intentionally wider than
    // the source face's usual low flag bits.
    std::uint32_t flags{};
    std::size_t material_index{CommandPointRuntime::npos};
    std::uint32_t material_checksum{};
    bool textured{};
    std::uint8_t vertex_count{};
    std::size_t working_vertex_offset{};
    std::vector<RenderPolygonVertex> vertices;
};

enum class RenderBucketDisposition : std::uint8_t {
    accepted,
    rejected_clip,
    rejected_winding,
    requires_near_clip,
};

struct RenderPolygonClipSummary {
    std::uint32_t all_flags{};
    std::uint32_t any_flags{};
};

// These are the resolved inputs to 0x004d26b0. Retail derives them from the
// renderer state word, material flags, texture mode, and level. Keeping that
// resolution outside this helper makes the native contract testable without
// claiming ownership of camera or asset-runtime state.
enum class RenderDepthBucketMode : std::uint8_t {
    base = 0,
    scaled_nearest = 1,
    scaled_farthest = 2,
    offset_farthest = 3,
    forced_offset = 4,
};

struct RenderDepthBucketOptions {
    RenderDepthBucketMode mode{RenderDepthBucketMode::base};
    float depth_offset{};
    float mode3_offset{};
    float mode2_multiplier{0.7F};
    bool choose_nearest_depth{};
    bool display_rect_depth{};
    bool reverse_winding{};
    float winding_threshold{};
    float near_depth{10.0F};
    float reciprocal_depth_cap{0.99F};
};

struct RenderBucketDecision {
    RenderBucketDisposition disposition{RenderBucketDisposition::rejected_clip};
    std::size_t bucket_index{kRenderNoPolygonIndex};
    float first_winding_determinant{};
    float second_winding_determinant{};
    float selected_depth{};
};

struct RenderBucketBuildResult {
    // These indices model the retail pointer links without exposing the
    // process-local polygon arena. Each accepted polygon prepends to its
    // bucket head, so `next_polygon` is the exact per-bucket link chain.
    std::array<std::size_t, kRenderDepthBucketCount> bucket_heads{};
    std::vector<std::size_t> next_polygon;
    std::vector<RenderBucketDecision> decisions;
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

    // Models the 0x004d1d40 -> 0x004d20f0 boundary and the lower
    // 0x004d26b0 depth classifier. Near-plane clipping itself remains a
    // separate native seam; a partial near clip is reported rather than
    // silently accepted.
    static RenderPolygonClipSummary summarize_clip(
        const RenderPolygonPacket& polygon);

    // 0x004d1d40 emits these two packets for an eligible textured quad before
    // invoking 0x004d20f0. The caller supplies the already-resolved view-state
    // condition because the global display/view selector is outside this
    // portable packet type.
    static std::vector<RenderPolygonPacket> split_textured_quad(
        const RenderPolygonPacket& polygon,
        bool split);

    static RenderBucketDecision classify_polygon(
        RenderPolygonPacket& polygon,
        const RenderPolygonClipSummary& clip,
        const RenderDepthBucketOptions& options = {});

    static RenderBucketBuildResult bucketize(
        std::span<RenderPolygonPacket> polygons,
        const RenderDepthBucketOptions& options = {});
};

} // namespace opentony::trg
