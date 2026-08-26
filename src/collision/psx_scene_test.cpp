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
    put_u32(bytes, 0x1c, 0xfff01234u);
    put_u16(bytes, 0x20, 0x8001);
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

void check_wrapped_blockmap_bounds() {
    auto bytes = synthetic_scene();
    constexpr std::size_t tag_offset = 140;
    constexpr std::size_t blockmap_payload_offset = tag_offset + 8;
    constexpr std::size_t blockmap_payload_size = 36;
    bytes.resize(tag_offset + 8 + blockmap_payload_size + 4, 0);
    put_u32(bytes, tag_offset, 0x0000000a);
    put_u32(bytes, tag_offset + 4, blockmap_payload_size);
    put_u32(bytes, blockmap_payload_offset + 0x00, 0x7fffffffu);
    put_u32(bytes, blockmap_payload_offset + 0x04, 0);
    put_u32(bytes, blockmap_payload_offset + 0x08, 0x80000000u);
    put_u32(bytes, blockmap_payload_offset + 0x0c, 0);
    put_u16(bytes, blockmap_payload_offset + 0x10, 1);
    put_u16(bytes, blockmap_payload_offset + 0x12, 1);
    // Zero-valued cell metadata, an empty reference list, and its zero
    // terminator are all valid payload values, not truncated reads.
    put_u32(bytes, blockmap_payload_offset + 0x14, 0);
    put_u32(bytes, blockmap_payload_offset + 0x18, 0);
    put_u32(bytes, blockmap_payload_offset + 0x1c, 0);
    put_u32(bytes, blockmap_payload_offset + 0x20, 0);
    put_u32(bytes, tag_offset + 8 + blockmap_payload_size, 0xffffffffu);

    std::string error;
    const auto scene = PsxScene::parse(bytes, &error);
    assert(scene && error.empty());
    assert(scene->blockmaps().size() == 1);
    assert(scene->blockmaps()[0].cells[0].object_indices.empty());
    // The wrapped one-unit span is outside this query, so the blockmap must
    // suppress the otherwise colliding synthetic face.
    const auto query = scene->query({4096, 4096, 4096},
                                    {4096, 4096, -4096});
    assert(query.hit_body == 0);
}

void check_blockmap_line_walk() {
    auto bytes = synthetic_scene();
    constexpr std::size_t tag_offset = 140;
    constexpr std::size_t payload_offset = tag_offset + 8;
    bytes.resize(payload_offset + 20 + 4 * 20 + 4, 0);
    put_u32(bytes, tag_offset, 0x0000000a);
    put_u32(bytes, payload_offset + 0x00, static_cast<std::uint32_t>(-8192));
    put_u32(bytes, payload_offset + 0x04, static_cast<std::uint32_t>(-8192));
    put_u32(bytes, payload_offset + 0x08, 8192);
    put_u32(bytes, payload_offset + 0x0c, 8192);
    put_u16(bytes, payload_offset + 0x10, 2);
    put_u16(bytes, payload_offset + 0x12, 2);

    std::size_t cell_offset = payload_offset + 20;
    for (std::size_t cell = 0; cell < 4; ++cell) {
        put_u32(bytes, cell_offset, 0);
        put_u32(bytes, cell_offset + 4, 0);
        const auto has_object = cell == 1;
        put_u32(bytes, cell_offset + 8, has_object ? 1 : 0);
        cell_offset += 12;
        if (has_object) {
            put_u32(bytes, cell_offset, 0);
            cell_offset += 4;
        }
        put_u32(bytes, cell_offset, 0);
        cell_offset += 4;
    }
    const auto payload_size = cell_offset - payload_offset;
    put_u32(bytes, tag_offset + 4, static_cast<std::uint32_t>(payload_size));
    bytes.resize(cell_offset + 4, 0);
    put_u32(bytes, cell_offset, 0xffffffffu);

    std::string error;
    const auto scene = PsxScene::parse(bytes, &error);
    assert(scene && error.empty());
    assert(scene->blockmaps().size() == 1);
    assert(scene->blockmaps()[0].cells.size() == 4);
    assert(scene->blockmaps()[0].cells[1].object_indices.size() == 1);
    const auto query = scene->query({4096, 4096, 4096},
                                    {4096, 4096, -4096});
    assert(query.hit_body == 1);
}

