#include "render_packet_builder.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace opentony::trg {
namespace {

[[nodiscard]] std::int32_t add_s32(std::int64_t left, std::int64_t right) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(left + right));
}

[[nodiscard]] std::array<std::int32_t, 3> view_position(
    const LevelRenderFaceSnapshot& face,
    std::size_t corner,
    const std::array<std::int32_t, 3>& object_position_q16,
    const camera::CameraStateRaw& camera) {
    const auto& local = face.local_vertices[corner];
    const std::array<std::int32_t, 3> world{
        add_s32(object_position_q16[0],
                static_cast<std::int64_t>(local[0]) << 12),
        add_s32(object_position_q16[1],
                static_cast<std::int64_t>(local[1]) << 12),
        add_s32(object_position_q16[2],
                static_cast<std::int64_t>(local[2]) << 12),
    };
    const std::array<std::int32_t, 3> relative{
        add_s32(world[0], -static_cast<std::int64_t>(camera.position.x)),
        add_s32(world[1], -static_cast<std::int64_t>(camera.position.y)),
        add_s32(world[2], -static_cast<std::int64_t>(camera.position.z)),
    };
    const camera::MatrixQ12 matrix =
        camera::camera_view_record_matrix_q12(camera.current_transform);
    const camera::Q12Vec3 transformed = camera::transform_matrix_q12_trunc(
        matrix, relative);
    return {transformed.x, transformed.y, transformed.z};
}

[[nodiscard]] std::array<float, 2> normalized_uv(
    const std::array<std::uint16_t, 2>& uv,
    const RenderTextureDimensions& dimensions,
    float half_texel) {
    return {
        (static_cast<float>(uv[0]) + half_texel)
            / static_cast<float>(dimensions.width),
        (static_cast<float>(uv[1]) + half_texel)
            / static_cast<float>(dimensions.height),
    };
}

[[nodiscard]] float store_f32(float value) {
    // The retail code stores each intermediate through an x87 f32 slot. The
    // volatile local prevents a host compiler from retaining excess
    // precision across the same boundary.
    volatile float stored = value;
    return stored;
}

[[nodiscard]] float store_f32(long double value) {
    volatile float stored = static_cast<float>(value);
    return stored;
}

void validate_polygon(const RenderPolygonPacket& polygon) {
    if (polygon.vertex_count != 3 && polygon.vertex_count != 4) {
        throw RenderPacketError(
            "render polygon must contain three or four vertices");
    }
    if (polygon.vertices.size() != polygon.vertex_count) {
        throw RenderPacketError(
            "render polygon vertex count does not match its record");
    }
}

[[nodiscard]] long double winding_determinant(
    const RenderPolygonPacket& polygon,
    std::size_t origin_index,
    std::size_t first,
    std::size_t last,
    bool reverse_operands = false) {
    const RenderProjectedVertex& origin =
        polygon.vertices[origin_index].projected;
    const RenderProjectedVertex& edge_first =
        polygon.vertices[first].projected;
    const RenderProjectedVertex& edge_last =
        polygon.vertices[last].projected;
    const long double first_x =
        static_cast<long double>(edge_first.x) - origin.x;
    const long double first_y =
        static_cast<long double>(edge_first.y) - origin.y;
    const long double last_x =
        static_cast<long double>(edge_last.x) - origin.x;
    const long double last_y =
        static_cast<long double>(edge_last.y) - origin.y;

    // This is the retail operand order: last_y * first_x - last_x * first_y.
    const long double determinant = last_y * first_x - last_x * first_y;
    return reverse_operands ? -determinant : determinant;
}

[[nodiscard]] std::size_t depth_bucket(float selected_depth) {
    // 0x005004f4 (__ftol) truncates toward zero. Do the range checks before
    // converting to an integer so extreme test fixtures remain defined on the
    // host while retaining the retail 0..0xfff result.
    const long double rounded = static_cast<long double>(selected_depth) + 0.5L;
    if (!(rounded > 0.0L)) {
        return 0;
    }
    if (rounded >= 16384.0L) {
        return kRenderDepthBucketCount - 1;
    }
    const long double truncated = std::trunc(rounded);
    if (!(truncated > 0.0L)) {
        return 0;
    }
    const auto quantized = static_cast<std::size_t>(truncated) >> 2;
    return std::min(quantized, kRenderDepthBucketCount - 1);
}

void adjust_depth(
    const RenderDepthBucketOptions& options,
    float& multiplier,
    float& offset,
    bool& choose_nearest) {
    multiplier = 1.0F;
    offset = options.depth_offset;
    choose_nearest = options.choose_nearest_depth;
    switch (options.mode) {
    case RenderDepthBucketMode::base:
        break;
    case RenderDepthBucketMode::scaled_nearest:
        multiplier = 0.95F;
        break;
    case RenderDepthBucketMode::scaled_farthest:
        multiplier = options.mode2_multiplier;
        break;
    case RenderDepthBucketMode::offset_farthest:
        offset = store_f32(offset + options.mode3_offset);
        choose_nearest = false;
        break;
    case RenderDepthBucketMode::forced_offset:
        offset = 16380.0F;
        choose_nearest = false;
        break;
    }
}

