#include "psx_scene.hpp"

#include "tests/test_check.hpp"
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
    CHECK(scene && error.empty());
    CHECK(scene->blockmaps().size() == 1);
    CHECK(scene->blockmaps()[0].cells[0].object_indices.empty());
    // The wrapped one-unit span is outside this query, so the blockmap must
    // suppress the otherwise colliding synthetic face.
    const auto query = scene->query({4096, 4096, 4096},
                                    {4096, 4096, -4096});
    CHECK(query.hit_body == 0);
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
    CHECK(scene && error.empty());
    CHECK(scene->blockmaps().size() == 1);
    CHECK(scene->blockmaps()[0].cells.size() == 4);
    CHECK(scene->blockmaps()[0].cells[1].object_indices.size() == 1);
    const auto query = scene->query({4096, 4096, 4096},
                                    {4096, 4096, -4096});
    CHECK(query.hit_body == 1);
}

void check_synthetic_scene() {
    std::string error;
    const auto bytes = synthetic_scene();
    const auto scene = PsxScene::parse(bytes, &error);
    CHECK(scene && error.empty());
    CHECK(scene->objects().size() == 1);
    CHECK(scene->models().size() == 1);
    CHECK(scene->objects()[0].collision_angles ==
           (std::array<std::int16_t, 3>{0x1234, -16, -32767}));
    CHECK(scene->models()[0].vertices.size() == 4);
    const std::array<std::int16_t, 3> expected_normal{0, 0, 4096};
    CHECK(scene->models()[0].normals[0] == expected_normal);
    CHECK(scene->models()[0].faces[0].normal_index == 0);
    const auto query = scene->query({4096, 4096, 4096},
                                    {4096, 4096, -4096});
    const RawVec3 expected_position{4096, 4096, 0};
    CHECK(query.hit_body == 1);
    CHECK(query.hit_model_index == 0);
    CHECK(query.hit_position == expected_position);
    CHECK(query.hit_normal == expected_normal);

    const auto with_metadata = scene->query_with_metadata(
        {4096, 4096, 4096}, {4096, 4096, -4096});
    CHECK(with_metadata.hit());
    CHECK(with_metadata.object_index == 0);
    CHECK(with_metadata.face_index == 0);
    CHECK(with_metadata.base_flags == 0x10);
    CHECK(with_metadata.surface_flags == 0x10);
    CHECK(with_metadata.surface_word == 0x00100000u);
    const auto decoded_flags = with_metadata.decoded_flags();
    CHECK(decoded_flags.is_triangle);
    CHECK(decoded_flags.surface_wallrideable);
    CHECK(decoded_flags.inverse_bit_23);
    CHECK(decoded_flags.inverse_bit_24);

    // 0x00466090 is a caller-record wrapper, not a hit predicate: it
    // publishes the static result and returns zero for both outcomes.
    QueryRecord wrapper_hit;
    wrapper_hit.start = {4096, 4096, 4096};
    wrapper_hit.end = {4096, 4096, -4096};
    reference::prepare(wrapper_hit, 0x0042);
    CHECK(scene->execute_query_wrapper(wrapper_hit, 0) == 0);
    CHECK(wrapper_hit.query_stamp == 0x0042);
    CHECK(wrapper_hit.hit_body == 1);
    CHECK(wrapper_hit.hit_model_index == 0);
    CHECK(wrapper_hit.hit_position == expected_position);

    QueryRecord wrapper_miss;
    wrapper_miss.start = {500000, 500000, 500000};
    wrapper_miss.end = {500000, 500000, 499000};
    reference::prepare(wrapper_miss, 0x0043);
    CHECK(scene->execute_query_wrapper(wrapper_miss, 0) == 0);
    CHECK(wrapper_miss.query_stamp == 0x0043);
    CHECK(wrapper_miss.hit_body == 0);
    CHECK(wrapper_miss.hit_distance == reference::kUnhit);
    CHECK(wrapper_miss.hit_parameter == reference::kUnhit);

    PsxLinkedCollisionObject wrapper_linked;
    wrapper_linked.body_id = 0xfeed1234;
    wrapper_linked.flags = 0x0110;
    wrapper_linked.position = {409600, 0, 409600};
    wrapper_linked.model_index = 0;
    const std::array<PsxLinkedCollisionObject, 1> wrapper_linked_span{
        wrapper_linked};

    QueryRecord mode_zero;
    mode_zero.start = {409600, 4096, 409600};
    mode_zero.end = {409600, -4096, 409600};
    reference::prepare(mode_zero, 0x0044);
    CHECK(scene->execute_query_wrapper(
               mode_zero, 0, wrapper_linked_span) == 0);
    CHECK(mode_zero.hit_body == 0);

    QueryRecord mode_one = mode_zero;
    reference::prepare(mode_one, 0x0045);
    CHECK(scene->execute_query_wrapper(
               mode_one, 1, wrapper_linked_span) == 0);
    CHECK(mode_one.query_stamp == 0x0045);
    CHECK(mode_one.hit_body == 0xfeed1234);

    const auto linked_source = scene->linked_collision_object_from_source(
        0, 6, 0x12345678, 0x0110);
    CHECK(linked_source.has_value());
    CHECK(linked_source->body_id == 0x12345678);
    CHECK(linked_source->flags == 0x0110);
    CHECK(linked_source->angles ==
           (std::array<std::int16_t, 3>{0x1234, -16, -32767}));
    CHECK(linked_source->model_index == 0);
    CHECK(linked_source->model_kind == 6);
    CHECK(!scene->linked_collision_object_from_source(
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
    CHECK(dynamic_result.hit());
    CHECK(dynamic_result.query.hit_body == 1);
    CHECK(dynamic_result.query.hit_distance == 10);
    CHECK(dynamic_result.query.hit_position.at(2) == 40900);
    CHECK(dynamic_result.query.hit_normal == expected_normal);
    CHECK(dynamic_result.object_index == 0);
    CHECK(dynamic_result.face_index == 0);

    // The dynamic walker has a non-physical query-mask branch for surface
    // bit 0x20000. It records the model selector sideband when q+0x88 is
    // enabled, but must not publish a collision hit.
    auto query_mask_bytes = bytes;
    put_u16(query_mask_bytes, 0x8a, 0x0002);  // face surface flags
    std::string query_mask_error;
    const auto query_mask_scene = PsxScene::parse(
        query_mask_bytes, &query_mask_error);
    CHECK(query_mask_scene && query_mask_error.empty());
    CollisionFaceFilter query_mask_filter;
    query_mask_filter.query_mask_mode = true;
    const auto query_mask_result = query_mask_scene->query_dynamic_object(
        {0, 0, 0}, {0, 0, 409600}, 0, dynamic_vertices, reverse_z,
        identity, 0, query_mask_filter);
    CHECK(!query_mask_result.hit());
    CHECK(query_mask_result.query.query_mask_mode == 1);
    CHECK(query_mask_result.query_mask_model_index == 0);

    PsxLinkedCollisionObject linked_object;
    linked_object.body_id = 0xfeed1234;
    linked_object.flags = 0x0110;
    linked_object.position = {0, 0, 0};
    linked_object.model_index = 0;
    const std::array<PsxLinkedCollisionObject, 1> linked_objects{
        linked_object};
    const auto linked_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, linked_objects, 7);
    CHECK(linked_result.hit());
    CHECK(linked_result.query.hit_body == 0xfeed1234);
    CHECK(linked_result.query.hit_distance == 1);
    CHECK(linked_result.query.hit_position.at(2) == 0);
    CHECK(linked_result.query.hit_normal == expected_normal);
    CHECK(linked_result.object_index == 0);
    CHECK(linked_result.face_index == 0);

    const auto aggregate_result = scene->query_with_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, linked_objects, 7);
    CHECK(aggregate_result.hit());
    CHECK(aggregate_result.query.hit_body == 1);
    CHECK(aggregate_result.query.hit_distance == 1);
    const auto static_aggregate_result = scene->query_with_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, {}, 7);
    CHECK(static_aggregate_result.hit());
    CHECK(static_aggregate_result.query.hit_body == 1);

    auto farther_object = linked_object;
    farther_object.body_id = 0xabcd;
    farther_object.position = {0, 0, -4096};
    const std::array<PsxLinkedCollisionObject, 2> ordered_objects{
        farther_object, linked_object};
    const auto nearest_linked_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, ordered_objects, 8);
    CHECK(nearest_linked_result.hit());
    CHECK(nearest_linked_result.query.hit_body == 0xfeed1234);
    CHECK(nearest_linked_result.query.hit_distance == 1);

    linked_object.flags = 0x0130;
    const std::array<PsxLinkedCollisionObject, 1> rejected_objects{
        linked_object};
    const auto rejected_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, rejected_objects, 8);
    CHECK(!rejected_result.hit());

    linked_object.flags = 0x0110;
    linked_object.body_id = 0xbeef;
    linked_object.position = {409600, 409600, 409600};
    const std::array<PsxLinkedCollisionObject, 1> distant_objects{
        linked_object};
    const auto distant_result = scene->query_linked_objects(
        {4096, 4096, 4096}, {4096, 4096, -4096}, distant_objects, 9);
    CHECK(!distant_result.hit());

    const auto dynamic = PsxScene::transform_dynamic_model(
        scene->models()[0], {0, 0, 0}, identity, 4096);
    CHECK(dynamic.vertices.size() == 4);
    CHECK(dynamic.vertices[3].x == 10);
    CHECK(dynamic.vertices[3].y == 10);
    CHECK(dynamic.vertices[3].z == 0);
    CHECK(dynamic.vertices[3].clip_mask == 0x600);
    CHECK(dynamic.clip_mask == 0);
}

