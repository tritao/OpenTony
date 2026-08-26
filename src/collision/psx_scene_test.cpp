#include "psx_scene.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace opentony::collision;

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint16_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
}

void put_i16(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::int16_t value) {
    put_u16(bytes, offset, static_cast<std::uint16_t>(value));
}

void put_u32(std::vector<std::uint8_t>& bytes, std::size_t offset,
             std::uint32_t value) {
    bytes[offset] = static_cast<std::uint8_t>(value);
    bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
    bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16u);
    bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24u);
}

std::vector<std::uint8_t> synthetic_scene() {
    constexpr std::size_t model_offset = 56;
    constexpr std::size_t tag_offset = 140;
    std::vector<std::uint8_t> bytes(tag_offset + 4, 0);
    put_u16(bytes, 0x00, 4);
    put_u16(bytes, 0x02, 2);
    put_u32(bytes, 0x04, tag_offset);
    put_u32(bytes, 0x08, 1);

    // One object, 36 bytes at 0x0c.
    put_u32(bytes, 0x0c, 0);
    put_u32(bytes, 0x10, 0);
    put_u32(bytes, 0x14, 0);
    put_u32(bytes, 0x18, 0);
    put_u32(bytes, 0x1c, 0);
    put_u16(bytes, 0x20, 0);
    put_u16(bytes, 0x22, 0);
    put_i16(bytes, 0x24, 0);
    put_i16(bytes, 0x26, 0);
    put_u32(bytes, 0x28, 0);
    put_u32(bytes, 0x2c, 0);
    put_u32(bytes, 0x30, 1);
    put_u32(bytes, 0x34, model_offset);

    // Model header at 0x38, then four vertices, one normal, one face.
    put_u16(bytes, model_offset + 0x00, 0);
    put_u16(bytes, model_offset + 0x02, 4);
    put_u16(bytes, model_offset + 0x04, 1);
    put_u16(bytes, model_offset + 0x06, 1);
    put_u32(bytes, model_offset + 0x08, 0);
    const auto vertex_base = model_offset + 0x1c;
    const auto put_vertex = [&bytes, vertex_base](std::size_t index,
                                                  std::int16_t x,
                                                  std::int16_t y,
                                                  std::int16_t z) {
        const auto offset = vertex_base + index * 8;
        put_i16(bytes, offset, x);
        put_i16(bytes, offset + 2, y);
        put_i16(bytes, offset + 4, z);
    };
    // Stored winding is the one accepted by the recovered PC edge tests.
    put_vertex(0, 0, 0, 0);
    put_vertex(1, 0, 10, 0);
    put_vertex(2, 10, 0, 0);
    put_vertex(3, 10, 10, 0);
    const auto normal_offset = vertex_base + 4 * 8;
    put_i16(bytes, normal_offset, 0);
    put_i16(bytes, normal_offset + 2, 0);
    put_i16(bytes, normal_offset + 4, 4096);
    const auto face_offset = normal_offset + 8;
    put_u16(bytes, face_offset, 0x10);
    put_u16(bytes, face_offset + 2, 0x10);
    bytes[face_offset + 4] = 0;
    bytes[face_offset + 5] = 1;
    bytes[face_offset + 6] = 2;
    bytes[face_offset + 7] = 3;
    put_u16(bytes, face_offset + 0x0c, 0);
    put_u16(bytes, face_offset + 0x0e, 0x10);
    put_u32(bytes, tag_offset, 0xffffffffu);
    return bytes;
}

