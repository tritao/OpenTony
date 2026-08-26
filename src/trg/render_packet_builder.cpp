#include "render_packet_builder.hpp"

#include <algorithm>
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
    if (!projector) {
        throw RenderPacketError("render packet requires a projector");
    }

    RenderPacketBuildResult result;
    result.polygons.reserve(snapshot.faces().size());
    for (const LevelRenderFaceSnapshot& face : snapshot.faces()) {
        const auto entity = std::find_if(
            snapshot.entities().begin(), snapshot.entities().end(),
            [&](const LevelRenderEntitySnapshot& candidate) {
                return candidate.entity == face.entity;
            });
        if (entity == snapshot.entities().end()) {
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

} // namespace opentony::trg
