#include "render_packet_builder.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    using namespace opentony;
    trg::LevelRenderFaceSnapshot face{};
    face.entity = 4;
    face.object_index = 17;
    face.model_index = 17;
    face.model_face_index = 2;
    face.flags = 0x10;
    face.runtime_material_index = 3;
    face.material_checksum = 0x12345678;
    face.has_texture = true;
    face.vertex_count = 3;
    face.local_vertices[0] = {1, 2, 3};
    face.local_vertices[1] = {4, 5, 6};
    face.local_vertices[2] = {-1, -2, -3};
    face.uv[0] = {0, 0};
    face.uv[1] = {8, 16};
    face.uv[2] = {32, 24};

    camera::CameraStateRaw camera{};
    camera.current_transform = {0, 0, 0, camera::kQ12One};
    const auto projector = [](const trg::RenderViewVertexInput& input) {
        return trg::RenderProjectedVertex{
            static_cast<float>(input.position_q16[0]),
            static_cast<float>(input.position_q16[1]),
            static_cast<float>(input.position_q16[2]),
            1.0F,
            input.source_flags,
            0.0F,
        };
    };
    trg::RenderPacketBuildOptions options;
    options.texture_dimensions = [](std::size_t index, std::uint32_t checksum)
        -> std::optional<trg::RenderTextureDimensions> {
        assert(index == 3);
        assert(checksum == 0x12345678);
        return trg::RenderTextureDimensions{64, 32};
    };
    const trg::RenderPolygonPacket packet = trg::RenderPacketBuilder::build_face(
        face, {0x1000, 0x2000, 0x3000}, camera, projector, options);

    assert(packet.format == trg::kRenderPolygonPacketFormat);
    assert(packet.object_index == 17);
    assert(packet.vertex_count == 3);
    assert(packet.vertices.size() == 3);
    assert(packet.vertices[0].projected.x == 0x2000);
    assert(packet.vertices[0].projected.y == 0x4000);
    assert(packet.vertices[0].projected.z == 0x6000);
    assert(packet.vertices[0].uv_normalized);
    const std::array<std::uint16_t, 2> expected_source_uv{0, 0};
    assert(packet.vertices[0].source_uv == expected_source_uv);
    assert(std::fabs(packet.vertices[0].uv[0] - (0.5F / 64.0F)) < 0.00001F);
    assert(std::fabs(packet.vertices[1].uv[1] - (16.5F / 32.0F)) < 0.00001F);

    trg::LevelRenderEntitySnapshot entity{};
    entity.entity = face.entity;
    entity.object_position = {0x1000, 0x2000, 0x3000};
    trg::LevelRenderFaceSnapshot second = face;
    second.model_face_index = 9;
    second.flags = 0;
    second.has_texture = false;
    second.runtime_material_index = trg::CommandPointRuntime::npos;
    second.material_checksum = 0;
    second.vertex_count = 3;
    second.local_vertices[0] = {7, 8, 9};
    second.local_vertices[1] = {10, 11, 12};
    second.local_vertices[2] = {13, 14, 15};
    const std::vector<trg::LevelRenderEntitySnapshot> entities{entity};
    const std::vector<trg::LevelRenderFaceSnapshot> faces{face, second};
    const auto result = trg::RenderPacketBuilder::build(
        entities, faces, camera, projector, options);
    assert(result.polygons.size() == 2);
    assert(result.working_vertices.size() == 6);
    assert(result.polygons[0].face_index == 2);
    assert(result.polygons[1].face_index == 9);
    assert(result.polygons[0].working_vertex_offset == 0);
    assert(result.polygons[1].working_vertex_offset == 3);
    assert(result.polygons[0].vertex_count == 3);
    assert(result.polygons[1].vertex_count == 3);
    const std::array<std::int32_t, 3> first_position{0x2000, 0x4000, 0x6000};
    const std::array<std::int32_t, 3> second_position{0x8000, 0xa000, 0xc000};
    assert(result.working_vertices[0].input.position_q16 == first_position);
    assert(result.working_vertices[3].input.position_q16 == second_position);

    std::cout << "Render packet builder tests passed\n";
}