void check_synthetic_scene() {
    std::string error;
    const auto bytes = synthetic_scene();
    const auto scene = PsxScene::parse(bytes, &error);
    assert(scene && error.empty());
    assert(scene->objects().size() == 1);
    assert(scene->models().size() == 1);
    assert(scene->objects()[0].collision_angles ==
           (std::array<std::int16_t, 3>{0x1234, -16, -32767}));
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
    const auto decoded_flags = with_metadata.decoded_flags();
    assert(decoded_flags.is_triangle);
    assert(decoded_flags.surface_wallrideable);
    assert(decoded_flags.inverse_bit_23);
    assert(decoded_flags.inverse_bit_24);

    const auto linked_source = scene->linked_collision_object_from_source(
        0, 6, 0x12345678, 0x0110);
    assert(linked_source.has_value());
    assert(linked_source->body_id == 0x12345678);
    assert(linked_source->flags == 0x0110);
    assert(linked_source->angles ==
           (std::array<std::int16_t, 3>{0x1234, -16, -32767}));
    assert(linked_source->model_index == 0);
    assert(linked_source->model_kind == 6);
    assert(!scene->linked_collision_object_from_source(
        scene->objects().size(), 6, 1, 0));

    const std::array<std::int16_t, 9> identity{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};

    PsxDynamicModelVertices dynamic_vertices;
    dynamic_vertices.clip_mask = 0;
    dynamic_vertices.vertices = {
        reference::DynamicVertexRecord{0, 0, 10, 0},
        reference::DynamicVertexRecord{10, 0, 10, 0},
        reference::DynamicVertexRecord{0, 10, 10, 0},
        reference::DynamicVertexRecord{10, 10, 10, 0},
    };
    const std::array<std::int16_t, 9> reverse_z{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, -0x1000};
    const auto dynamic_result = scene->query_dynamic_object(
        {0, 0, 0}, {0, 0, 409600}, 0, dynamic_vertices, reverse_z,
        identity, 0);
    assert(dynamic_result.hit());
    assert(dynamic_result.query.hit_body == 1);
    assert(dynamic_result.query.hit_distance == 10);
    assert(dynamic_result.query.hit_position.at(2) == 40900);
    assert(dynamic_result.query.hit_normal == expected_normal);
    assert(dynamic_result.object_index == 0);
    assert(dynamic_result.face_index == 0);

    // The dynamic walker has a non-physical query-mask branch for surface
    // bit 0x20000. It records the model selector sideband when q+0x88 is
    // enabled, but must not publish a collision hit.
    auto query_mask_bytes = bytes;
    put_u16(query_mask_bytes, 0x8a, 0x0002);  // face surface flags
    std::string query_mask_error;
    const auto query_mask_scene = PsxScene::parse(
        query_mask_bytes, &query_mask_error);
    assert(query_mask_scene && query_mask_error.empty());
    CollisionFaceFilter query_mask_filter;
    query_mask_filter.query_mask_mode = true;
    const auto query_mask_result = query_mask_scene->query_dynamic_object(
        {0, 0, 0}, {0, 0, 409600}, 0, dynamic_vertices, reverse_z,
        identity, 0, query_mask_filter);
    assert(!query_mask_result.hit());
    assert(query_mask_result.query.query_mask_mode == 1);
    assert(query_mask_result.query_mask_model_index == 0);

    PsxLinkedCollisionObject linked_object;
    linked_object.body_id = 0xfeed1234;
    linked_object.flags = 0x0110;
    linked_object.position = {0, 0, 0};
    linked_object.model_index = 0;
    const std::array<PsxLinkedCollisionObject, 1> linked_objects{
        linked_object};
    const auto linked_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, linked_objects, 7);
    assert(linked_result.hit());
    assert(linked_result.query.hit_body == 0xfeed1234);
    assert(linked_result.query.hit_distance == 1);
    assert(linked_result.query.hit_position.at(2) == 0);
    assert(linked_result.query.hit_normal == expected_normal);
    assert(linked_result.object_index == 0);
    assert(linked_result.face_index == 0);

    const auto aggregate_result = scene->query_with_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, linked_objects, 7);
    assert(aggregate_result.hit());
    assert(aggregate_result.query.hit_body == 1);
    assert(aggregate_result.query.hit_distance == 1);
    const auto static_aggregate_result = scene->query_with_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, {}, 7);
    assert(static_aggregate_result.hit());
    assert(static_aggregate_result.query.hit_body == 1);

    auto farther_object = linked_object;
    farther_object.body_id = 0xabcd;
    farther_object.position = {0, 0, -4096};
    const std::array<PsxLinkedCollisionObject, 2> ordered_objects{
        farther_object, linked_object};
    const auto nearest_linked_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, ordered_objects, 8);
    assert(nearest_linked_result.hit());
    assert(nearest_linked_result.query.hit_body == 0xfeed1234);
    assert(nearest_linked_result.query.hit_distance == 1);

    linked_object.flags = 0x0130;
    const std::array<PsxLinkedCollisionObject, 1> rejected_objects{
        linked_object};
    const auto rejected_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, rejected_objects, 8);
    assert(!rejected_result.hit());

    linked_object.flags = 0x0110;
    linked_object.body_id = 0xbeef;
    linked_object.position = {409600, 409600, 409600};
    const std::array<PsxLinkedCollisionObject, 1> distant_objects{
        linked_object};
    const auto distant_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, distant_objects, 9);
    assert(!distant_result.hit());

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
    assert(scene->objects()[170].model_index == 171);
    assert(scene->objects()[170].position ==
           (RawVec3{-4100096, -6782976, 9408512}));
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

    // Controlled replay of the live linked-object face path captured by
    // collision-dynamic-positive5. The PC node used flags 0x0110, zero
    // angles, and the model-171 object origin below; unlike the static path,
    // its contact is reconstructed from q+0x40 rather than q+0x8c.
    const auto linked_model_171 = scene->linked_collision_object_from_source(
        170, 6, 0x05f26c84, 0x0110);
    assert(linked_model_171.has_value());
    assert(linked_model_171->source_object_index == 170);
    assert(linked_model_171->position ==
           (RawVec3{-4100096, -6782976, 9408512}));
    assert(linked_model_171->angles == (std::array<std::int16_t, 3>{0, 0, 0}));
    assert(linked_model_171->model_index == 171);
    assert(linked_model_171->model_kind == 6);
    const auto dynamic_replay = scene->query_linked_objects(
        {-4100096, -8822784, 11472896},
        {-4100096, 23945216, 11472896},
        std::span<const PsxLinkedCollisionObject>(&*linked_model_171, 1), 1);
    assert(dynamic_replay.hit());
    assert(dynamic_replay.query.hit_body == 0x05f26c84);
    assert(dynamic_replay.source_object_index == 170);
    assert(dynamic_replay.query.hit_model_index == 171);
    assert(dynamic_replay.query.hit_distance == 29);
    assert(dynamic_replay.query.hit_position ==
           (RawVec3{-4100096, -8710784, 11472896}));
    const std::array<std::int16_t, 3> expected_dynamic_normal{1, -4093, -160};
    assert(dynamic_replay.query.hit_normal == expected_dynamic_normal);
    assert(dynamic_replay.query.hit_parameter == 0x7fffffff);
    assert(dynamic_replay.surface_word == 0x00100028u);
    assert(dynamic_replay.face_index == 5);
    assert(dynamic_replay.query.hit_face_record != 0);
}

void check_parseable_scene(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    assert(stream);
    const std::vector<char> raw_bytes{std::istreambuf_iterator<char>(stream),
                                      std::istreambuf_iterator<char>()};
    const std::vector<std::uint8_t> bytes(raw_bytes.begin(), raw_bytes.end());
    std::string error;
    const auto scene = PsxScene::parse(bytes, &error);
    assert(scene && error.empty());
}

}  // namespace

int main(int argc, char** argv) {
    check_synthetic_scene();
    check_wrapped_blockmap_bounds();
    check_blockmap_line_walk();
    if (argc == 2) {
        check_packaged_scene(argv[1]);
    } else if (argc > 2) {
        check_packaged_scene(argv[1]);
        for (int index = 2; index < argc; ++index) {
            check_parseable_scene(argv[index]);
        }
    }
    std::cout << "native PSX collision scene checks passed\n";
}