void check_packaged_scene(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    CHECK(stream);
    const std::vector<char> raw_bytes{std::istreambuf_iterator<char>(stream),
                                      std::istreambuf_iterator<char>()};
    const std::vector<std::uint8_t> bytes(raw_bytes.begin(), raw_bytes.end());
    std::string error;
    const auto scene = PsxScene::parse(bytes, &error);
    CHECK(scene && error.empty());
    CHECK(scene->objects().size() == 470);
    CHECK(scene->models().size() == 471);
    CHECK(scene->blockmaps().size() == 1);
    CHECK(scene->blockmaps()[0].cell_count_x == 20);
    CHECK(scene->blockmaps()[0].cell_count_z == 20);
    CHECK(scene->models()[171].vertices.size() == 14);
    CHECK(scene->models()[171].normals.size() == 6);
    CHECK(scene->models()[171].faces.size() == 6);
    CHECK(scene->objects()[170].model_index == 171);
    CHECK(scene->objects()[170].position ==
           (RawVec3{-4100096, -6782976, 9408512}));
    const std::array<std::int16_t, 3> expected_normal{1, -3867, -1351};
    CHECK(scene->models()[171].normals[4] == expected_normal);
    bool found_surface_match = false;
    for (const auto& face : scene->models()[171].faces) {
        found_surface_match |= face.normal_index == 4 && face.surface_flags == 0x10;
    }
    CHECK(found_surface_match);
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
    CHECK(trace_query.hit_model_index == 171);
    CHECK(trace_query.hit_parameter == 61);
    CHECK(trace_query.hit_distance == 29);
    CHECK(trace_query.hit_position == expected_trace_contact);
    CHECK(trace_query.hit_normal == expected_trace_normal);

    const auto trace_with_metadata = scene->query_with_metadata(
        {-4100096, -8822784, 11472896},
        {-4100096, 23945216, 11472896});
    CHECK(trace_with_metadata.hit());
    CHECK(trace_with_metadata.object_index + 1 == trace_with_metadata.query.hit_body);
    CHECK(trace_with_metadata.face_index < scene->models()[171].faces.size());
    CHECK(trace_with_metadata.base_flags == 0x1003);
    CHECK(trace_with_metadata.surface_flags == 0x10);
    CHECK(trace_with_metadata.surface_word == 0x00100020u);

    // Controlled replay of the live linked-object face path captured by
    // collision-dynamic-positive5. The PC node used flags 0x0110, zero
    // angles, and the model-171 object origin below; unlike the static path,
    // its contact is reconstructed from q+0x40 rather than q+0x8c.
    const auto linked_model_171 = scene->linked_collision_object_from_source(
        170, 6, 0x05f26c84, 0x0110);
    CHECK(linked_model_171.has_value());
    CHECK(linked_model_171->source_object_index == 170);
    CHECK(linked_model_171->position ==
           (RawVec3{-4100096, -6782976, 9408512}));
    CHECK(linked_model_171->angles == (std::array<std::int16_t, 3>{0, 0, 0}));
    CHECK(linked_model_171->model_index == 171);
    CHECK(linked_model_171->model_kind == 6);
    const auto dynamic_replay = scene->query_linked_objects(
        {-4100096, -8822784, 11472896},
        {-4100096, 23945216, 11472896},
        std::span<const PsxLinkedCollisionObject>(&*linked_model_171, 1), 1);
    CHECK(dynamic_replay.hit());
    CHECK(dynamic_replay.query.hit_body == 0x05f26c84);
    CHECK(dynamic_replay.source_object_index == 170);
    CHECK(dynamic_replay.query.hit_model_index == 171);
    CHECK(dynamic_replay.query.hit_distance == 29);
    CHECK(dynamic_replay.query.hit_position ==
           (RawVec3{-4100096, -8710784, 11472896}));
    const std::array<std::int16_t, 3> expected_dynamic_normal{1, -4093, -160};
    CHECK(dynamic_replay.query.hit_normal == expected_dynamic_normal);
    CHECK(dynamic_replay.query.hit_parameter == 0x7fffffff);
    CHECK(dynamic_replay.surface_word == 0x00100028u);
    CHECK(dynamic_replay.face_index == 5);
    CHECK(dynamic_replay.query.hit_face_record != 0);
}

void check_parseable_scene(const char* path) {
    std::ifstream stream(path, std::ios::binary);
    CHECK(stream);
    const std::vector<char> raw_bytes{std::istreambuf_iterator<char>(stream),
                                      std::istreambuf_iterator<char>()};
    const std::vector<std::uint8_t> bytes(raw_bytes.begin(), raw_bytes.end());
    std::string error;
    const auto scene = PsxScene::parse(bytes, &error);
    CHECK(scene && error.empty());
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
