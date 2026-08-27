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

struct NearClipVertex {
    RenderPolygonVertex vertex{};
    float clip_depth{};
};

[[nodiscard]] std::uint8_t color_byte(
    std::uint32_t color,
    unsigned shift) {
    return static_cast<std::uint8_t>((color >> shift) & 0xffU);
}

void set_color_byte(
    std::uint32_t& color,
    unsigned shift,
    std::uint8_t value) {
    const std::uint32_t mask = 0xffU << shift;
    color = (color & ~mask) | (static_cast<std::uint32_t>(value) << shift);
}

[[nodiscard]] std::uint8_t interpolate_color_byte(
    std::uint8_t current,
    std::uint8_t next,
    long double factor) {
    // The retail helper loads both bytes as unsigned integers, calls the
    // truncating __ftol, and adds the result to the current byte in AL.
    const int delta = static_cast<int>(std::trunc(
        (static_cast<long double>(next) - current) * factor));
    return static_cast<std::uint8_t>(
        static_cast<unsigned int>(current + delta));
}

[[nodiscard]] NearClipVertex interpolate_near_edge(
    const NearClipVertex& current,
    const NearClipVertex& next,
    float near_depth,
    bool textured) {
    const long double factor =
        (static_cast<long double>(near_depth) - current.clip_depth)
        / (static_cast<long double>(next.clip_depth) - current.clip_depth);
    NearClipVertex result = current;
    result.clip_depth = near_depth;
    result.vertex.projected.x = store_f32(
        (static_cast<long double>(next.vertex.projected.x)
         - current.vertex.projected.x)
            * factor
        + current.vertex.projected.x);
    result.vertex.projected.y = store_f32(
        (static_cast<long double>(next.vertex.projected.y)
         - current.vertex.projected.y)
            * factor
        + current.vertex.projected.y);
    result.vertex.projected.reciprocal_depth = near_depth;

    for (const unsigned shift : {0U, 8U, 16U}) {
        set_color_byte(
            result.vertex.color,
            shift,
            interpolate_color_byte(
                color_byte(current.vertex.color, shift),
                color_byte(next.vertex.color, shift),
                factor));
    }
    // The byte at stream offset +0x13 is not written by 0x004d25c0. Keeping
    // the current native byte is deterministic while retaining that target
    // field as an unresolved/stale-byte seam.

    if (textured) {
        result.vertex.uv[0] = store_f32(
            (static_cast<long double>(next.vertex.uv[0])
             - current.vertex.uv[0])
                * factor
            + current.vertex.uv[0]);
        result.vertex.uv[1] = store_f32(
            (static_cast<long double>(next.vertex.uv[1])
             - current.vertex.uv[1])
                * factor
            + current.vertex.uv[1]);
    }
    return result;
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

RenderNearClipResult RenderPacketBuilder::clip_near_plane(
    RenderPolygonPacket& polygon,
    const RenderNearClipOptions& options) {
    validate_polygon(polygon);

    const bool textured = polygon.textured;
    std::vector<NearClipVertex> source;
    source.reserve(polygon.vertices.size());
    for (const RenderPolygonVertex& input : polygon.vertices) {
        NearClipVertex transformed{input, 0.0F};
        const long double reciprocal =
            static_cast<long double>(options.projection_unit_scale)
            / static_cast<long double>(input.projected.reciprocal_depth);
        transformed.vertex.projected.x = store_f32(
            (static_cast<long double>(input.projected.x)
             - options.screen_center_x)
            * reciprocal);
        transformed.vertex.projected.y = store_f32(
            (static_cast<long double>(input.projected.y)
             - options.screen_center_y)
            * reciprocal);
        transformed.clip_depth = store_f32(
            static_cast<long double>(options.depth_scale) * reciprocal);
        // The clipper uses +0xc as view depth until its final reprojection.
        transformed.vertex.projected.reciprocal_depth =
            transformed.clip_depth;
        source.push_back(transformed);
    }

    std::vector<NearClipVertex> output;
    output.reserve(6);
    for (std::size_t index = 0; index < source.size(); ++index) {
        const NearClipVertex& current = source[index];
        const NearClipVertex& next = source[(index + 1) % source.size()];
        const bool current_outside = current.clip_depth < options.near_depth;
        const bool next_outside = next.clip_depth < options.near_depth;

        // 0x004d2310 walks current -> next in source order. An inside vertex
        // is copied before an inside transition's intersection; an outside
        // to inside transition emits only the intersection.
        if (current_outside) {
            if (!next_outside) {
                output.push_back(interpolate_near_edge(
                    current, next, options.near_depth, textured));
            }
        } else {
            output.push_back(current);
            if (next_outside) {
                output.push_back(interpolate_near_edge(
                    current, next, options.near_depth, textured));
            }
        }
    }

    const std::size_t output_count = output.size();
    RenderNearClipResult result;
    result.output_vertex_count = static_cast<std::uint8_t>(output_count);

    if (output_count > 2) {
        std::uint32_t all_lateral_flags = 0;
        for (std::size_t index = 0; index < output.size(); ++index) {
            NearClipVertex& vertex = output[index];
            const float clip_depth = vertex.vertex.projected.reciprocal_depth;
            vertex.vertex.projected.z = clip_depth;

            // Retail keeps this reciprocal extended for X, stores it to +0xc,
            // then reloads that f32 for Y.
            const long double reciprocal_extended =
                static_cast<long double>(options.depth_scale)
                / static_cast<long double>(clip_depth);
            const float reciprocal = store_f32(reciprocal_extended);
            vertex.vertex.projected.reciprocal_depth = reciprocal;
            vertex.vertex.projected.x = store_f32(
                reciprocal_extended
                    * static_cast<long double>(vertex.vertex.projected.x)
                + options.screen_center_x);
            vertex.vertex.projected.y = store_f32(
                static_cast<long double>(vertex.vertex.projected.y)
                    * static_cast<long double>(reciprocal)
                + options.screen_center_y);

            std::uint32_t lateral_flags = 0;
            if (vertex.vertex.projected.x < options.viewport_edges[2]) {
                lateral_flags |= 0x01U;
            }
            if (options.viewport_edges[0] <= vertex.vertex.projected.x) {
                lateral_flags |= 0x02U;
            }
            if (vertex.vertex.projected.y < options.viewport_edges[3]) {
                lateral_flags |= 0x04U;
            }
            if (options.viewport_edges[1] <= vertex.vertex.projected.y) {
                lateral_flags |= 0x08U;
            }
            vertex.vertex.projected.clip_flags = lateral_flags;
            all_lateral_flags = index == 0
                ? lateral_flags
                : all_lateral_flags & lateral_flags;
        }
        result.all_lateral_clip_flags = all_lateral_flags;
        if (all_lateral_flags != 0) {
            polygon.vertices.clear();
            polygon.vertex_count = 0;
            result.output_vertex_count = 0;
            return result;
        }
    }

    polygon.vertices.clear();
    polygon.vertices.reserve(output.size());
    for (NearClipVertex& vertex : output) {
        polygon.vertices.push_back(std::move(vertex.vertex));
    }
    polygon.vertex_count = static_cast<std::uint8_t>(output_count);

    if (output_count < 3 || output_count > 6) {
        return result;
    }
    result.disposition = RenderNearClipDisposition::accepted;
    return result;
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