void check_synthetic_scene() {
    std::string error;
    const auto bytes = synthetic_scene();
    const auto scene = PsxScene::parse(bytes, &error);
    assert(scene && error.empty());
    assert(scene->objects().size() == 1);
    assert(scene->models().size() == 1);
    assert(scene->models()[0].vertices.size() == 4);
    const std::array<std::int16_t, 3> expected_normal{0, 0, 4096};
    assert(scene->models()[0].normals[0] == expected_normal);
    assert(scene->models()[0].faces[0].normal_index == 0);
    const auto query = scene->query({4096, 4096, 4096},
                                    {4096, 4096, -4096});
    const RawVec3 expected_position{4096, 4096, 0};
    assert(query.hit_body == 1);
    assert(query.hit_model_index == 0);
    assert(query.hit_position == expected_position);
    assert(query.hit_normal == expected_normal);

    const auto with_metadata = scene->query_with_metadata(
        {4096, 4096, 4096}, {4096, 4096, -4096});
    assert(with_metadata.hit());
    assert(with_metadata.object_index == 0);
    assert(with_metadata.face_index == 0);
    assert(with_metadata.base_flags == 0x10);
    assert(with_metadata.surface_flags == 0x10);
    assert(with_metadata.surface_word == 0x00100000u);

    const std::array<std::int16_t, 9> identity{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};
    const auto dynamic = PsxScene::transform_dynamic_model(
        scene->models()[0], {0, 0, 0}, identity, 4096);
    assert(dynamic.vertices.size() == 4);
    assert(dynamic.vertices[3].x == 10);
    assert(dynamic.vertices[3].y == 10);
    assert(dynamic.vertices[3].z == 0);
    assert(dynamic.vertices[3].clip_mask == 0x600);
    assert(dynamic.clip_mask == 0);
}

void check_packaged_scene(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    assert(stream);
    const std::vector<char> raw_bytes{std::istreambuf_iterator<char>(stream),
                                      std::istreambuf_iterator<char>()};
    const std::vector<std::uint8_t> bytes(raw_bytes.begin(), raw_bytes.end());
    std::string error;
    const auto scene = PsxScene::parse(bytes, &error);
    assert(scene && error.empty());
    assert(scene->objects().size() == 470);
    assert(scene->models().size() == 471);
    assert(scene->blockmaps().size() == 1);
    assert(scene->blockmaps()[0].cell_count_x == 20);
    assert(scene->blockmaps()[0].cell_count_z == 20);
    assert(scene->models()[171].vertices.size() == 14);
    assert(scene->models()[171].normals.size() == 6);
    assert(scene->models()[171].faces.size() == 6);
    const std::array<std::int16_t, 3> expected_normal{1, -3867, -1351};
    assert(scene->models()[171].normals[4] == expected_normal);
    bool found_surface_match = false;
    for (const auto& face : scene->models()[171].faces) {
        found_surface_match |= face.normal_index == 4 && face.surface_flags == 0x10;
    }
    assert(found_surface_match);
    const auto trace_query = scene->query(
        {-4100096, -8822784, 11472896},
        {-4100096, 23945216, 11472896});
    std::cout << "trace model=" << trace_query.hit_model_index
              << " body=" << trace_query.hit_body
              << " face=0x" << std::hex << trace_query.hit_face_record
              << std::dec << " parameter=" << trace_query.hit_parameter
              << " distance=" << trace_query.hit_distance
              << " contact=" << trace_query.hit_position[0] << ","
              << trace_query.hit_position[1] << ","
              << trace_query.hit_position[2] << " normal="
              << trace_query.hit_normal[0] << ","
              << trace_query.hit_normal[1] << ","
              << trace_query.hit_normal[2] << "\n";
    const RawVec3 expected_trace_contact{-4100096, -8700784, 11472896};
    const std::array<std::int16_t, 3> expected_trace_normal{1, -3867, -1351};
    assert(trace_query.hit_model_index == 171);
    assert(trace_query.hit_parameter == 61);
    assert(trace_query.hit_distance == 29);
    assert(trace_query.hit_position == expected_trace_contact);
    assert(trace_query.hit_normal == expected_trace_normal);

    const auto trace_with_metadata = scene->query_with_metadata(
        {-4100096, -8822784, 11472896},
        {-4100096, 23945216, 11472896});
    assert(trace_with_metadata.hit());
    assert(trace_with_metadata.object_index + 1 == trace_with_metadata.query.hit_body);
    assert(trace_with_metadata.face_index < scene->models()[171].faces.size());
    assert(trace_with_metadata.base_flags == 0x1003);
    assert(trace_with_metadata.surface_flags == 0x10);
    assert(trace_with_metadata.surface_word == 0x00100020u);
}

}  // namespace

int main(int argc, char** argv) {
    check_synthetic_scene();
    if (argc == 2) {
        check_packaged_scene(argv[1]);
    }
    std::cout << "native PSX collision scene checks passed\n";
}
