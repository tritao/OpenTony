#include "render_packet_builder.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

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
    trg::LevelRenderSnapshot snapshot;
    // The snapshot is intentionally built from the retail asset path in the
    // integration test; this unit test covers the face contract directly.
    (void)entity;
    (void)snapshot;

    std::cout << "Render packet builder tests passed\n";
}
