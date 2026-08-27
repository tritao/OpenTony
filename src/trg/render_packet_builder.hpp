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
inline constexpr std::size_t kRenderViewRecordSize = 0x14;
inline constexpr std::size_t kRenderPolygonArenaSlotSize = 0xc0;
inline constexpr std::size_t kRenderDepthBucketCount = 0x1000;
inline constexpr std::size_t kRenderNoPolygonIndex =
    std::numeric_limits<std::size_t>::max();

struct RenderPolygonArenaAllocation {
    std::size_t slot_offset{};
    std::size_t cursor_before{};
    std::size_t cursor_after{};
};

// The retail polygon arena is a cursor over a per-buffer byte range. The
// view record is placed first; every polygon construction attempt then
// reserves one fixed 0xc0-byte slot, even though its stable packet prefix is
// only 0x30 bytes. A failed attempt owns no bucket link and rolls back only
// that most recent reservation.
class RenderPolygonArena final {
public:
    RenderPolygonArena(std::size_t initial_cursor, std::size_t end_cursor);

    [[nodiscard]] std::size_t cursor() const noexcept;
    [[nodiscard]] std::size_t end() const noexcept;

    // Models M3D_BeginRenderView's +0x14 control/view record consumption.
    [[nodiscard]] std::size_t begin_view_record();

    // Retail tests cursor < end, then advances by 0xc0. The prepared arena
    // end is slot-aligned, so this preserves the observed condition exactly.
    [[nodiscard]] std::optional<RenderPolygonArenaAllocation>
    allocate_polygon();

    // Publishes a successful slot. Pending slots are construction attempts;
    // only committed slots model storage that may be linked and dispatched.
    void commit_polygon(const RenderPolygonArenaAllocation& allocation);

    [[nodiscard]] bool has_live_polygon(
        std::size_t slot_offset) const noexcept;
    [[nodiscard]] std::size_t live_polygon_count() const noexcept;

    // Models the caller-side rollback in 0x004d1d40/0x004d20f0. It does not
    // reclaim an accepted packet or alter any already-published bucket link.
    void rollback_polygon();

private:
    std::size_t polygon_cursor_{};
    std::size_t cursor_{};
    std::size_t end_{};
    std::vector<std::size_t> pending_slots_;
    std::vector<std::size_t> live_slots_;
};

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

// 0x004d2310 receives a packet whose projected reciprocal-depth field still
// contains the ordinary transform's reciprocal. The retail viewport record is
// laid out as [right, bottom, left, top] at 0x00563a38.
struct RenderNearClipOptions {
    float near_depth{10.0F};
    float screen_center_x{320.0F};
    float screen_center_y{240.0F};
    float depth_scale{384.0F};
    std::array<float, 4> viewport_edges{640.0F, 480.0F, 0.0F, 0.0F};
    float projection_unit_scale{1.0F};
};

enum class RenderNearClipDisposition : std::uint8_t {
    accepted,
    rejected,
};

struct RenderNearClipResult {
    RenderNearClipDisposition disposition{RenderNearClipDisposition::rejected};
    std::uint8_t output_vertex_count{};
    std::uint32_t all_lateral_clip_flags{};
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

// Inputs resolved by M3D_ClipAndBucketPolygon (0x004d20f0) immediately before
// it calls M3D_ClassifyPolygonDepth (0x004d26b0). The three offsets are the
// renderer-state values copied into 0x00563a64, 0x00563a7c, and 0x00563a88;
// material_runtime_flags is supplied by the already-resolved runtime material
// record rather than parsed by this renderer adapter.
struct RenderDepthStateInputs {
    std::uint32_t renderer_state_word{};
    float depth_offset_2000{};
    float depth_offset_4000{};
    float depth_offset_6000{};
    std::uint32_t current_level{};
    std::uint32_t material_runtime_flags{};
};

struct RenderDepthStateResolution {
    std::uint32_t selected_state_mask{};
    float depth_offset{};
    std::uint32_t packet_flags{};
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

    // Models 0x004d2310 and its 0x004d25c0 edge-intersection helper. The
    // packet is replaced with the generated stream; a rejected result retains
    // the target's output count (including counts below three), while the
    // lateral-trivial-reject case exposes count zero to native callers.
    static RenderNearClipResult clip_near_plane(
        RenderPolygonPacket& polygon,
        const RenderNearClipOptions& options = {});

    // Reconstructs the caller-owned state resolution at 0x004d20f0. The
    // offset selection uses (renderer_state_word & 0x6000) with exact matches
    // for 0x2000, 0x4000, and 0x6000; all other values select zero. The packet
    // flag rewrite is the post-0x004d26b0 textured-material exception.
    static RenderDepthStateResolution resolve_depth_state(
        const RenderPolygonPacket& polygon,
        const RenderDepthStateInputs& inputs);

    static RenderBucketDecision classify_polygon(
        RenderPolygonPacket& polygon,
        const RenderPolygonClipSummary& clip,
        const RenderDepthBucketOptions& options = {});

    static RenderBucketBuildResult bucketize(
        std::span<RenderPolygonPacket> polygons,
        const RenderDepthBucketOptions& options = {});
};

} // namespace opentony::trg