[[nodiscard]] float update_depth_and_select(
    RenderPolygonPacket& polygon,
    const RenderDepthBucketOptions& options) {
    if (options.display_rect_depth) {
        float selected = polygon.vertices.front().projected.z;
        for (std::size_t index = 1; index < polygon.vertices.size(); ++index) {
            selected = std::max(selected, polygon.vertices[index].projected.z);
        }
        return selected;
    }

    float multiplier = 1.0F;
    float offset = options.depth_offset;
    bool choose_nearest = options.choose_nearest_depth;
    adjust_depth(options, multiplier, offset, choose_nearest);
    if (options.mode == RenderDepthBucketMode::forced_offset) {
        polygon.flags |= 0x80000000U;
    }

    float selected = 0.0F;
    for (std::size_t index = 0; index < polygon.vertices.size(); ++index) {
        RenderProjectedVertex& projected = polygon.vertices[index].projected;
        const long double adjusted_extended =
            (static_cast<long double>(offset) + projected.z)
            * static_cast<long double>(multiplier);
        const float adjusted = store_f32(adjusted_extended);
        projected.z = adjusted;
        if (index == 0 || (choose_nearest ? adjusted < selected
                                          : selected < adjusted)) {
            selected = adjusted;
        }
        if (projected.z < options.near_depth) {
            projected.z = options.near_depth;
        }
        const long double reciprocal =
            static_cast<long double>(options.near_depth) / projected.z;
        if (static_cast<long double>(options.reciprocal_depth_cap)
            < reciprocal) {
            projected.z = options.reciprocal_depth_cap;
        } else {
            projected.z = store_f32(reciprocal);
        }
    }
    return selected;
}

} // namespace

RenderPolygonPacket RenderPacketBuilder::build_face(
    const LevelRenderFaceSnapshot& face,
    const std::array<std::int32_t, 3>& object_position_q16,
    const camera::CameraStateRaw& camera,
    const RenderProjector& projector,
    const RenderPacketBuildOptions& options) {
    if (!projector) {
        throw RenderPacketError("render packet requires a projector");
    }
    if (face.vertex_count != 3 && face.vertex_count != 4) {
        throw RenderPacketError("render face must contain three or four vertices");
    }
    std::optional<RenderTextureDimensions> dimensions;
    if (face.has_texture) {
        if (options.texture_dimensions) {
            dimensions = options.texture_dimensions(
                face.runtime_material_index, face.material_checksum);
        }
        if (!dimensions.has_value() && face.has_texture_dimensions) {
            dimensions = RenderTextureDimensions{
                face.texture_width, face.texture_height};
        }
        if (dimensions.has_value()
            && (dimensions->width == 0 || dimensions->height == 0)) {
            throw RenderPacketError("textured render face has invalid dimensions");
        }
    }

    RenderPolygonPacket packet{
        face.entity,
        face.object_index,
        face.model_index,
        face.model_face_index,
        kRenderPolygonPacketFormat,
        face.flags,
        face.runtime_material_index,
        face.material_checksum,
        face.has_texture,
        face.vertex_count,
        0,
        {},
    };
    packet.vertices.reserve(face.vertex_count);

    for (std::size_t corner = 0; corner < face.vertex_count; ++corner) {
        const auto position =
            view_position(face, corner, object_position_q16, camera);
        const RenderViewVertexInput input{position, face.flags};
        RenderPolygonVertex vertex{
            projector(input),
            options.color,
            {static_cast<float>(face.uv[corner][0]),
             static_cast<float>(face.uv[corner][1])},
            face.uv[corner],
            false,
        };
        if (dimensions.has_value()) {
            vertex.uv = normalized_uv(
                face.uv[corner], *dimensions, options.half_texel);
            vertex.uv_normalized = true;
        }
        packet.vertices.push_back(vertex);
    }
    return packet;
}

RenderPacketBuildResult RenderPacketBuilder::build(
    const LevelRenderSnapshot& snapshot,
    const camera::CameraStateRaw& camera,
    const RenderProjector& projector,
    const RenderPacketBuildOptions& options) {
    return build(
        snapshot.entities(), snapshot.faces(), camera, projector, options);
}

