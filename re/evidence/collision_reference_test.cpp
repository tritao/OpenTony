#include "collision_reference.hpp"

#include "tests/test_check.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace opentony::collision_reference;

int main() {
    CHECK(clamp_to_s16(-40000) == -32768);
    CHECK(clamp_to_s16(-32768) == -32768);
    CHECK(clamp_to_s16(32767) == 32767);
    CHECK(clamp_to_s16(40000) == 32767);
    CHECK(narrow_s16(-32769) == 32767);
    CHECK(narrow_s16(32768) == -32768);

    std::array<std::uint8_t, sizeof(LinkedCollisionObjectLayout)> linked_bytes{};
    const auto put_linked16 = [&linked_bytes](std::size_t offset,
                                               std::uint16_t value) {
        linked_bytes[offset] = static_cast<std::uint8_t>(value);
        linked_bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
    };
    const auto put_linked32 = [&linked_bytes](std::size_t offset,
                                               std::uint32_t value) {
        linked_bytes[offset] = static_cast<std::uint8_t>(value);
        linked_bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
        linked_bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16u);
        linked_bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24u);
    };
    put_linked16(0x04, 0x0025);
    put_linked16(0x06, 0x1234);
    put_linked32(0x08, 4096);
    put_linked32(0x0c, static_cast<std::uint32_t>(-8192));
    put_linked32(0x10, 12288);
    put_linked16(0x14, 0x0100);
    put_linked16(0x16, 0xff00);
    put_linked16(0x18, 0x0200);
    put_linked16(0x1a, 171);
    linked_bytes[0x1f] = 6;
    put_linked32(0x20, 0x12345678);
    const auto linked = read_linked_collision_object(linked_bytes);
    CHECK(linked);
    CHECK(linked->flags == 0x0025);
    CHECK(linked->query_stamp == 0x1234);
    CHECK((linked->position == RawVec3{4096, -8192, 12288}));
    CHECK((linked->angles == std::array<std::int16_t, 3>{0x0100, -0x0100,
                                                           0x0200}));
    CHECK(linked->model_index == 171);
    CHECK(linked->model_kind == 6);
    CHECK(linked->next == 0x12345678);
    CHECK(linked_object_flag_gate(0x0110));
    CHECK(linked_object_flag_gate(0x8110));
    CHECK(linked_object_flag_gate(0x0410));
    CHECK(linked_object_uses_matrix_transform(0x0200));
    CHECK(linked_object_uses_matrix_transform(0x0600));
    CHECK(!linked_object_uses_matrix_transform(0x0400));
    CHECK(!linked_object_flag_gate(0x0130));
    CHECK(!linked_object_flag_gate(0x0430));
    CHECK(!linked_object_flag_gate(0x8171));

    CHECK(candidate_cell_source_bytes(0) == 0x10);
    CHECK(candidate_cell_source_bytes(2) == 0x18);
    std::array<std::uint8_t, 0x18> candidate_source{};
    const auto put_source32 = [&candidate_source](std::size_t offset,
                                                   std::uint32_t value) {
        candidate_source[offset] = static_cast<std::uint8_t>(value);
        candidate_source[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
        candidate_source[offset + 2] = static_cast<std::uint8_t>(value >> 16u);
        candidate_source[offset + 3] = static_cast<std::uint8_t>(value >> 24u);
    };
    put_source32(0x08, 2);
    put_source32(0x0c, 0x1010);
    put_source32(0x10, 0x2020);
    std::vector<std::uint32_t> source_values;
    const auto source_read = visit_candidate_cell_source(
        candidate_source,
        [&source_values](std::size_t, std::uint32_t value) {
            source_values.push_back(value);
        });
    CHECK(source_read && source_read->terminated && source_read->count == 2);
    CHECK((source_values == std::vector<std::uint32_t>{0x1010, 0x2020}));

    std::array<std::uint8_t, sizeof(LinkedCollisionObjectListLinksLayout)>
        linked_links_bytes{};
    const auto put_linked_links32 =
        [&linked_links_bytes](std::size_t offset, std::uint32_t value) {
            linked_links_bytes[offset] = static_cast<std::uint8_t>(value);
            linked_links_bytes[offset + 1] =
                static_cast<std::uint8_t>(value >> 8u);
            linked_links_bytes[offset + 2] =
                static_cast<std::uint8_t>(value >> 16u);
            linked_links_bytes[offset + 3] =
                static_cast<std::uint8_t>(value >> 24u);
        };
    put_linked_links32(0x20, 0x11111111);
    put_linked_links32(0x34, 0x22222222);
    const auto linked_links = read_linked_collision_list_links(linked_links_bytes);
    CHECK(linked_links);
    CHECK(linked_links->next == 0x11111111);
    CHECK(linked_links->previous == 0x22222222);
    CHECK(sizeof(LinkedCollisionObjectElementLayout) == 0x4c);
    CHECK(offsetof(LinkedCollisionObjectElementLayout, previous) == 0x34);
    CHECK(offsetof(LinkedCollisionObjectElementLayout, unknown_4a) == 0x4a);
    CHECK(linked_collision_object_array_bytes(3) == 0xe8);

    std::array<std::uint8_t, 12> candidate_heads{};
    const auto put_candidate32 =
        [&candidate_heads](std::size_t offset, std::uint32_t value) {
            candidate_heads[offset] = static_cast<std::uint8_t>(value);
            candidate_heads[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
            candidate_heads[offset + 2] =
                static_cast<std::uint8_t>(value >> 16u);
            candidate_heads[offset + 3] =
                static_cast<std::uint8_t>(value >> 24u);
        };
    put_candidate32(0x00, 0xaaaa0001);
    put_candidate32(0x04, 0xbbbb0002);
    // Entry +0x08 is the null array terminator.
    std::vector<std::uint32_t> captured_heads;
    const auto candidate_read = visit_candidate_head_array(
        candidate_heads,
        [&captured_heads](std::size_t, std::uint32_t head) {
            captured_heads.push_back(head);
        });
    CHECK(candidate_read.terminated);
    CHECK(candidate_read.count == 2);
    CHECK((captured_heads == std::vector<std::uint32_t>{0xaaaa0001,
                                                          0xbbbb0002}));
    CHECK(!visit_candidate_head_array(
                 std::span<const std::uint8_t>(candidate_heads).first(8),
                 [](std::size_t, std::uint32_t) {})
                .terminated);

    CHECK(face_record_stride_bytes(0x001c1083) == 0x1c);
    CHECK(face_record_stride_bytes(0x00205823) == 0x20);

    CollisionModelHeader model;
    model.vertex_count = 14;
    model.normal_count = 6;
    model.face_count = 6;
    CHECK(model_normal_offset(model) == 0x8c);
    CHECK(model_face_offset(model) == 0xbc);
    CHECK(model_record_offset(13) == 13u * 8u);

    model.x_min = -10;
    model.x_max = 20;
    model.y_min = -5;
    model.y_max = 15;
    model.z_min = -30;
    model.z_max = 40;
    const auto object_bounds = build_object_bounds(
        model, {0, 0, 0}, {100, 100, 100}, {5, 6, 7});
    CHECK((object_bounds.min == RawVec3{-7, -1, -25}));
    CHECK((object_bounds.max == RawVec3{27, 23, 49}));
    const auto reflected_bounds = build_object_bounds(
        model, {0, 0, 0}, {100, 100, 100}, {0, 0, 0}, 0x01);
    CHECK(reflected_bounds.min[0] == 78);
    CHECK(reflected_bounds.max[0] == 112);
    const auto scaled_bounds = build_object_bounds(
        model, {0, 0, 0}, {100, 100, 100}, {5, 6, 7}, 0,
        std::array<std::int16_t, 3>{0x0800, 0x1000, 0x1000});
    CHECK(scaled_bounds.min[0] == -3);  // x87 conversion truncates -3.5
    CHECK(scaled_bounds.max[0] == 13);

    const CollisionBounds broadphase_bounds{
        .min = {4, -1, -1}, .max = {6, 1, 1}};
    CHECK(object_broadphase_test({0, 0, 0}, {10, 0, 0},
                                  broadphase_bounds));
    CHECK(!object_broadphase_test({0, 3, 0}, {10, 3, 0},
                                   broadphase_bounds));

    CHECK(sizeof(CollisionQueryLayout) == 0x90);
    CHECK(offsetof(CollisionQueryLayout, line_basis) == 0x48);
    CHECK(offsetof(CollisionQueryLayout, hit_parameter) == 0x8c);
    CHECK(kCollisionModelCacheCapacity == 20);
    CHECK(kCollisionFaceCacheCapacity == 500);
    CHECK(kCollisionFaceCacheRecordStride == 0x1c);
    CHECK(kCollisionFaceCacheBytes == 0x36b0);
    CHECK(collision_face_cache_span_fits(0, 500));
    CHECK(collision_face_cache_span_fits(499, 1));
    CHECK(!collision_face_cache_span_fits(500, 1));
    CHECK(!collision_face_cache_span_fits(499, 2));

    CollisionFacePrefix face;
    face.base_flags = 0x1083;
    face.length_bytes = 0x1c;
    face.normal_index_shifted = 0x20;
    face.surface_flags = 0x10;
    CHECK(face_record_stride_bytes(0x001c1083) == 0x1c);
    CHECK(face_normal_index(face.normal_index_shifted) == 4);

    CollisionModelCacheEntry cache_entry;
    cache_entry.face_aabb_start = 17;
    CHECK(cache_entry.face_aabb_start == 17);
    CollisionFaceAabbRecord aabb;
    aabb.min_x = -10;
    aabb.max_z = 20;
    CHECK(aabb.min_x == -10 && aabb.max_z == 20);

    QueryRecord query;
    query.start = {-4098781, -647119, -16628281};
    query.end = {-4098749, -420744, -16813694};
    prepare(query, 0x1234);

    // This is the airborne runtime hit recorded in collision-air4.trace.ndjson.
    CHECK(query.line_length == 71);
    CHECK(query.query_stamp == 0x1234);
    CHECK(record_nearest_plane_candidate(query, 2101, -14283, 0x05f2dcfc,
                                          0x05db3534, 132));
    CHECK(query.hit_parameter == 2101);
    CHECK(query.hit_distance == 9);
    const RawVec3 expected_contact{-4098777, -618090, -16652057};
    CHECK(query.hit_position == expected_contact);
    CHECK(query.hit_body == 0x05f2dcfc);
    CHECK(query.hit_face_record == 0x05db3534);
    CHECK(query.hit_model_index == 132);

    // These line-basis records are the compact `line_basis_s16` observations
    // from collision-runtime-20260826-2. They exercise the nonvertical path
    // with mixed signs and nonzero values in all three source components.
    const auto check_basis_fixture = [](const RawVec3& start,
                                        const RawVec3& end,
                                        Raw expected_length,
                                        const std::array<std::int16_t, 9>& expected_basis) {
        QueryRecord fixture;
        fixture.start = start;
        fixture.end = end;
        prepare(fixture);
        CHECK(fixture.line_length == expected_length);
        CHECK(fixture.line_basis == expected_basis);
    };
    check_basis_fixture(
        {23928832, -6352896, 11186176},
        {23926261, -6220058, 9440996},
        428,
        {-4096, 0, 9, 0, 4084, 306, -9, 306, -4084});
    check_basis_fixture(
        {23928832, -6352896, 11186176},
        {24104960, -6754304, 15364096},
        1025,
        {4092, 0, -172, 16, 4077, 390, 171, -391, 4073});
    check_basis_fixture(
        {0, -450560, 0},
        {0, -374133, 1597584},
        390,
        {4096, 0, 0, 0, 4091, -188, 0, 188, 4091});
    check_basis_fixture(
        {0, 0, 0},
        {-2372780, -177737, 758952},
        610,
        {1244, 0, 3902, -282, 4085, 89, -3892, -295, 1240});
    check_basis_fixture(
        {-204550, 0, 0},
        {409100, -541140, 2310720},
        598,
        {3960, 0, -1046, 232, 3993, 879, 1019, -910, 3860});
    check_basis_fixture(
        {204550, 0, 0},
        {-409100, -541140, 2310720},
        598,
        {3958, 0, 1052, -234, 3993, 879, -1026, -910, 3858});
    check_basis_fixture(
        {0, -450560, 0},
        {0, -342411, 1462128},
        356,
        {4096, 0, 0, 0, 4085, -298, 0, 298, 4085});
    check_basis_fixture(
        {0, -450560, 0},
        {0, -355473, 1517904},
        370,
        {4096, 0, 0, 0, 4088, -254, 0, 254, 4088});
    check_basis_fixture(
        {0, -450560, 0},
        {0, -332148, 1418304},
        347,
        {4096, 0, 0, 0, 4082, -330, 0, 330, 4082});

    const auto raw_query = to_query_layout(query);
    const auto round_trip = from_query_layout(raw_query);
    CHECK(round_trip.start == query.start);
    CHECK(round_trip.end == query.end);
    CHECK(round_trip.line_basis == query.line_basis);
    CHECK(round_trip.hit_position == query.hit_position);
    CHECK(round_trip.hit_parameter == query.hit_parameter);

    QueryRecord triangle_query;
    triangle_query.start = {1 << 12, 1 << 12, 4096 << 12};
    triangle_query.end = {1 << 12, 1 << 12, -4096 << 12};
    prepare(triangle_query);
    FaceGeometry triangle;
    triangle.vertex0 = {0, 0, 0};
    // The recovered side predicates expect the model's stored winding to be
    // opposite the positive plane-normal convention used by this fixture.
    triangle.vertex1 = {0, 10, 0};
    triangle.vertex2 = {10, 0, 0};
    triangle.plane_normal = {0, 0, 1};
    triangle.is_triangle = true;
    CHECK(record_nearest_face_candidate(triangle_query, triangle, {0, 0, 0},
                                         0x10, 0x20, 7));
    CHECK(triangle_query.hit_parameter == 8192);
    CHECK(triangle_query.hit_distance == 4096);
    const RawVec3 expected_triangle_contact{4096, 4096, 0};
    CHECK(triangle_query.hit_position == expected_triangle_contact);

    QueryRecord quad_query;
    quad_query.start = triangle_query.start;
    quad_query.end = triangle_query.end;
    prepare(quad_query);
    FaceGeometry quad = triangle;
    quad.vertex2 = {10, 10, 0};
    quad.vertex3 = {10, 0, 0};
    quad.is_triangle = false;
    CHECK(record_nearest_face_candidate(quad_query, quad, {0, 0, 0},
                                         0x11, 0x21, 8));
    CHECK(quad_query.hit_parameter == 8192);

    QueryRecord arithmetic_shift;
    arithmetic_shift.start = {0, 0, 0};
    arithmetic_shift.end = {0, 0, -1};
    prepare(arithmetic_shift);
    CHECK(arithmetic_shift.line_length == 1);
    CHECK(arithmetic_shift.bounds_min[2] == -1);
    CHECK(arithmetic_shift.bounds_max[2] == 0);

    QueryRecord vertical;
    vertical.start = {0, 0, 0};
    vertical.end = {0, 4096, 0};
    prepare(vertical);
    CHECK(vertical.line_length == 1);
    CHECK(vertical.direction_flag == 1);
    const std::array<std::int16_t, 9> expected_vertical_basis{
        0x1000, 0, 0, 0, 0, -0x1000, 0, 0x1000, 0};
    CHECK(vertical.line_basis == expected_vertical_basis);

    // Exercise the recovered model-data and variable-length face record
    // views without involving a level-file parser.
    std::array<std::uint8_t, 0x100> model_bytes{};
    const auto put16 = [&model_bytes](std::size_t offset, std::uint16_t value) {
        model_bytes[offset] = static_cast<std::uint8_t>(value);
        model_bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8u);
    };
    const auto put_i16 = [&put16](std::size_t offset, std::int16_t value) {
        put16(offset, static_cast<std::uint16_t>(value));
    };
    put16(0x02, 4);  // vertices
    put16(0x04, 1);  // normals
    put16(0x06, 1);  // faces
    const auto vertex_base = std::size_t{0x1c};
    const auto put_vertex = [&put_i16, vertex_base](std::size_t index,
                                                    std::int16_t x,
                                                    std::int16_t y,
                                                    std::int16_t z) {
        const auto offset = vertex_base + index * 8;
        put_i16(offset, x);
        put_i16(offset + 2, y);
        put_i16(offset + 4, z);
    };
    put_vertex(0, 0, 0, 0);
    put_vertex(1, 0, 10, 0);
    put_vertex(2, 10, 0, 0);
    put_vertex(3, 10, 10, 0);
    put_i16(vertex_base + 4 * 8, 0);
    put_i16(vertex_base + 4 * 8 + 2, 0);
    put_i16(vertex_base + 4 * 8 + 4, 4096);
    const auto face_offset = vertex_base + 4 * 8 + 8;
    put16(face_offset, 0x0010);       // triangle bit
    put16(face_offset + 2, 0x0010);   // 16-byte minimum face record
    model_bytes[face_offset + 4] = 0;
    model_bytes[face_offset + 5] = 1;
    model_bytes[face_offset + 6] = 2;
    model_bytes[face_offset + 7] = 3;  // ignored on triangle path
    put16(face_offset + 0x0c, 0);      // normal index << 3
    put16(face_offset + 0x0e, 0);      // surface flags

    QueryRecord model_query;
    model_query.start = {4096, 4096, 4096};
    model_query.end = {4096, 4096, -4096};
    prepare(model_query);
    CollisionModelView model_view{
        .bytes = std::span<const std::uint8_t>(model_bytes),
        .body_id = 0x1234,
        .face_address_base = 0x5000,
    };
    CHECK(model_view.face(0));
    CHECK(model_view.face(0)->record_offset == face_offset);
    std::vector<CollisionFaceAabbRecord> model_aabbs;
    visit_model_face_aabbs(model_view, {100, 200, 300},
                           [&model_aabbs](std::uint16_t, const CollisionFaceAabbRecord& aabb) {
                               model_aabbs.push_back(aabb);
                           });
    CHECK(model_aabbs.size() == 1);
    CHECK(model_aabbs[0].min_x == 100);
    CHECK(model_aabbs[0].min_y == 200);
    CHECK(model_aabbs[0].min_z == 300);
    CHECK(model_aabbs[0].max_x == 100 + 10 * 0x1000);
    CHECK(model_aabbs[0].max_y == 200 + 10 * 0x1000);
    CHECK(model_aabbs[0].max_z == 300);
    CHECK(query_model_faces(model_query, model_view, {0, 0, 0}, 9) == 1);
    CHECK(model_query.hit_body == 0x1234);
    CHECK(model_query.hit_face_record == 0x5000u + face_offset);
    const RawVec3 expected_model_contact{4096, 4096, 0};
    CHECK(model_query.hit_position == expected_model_contact);

    std::array<DynamicVertexRecord, 4> transformed_vertices{};
    const std::array<std::int16_t, 9> identity_q12{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};
    CHECK(transform_model_vertices(model_view, {0, 0, 0}, identity_q12,
                                     20, transformed_vertices) == 0);
    CHECK(transformed_vertices[1].x == 0);
    CHECK(transformed_vertices[1].y == 10);
    CHECK(transformed_vertices[2].x == 10);
    CHECK(transformed_vertices[2].y == 0);
    const auto dynamic_indices = dynamic_face_indices(0x03020100);
    CHECK(dynamic_indices.vertex0 == 0);
    CHECK(dynamic_indices.vertex1 == 1);
    CHECK(dynamic_indices.vertex2 == 2);
    CHECK(dynamic_indices.vertex3 == 3);
    CHECK(dynamic_face_clip_accepts(transformed_vertices[0],
                                     transformed_vertices[1],
                                     transformed_vertices[2],
                                     transformed_vertices[3]));
    std::array<DynamicVertexRecord, 4> saturated_vertices{};
    CHECK(transform_model_vertices(model_view, {40000, 40000, 40000},
                                    identity_q12, 20, saturated_vertices) ==
           0x608);
    CHECK(saturated_vertices[0].x == 32767);
    CHECK(saturated_vertices[0].y == 32767);
    CHECK(saturated_vertices[0].z == 32767);
    CHECK(saturated_vertices[0].clip_mask == 0x608);
    const auto projected_triangle = dynamic_projected_face(
        transformed_vertices[0], transformed_vertices[1],
        transformed_vertices[2], transformed_vertices[3], true);
    CHECK(!projected_triangle.accepted);
    std::array<DynamicVertexRecord, 4> quad_vertices{
        DynamicVertexRecord{0, 0, 0, 0},
        DynamicVertexRecord{10, 0, 0, 0},
        DynamicVertexRecord{0, 10, 0, 0},
        DynamicVertexRecord{10, 10, 0, 0},
    };
    const auto projected_quad = dynamic_projected_face(
        quad_vertices[0], quad_vertices[1], quad_vertices[2],
        quad_vertices[3], false);
    CHECK(projected_quad.accepted);
    CHECK(projected_quad.first_vertex == 0);

    std::array<DynamicVertexRecord, 4> candidate_vertices{
        DynamicVertexRecord{0, 0, 10, 0},
        DynamicVertexRecord{10, 0, 10, 0},
        DynamicVertexRecord{0, 10, 10, 0},
        DynamicVertexRecord{10, 10, 10, 0},
    };
    const std::array<std::int16_t, 3> candidate_normal{0, 0, -4096};
    const auto dynamic_candidate = dynamic_face_candidate(
        candidate_vertices[0], candidate_vertices[1], candidate_vertices[2],
        candidate_vertices[3], false, candidate_normal, identity_q12, 100);
    CHECK(dynamic_candidate);
    CHECK(dynamic_candidate->distance == 10);
    CHECK(dynamic_candidate->query_normal == candidate_normal);

    QueryRecord dynamic_query;
    dynamic_query.start = {0, 0, 0};
    dynamic_query.end = {0, 0, 409600};
    prepare(dynamic_query);
    CHECK(record_nearest_dynamic_candidate(dynamic_query, *dynamic_candidate,
                                            7, 9, 11));
    CHECK(dynamic_query.hit_body == 7);
    CHECK(dynamic_query.hit_distance == 10);
    const auto dynamic_contact = dynamic_contact_at_distance(dynamic_query);
    CHECK(dynamic_contact);
    CHECK(dynamic_contact->at(2) == 40900);

    QueryRecord dynamic_transform_query;
    dynamic_transform_query.start = {0, 0, 0};
    dynamic_transform_query.end = {0, 4096, 0};
    prepare(dynamic_transform_query);
    const RawVec3 dynamic_object_position{4096, 8192, 12288};
    const auto fast_transform = build_dynamic_object_transform(
        dynamic_transform_query, dynamic_object_position, {0, 0, 0});
    CHECK((fast_transform.model_origin_units == RawVec3{1, 2, 3}));
    CHECK(fast_transform.vertex_basis == dynamic_transform_query.line_basis);
    CHECK(fast_transform.final_basis == identity_q12_basis());
    const auto oriented_transform = build_dynamic_object_transform(
        dynamic_transform_query, dynamic_object_position, {0, 0, 0}, true);
    CHECK((oriented_transform.model_origin_units == RawVec3{0, 0, 0}));
    CHECK((oriented_transform.transformed_translation == RawVec3{1, -3, 2}));
    CHECK(oriented_transform.vertex_basis == dynamic_transform_query.line_basis);
    CHECK(oriented_transform.normal_basis == dynamic_transform_query.line_basis);
    CHECK(oriented_transform.final_basis == identity_q12_basis());
    const auto scaled_transform = build_dynamic_object_transform(
        dynamic_transform_query, dynamic_object_position, {0, 0, 0}, true,
        std::array<std::int16_t, 3>{0x0800, 0x1000, 0x2000});
    const std::array<std::int16_t, 9> expected_scaled_basis{
        0x0800, 0, 0, 0, 0, static_cast<std::int16_t>(-0x2000),
        0, 0x1000, 0};
    CHECK(scaled_transform.vertex_basis == expected_scaled_basis);
    CHECK(scaled_transform.normal_basis == expected_scaled_basis);
    CHECK(scaled_transform.final_basis == identity_q12_basis());

    const auto rotated_transform = build_dynamic_object_transform(
        dynamic_transform_query, dynamic_object_position, {0x0400, 0x0800,
                                                            0x0c00}, true);
    const auto rotated_object_basis = build_object_rotation_basis(
        {0x0400, 0x0800, 0x0c00});
    CHECK(rotated_transform.vertex_basis == compose_q12_basis(
        dynamic_transform_query.line_basis, rotated_object_basis));
    CHECK(rotated_transform.normal_basis == rotated_transform.vertex_basis);
    CHECK(rotated_transform.final_basis == rotated_object_basis);

    // A quad whose first projected determinant is negative takes the v3
    // alternate triangle path in 0x004f4c50. Give v0 and v3 different depth
    // values so using the wrong first vertex is observable in the distance.
    std::array<DynamicVertexRecord, 4> alternate_quad{
        DynamicVertexRecord{0, 0, 100, 0},
        DynamicVertexRecord{0, 10, 0, 0},
        DynamicVertexRecord{10, 0, 0, 0},
        DynamicVertexRecord{-10, -10, 20, 0},
    };
    const auto alternate_projected = dynamic_projected_face(
        alternate_quad[0], alternate_quad[1], alternate_quad[2],
        alternate_quad[3], false);
    CHECK(alternate_projected.accepted);
    CHECK(alternate_projected.first_vertex == 3);
    const auto alternate_candidate = dynamic_face_candidate(
        alternate_quad[0], alternate_quad[1], alternate_quad[2],
        alternate_quad[3], false, candidate_normal, identity_q12, 100);
    CHECK(alternate_candidate);
    CHECK(alternate_candidate->distance == 20);

    QueryRecord translated_query;
    translated_query.start = {8192, 8192, 4096};
    translated_query.end = {8192, 8192, -4096};
    prepare(translated_query);
    CHECK(query_model_faces(translated_query, model_view, {0, 0, 0}, 9) == 1);
    const RawVec3 expected_translated_contact{8192, 8192, 0};
    CHECK(translated_query.hit_position == expected_translated_contact);

    CollisionZoneGrid zone{
        .min_x = -5000,
        .min_z = -5000,
        .max_x = 5000,
        .max_z = 5000,
        .cell_divisor = 10,
        .cell_count_x = 20,
        .cell_count_z = 10,
    };
    CHECK(zone_overlaps_query(zone, model_query));
    CHECK(zone_candidate_index(3, 4, 5, zone) == 3 * 0x198 + 4 * 0x14 + 5);
    CHECK(!zone_candidate_index(3, 20, 0, zone));

    CollisionZoneGrid walk_zone{
        .min_x = 0,
        .min_z = 0,
        .max_x = 100,
        .max_z = 100,
        .cell_divisor = 10,
        .cell_count_x = 10,
        .cell_count_z = 10,
    };
    QueryRecord horizontal_query;
    horizontal_query.start = {1, 0, 1};
    horizontal_query.end = {99, 0, 1};
    prepare(horizontal_query);
    std::vector<std::size_t> horizontal_cells;
    visit_zone_cells(horizontal_query, 2, walk_zone,
                     [&horizontal_cells](std::size_t index, std::int32_t, std::int32_t) {
                         horizontal_cells.push_back(index);
                     });
    CHECK(horizontal_cells.size() == 10);
    CHECK(horizontal_cells.front() == 2 * 0x198);
    CHECK(horizontal_cells.back() == 2 * 0x198 + 9 * 0x14);

    CHECK(arithmetic_shift_right_12(-33'554'432) == -8192);

    const std::array<std::int16_t, 3> airborne_normal{1, -2897, 2897};
    CHECK(finalize_hit(query, airborne_normal));
    CHECK(query.hit_normal == airborne_normal);
    const std::array<std::int16_t, 9> identity_basis{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};
    CHECK(finalize_hit(query, identity_basis, airborne_normal));
    CHECK(query.hit_normal == airborne_normal);
    const std::array<std::int16_t, 3> zero_angles{0, 0, 0};
    CHECK(build_object_rotation_basis(zero_angles) == identity_basis);
    CHECK(finalize_hit(query, zero_angles, airborne_normal));
    CHECK(query.hit_normal == airborne_normal);
    const auto normal_squared =
        static_cast<std::uint64_t>(std::int64_t{airborne_normal[0]} * airborne_normal[0]) +
        static_cast<std::uint64_t>(std::int64_t{airborne_normal[1]} * airborne_normal[1]) +
        static_cast<std::uint64_t>(std::int64_t{airborne_normal[2]} * airborne_normal[2]);
    CHECK(normal_squared > std::uint64_t{4096} * 4096);
    CHECK(normal_squared < std::uint64_t{4098} * 4098);

    QueryRecord no_hit;
    CHECK(!finalize_hit(no_hit, airborne_normal));

    // q+0x8c is the nearest-candidate comparator, not just a last-hit field.
    CHECK(!record_nearest_plane_candidate(query, 4202, 0, 1, 2, 3));
    CHECK(record_nearest_plane_candidate(query, 1000, -15384, 4, 5, 6));
    CHECK(query.hit_parameter == 1000);

    const auto flags = decode_face_flags(0x80, 0x04200008);
    CHECK(!flags.surface_bit_40);
    CHECK(!flags.is_triangle);
    CHECK(flags.base_nonphysical);
    CHECK(!flags.surface_wallrideable);
    CHECK(!flags.surface_large_polygon);
    CHECK(flags.surface_skateable);
    CHECK(flags.inverse_bit_23);
    CHECK(flags.face_bit_80);
    CHECK(flags.inverse_bit_24);
    CHECK(flags.surface_class == 2);

    const auto surface_flags = decode_face_flags(0, 0x04700000);
    CHECK(surface_flags.surface_bit_40);
    CHECK(surface_flags.surface_large_polygon);
    CHECK(surface_flags.surface_wallrideable);
    CHECK(surface_flags.surface_class == 2);

    // M3D_ReadSurfaceFlags (0x0048ea80) publishes raw masks and preserves
    // the shared state when q+0x68 does not identify a hit.
    CollisionResultSurfaceFlags preserved_flags{
        .surface_bit_40 = 0xaaaaaaaau,
        .inverse_bit_24 = 0x11111111u,
        .inverse_bit_23 = 0x22222222u,
        .face_bit_80 = 0x33333333u,
        .surface_class = 0x44444444u,
        .face_record = 0x55555555u,
    };
    QueryRecord no_surface_hit;
    read_surface_flags(no_surface_hit, 0x1083u, 0x04400000u,
                       preserved_flags);
    CHECK(preserved_flags.surface_bit_40 == 0xaaaaaaaau);
    CHECK(preserved_flags.inverse_bit_24 == 0x11111111u);
    CHECK(preserved_flags.inverse_bit_23 == 0x22222222u);
    CHECK(preserved_flags.face_bit_80 == 0x33333333u);
    CHECK(preserved_flags.surface_class == 0x44444444u);
    CHECK(preserved_flags.face_record == 0x55555555u);

    const auto check_surface_fixture =
        [](std::uint32_t base_word, std::uint32_t surface_word,
           std::uint32_t expected_surface_bit_40,
           std::uint32_t expected_inverse_bit_24,
           std::uint32_t expected_inverse_bit_23,
           std::uint32_t expected_face_bit_80,
           std::uint32_t expected_surface_class) {
            QueryRecord surface_hit;
            surface_hit.hit_body = 0x12345678u;
            surface_hit.hit_face_record = 0x87654321u;
            CollisionResultSurfaceFlags decoded;
            read_surface_flags(surface_hit, base_word, surface_word, decoded);
            CHECK(decoded.surface_bit_40 == expected_surface_bit_40);
            CHECK(decoded.inverse_bit_24 == expected_inverse_bit_24);
            CHECK(decoded.inverse_bit_23 == expected_inverse_bit_23);
            CHECK(decoded.face_bit_80 == expected_face_bit_80);
            CHECK(decoded.surface_class == expected_surface_class);
            CHECK(decoded.face_record == 0x87654321u);
        };
    // These combinations are the raw/derived rows from the runtime
    // collision-materials1 capture.
    check_surface_fixture(0x1083u, 0x00100000u, 0, 1, 1, 0x80, 0);
    check_surface_fixture(0x18a3u, 0x04000000u, 0, 1, 1, 0x80, 2);
    check_surface_fixture(0x18a3u, 0x04200000u, 0, 1, 1, 0x80, 2);
    check_surface_fixture(0x0823u, 0x04400000u, 0x40, 1, 1, 0, 2);
    // Set source bits 23 and 24 to exercise both inverse-zero results.
    check_surface_fixture(0, 0x01800000u, 0, 0, 0, 0, 0);

    std::cout << "collision reference checks passed\n";
}