RenderPacketBuildResult RenderPacketBuilder::build(
    std::span<const LevelRenderEntitySnapshot> entities,
    std::span<const LevelRenderFaceSnapshot> faces,
    const camera::CameraStateRaw& camera,
    const RenderProjector& projector,
    const RenderPacketBuildOptions& options) {
    if (!projector) {
        throw RenderPacketError("render packet requires a projector");
    }

    RenderPacketBuildResult result;
    result.polygons.reserve(faces.size());
    for (const LevelRenderFaceSnapshot& face : faces) {
        const auto entity = std::find_if(
            entities.begin(), entities.end(),
            [&](const LevelRenderEntitySnapshot& candidate) {
                return candidate.entity == face.entity;
            });
        if (entity == entities.end()) {
            throw RenderPacketError("render face references a missing entity");
        }
        RenderPolygonPacket packet = build_face(
            face, entity->object_position, camera, projector, options);
        packet.working_vertex_offset = result.working_vertices.size();
        for (std::size_t corner = 0; corner < packet.vertices.size(); ++corner) {
            const auto position = view_position(
                face, corner, entity->object_position, camera);
            result.working_vertices.push_back({
                {position, face.flags}, packet.vertices[corner].projected});
        }
        result.polygons.push_back(std::move(packet));
    }
    return result;
}

RenderPolygonClipSummary RenderPacketBuilder::summarize_clip(
    const RenderPolygonPacket& polygon) {
    validate_polygon(polygon);
    RenderPolygonClipSummary summary{
        polygon.vertices.front().projected.clip_flags, 0};
    for (const RenderPolygonVertex& vertex : polygon.vertices) {
        summary.all_flags &= vertex.projected.clip_flags;
        summary.any_flags |= vertex.projected.clip_flags;
    }
    return summary;
}

std::vector<RenderPolygonPacket> RenderPacketBuilder::split_textured_quad(
    const RenderPolygonPacket& polygon,
    bool split) {
    validate_polygon(polygon);
    if (!split || !polygon.textured || polygon.vertex_count != 4) {
        return {polygon};
    }

    RenderPolygonPacket first = polygon;
    first.vertex_count = 3;
    first.vertices = {polygon.vertices[0], polygon.vertices[1],
                     polygon.vertices[3]};

    RenderPolygonPacket second = polygon;
    second.vertex_count = 3;
    second.vertices = {polygon.vertices[3], polygon.vertices[1],
                      polygon.vertices[2]};
    return {std::move(first), std::move(second)};
}

RenderBucketDecision RenderPacketBuilder::classify_polygon(
    RenderPolygonPacket& polygon,
    const RenderPolygonClipSummary& clip,
    const RenderDepthBucketOptions& options) {
    validate_polygon(polygon);
    RenderBucketDecision decision;
    if ((clip.all_flags & 0x10U) != 0
        || ((clip.all_flags & 0x3fU) != 0
            && (clip.any_flags & 0x10U) == 0)) {
        decision.disposition = RenderBucketDisposition::rejected_clip;
        return decision;
    }
    if ((clip.any_flags & 0x10U) != 0) {
        decision.disposition = RenderBucketDisposition::requires_near_clip;
        return decision;
    }

    const long double first_determinant = winding_determinant(
        polygon, 0, 1, polygon.vertex_count - 1);
    decision.first_winding_determinant = store_f32(first_determinant);
    const bool first_is_back_facing =
        first_determinant <= options.winding_threshold;
    if (polygon.vertex_count == 4) {
        const long double second_determinant = winding_determinant(
            polygon, 2, 1, 3, true);
        decision.second_winding_determinant = store_f32(second_determinant);
        const bool second_is_back_facing =
            second_determinant <= options.winding_threshold;
        if (second_is_back_facing == first_is_back_facing) {
            const bool reject = options.reverse_winding
                ? options.winding_threshold
                    < first_determinant
                : first_is_back_facing;
            if (reject) {
                decision.disposition = RenderBucketDisposition::rejected_winding;
                return decision;
            }
        }
    } else if (options.reverse_winding
                   ? options.winding_threshold
                       < first_determinant
                   : first_is_back_facing) {
        decision.disposition = RenderBucketDisposition::rejected_winding;
        return decision;
    }

    decision.selected_depth = update_depth_and_select(polygon, options);
    decision.bucket_index = depth_bucket(decision.selected_depth);
    decision.disposition = RenderBucketDisposition::accepted;
    return decision;
}

RenderBucketBuildResult RenderPacketBuilder::bucketize(
    std::span<RenderPolygonPacket> polygons,
    const RenderDepthBucketOptions& options) {
    RenderBucketBuildResult result;
    result.bucket_heads.fill(kRenderNoPolygonIndex);
    result.next_polygon.assign(polygons.size(), kRenderNoPolygonIndex);
    result.decisions.reserve(polygons.size());

    for (std::size_t index = 0; index < polygons.size(); ++index) {
        const RenderPolygonClipSummary clip = summarize_clip(polygons[index]);
        RenderBucketDecision decision = classify_polygon(
            polygons[index], clip, options);
        if (decision.disposition == RenderBucketDisposition::accepted) {
            result.next_polygon[index] =
                result.bucket_heads[decision.bucket_index];
            result.bucket_heads[decision.bucket_index] = index;
        }
        result.decisions.push_back(decision);
    }
    return result;
}

} // namespace opentony::trg
