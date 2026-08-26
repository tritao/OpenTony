#include "collision_reference.hpp"

#include <cassert>
#include <array>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace opentony::collision_reference;

int main() {
    CollisionModelHeader model;
    model.vertex_count = 14;
    model.normal_count = 6;
    model.face_count = 6;
    assert(model_normal_offset(model) == 0x8c);
    assert(model_face_offset(model) == 0xbc);
    assert(model_record_offset(13) == 13u * 8u);

    assert(sizeof(CollisionQueryLayout) == 0x90);
    assert(offsetof(CollisionQueryLayout, line_basis) == 0x48);
    assert(offsetof(CollisionQueryLayout, hit_parameter) == 0x8c);

    CollisionFacePrefix face;
    face.base_flags = 0x1083;
    face.length_bytes = 0x1c;
    face.normal_index_shifted = 0x20;
    face.surface_flags = 0x10;
    assert(face_record_stride_bytes(0x001c1083) == 0x20);
    assert(face_normal_index(face.normal_index_shifted) == 4);

    CollisionModelCacheEntry cache_entry;
    cache_entry.face_aabb_start = 17;
    assert(cache_entry.face_aabb_start == 17);
    CollisionFaceAabbRecord aabb;
    aabb.min_x = -10;
    aabb.max_z = 20;
    assert(aabb.min_x == -10 && aabb.max_z == 20);

    QueryRecord query;
    query.start = {-4098781, -647119, -16628281};
    query.end = {-4098749, -420744, -16813694};
    prepare(query, 0x1234);

    // This is the airborne runtime hit recorded in collision-air4.trace.ndjson.
    assert(query.line_length == 71);
    assert(query.query_stamp == 0x1234);
    assert(record_nearest_plane_candidate(query, -14283, 2101, 0x05f2dcfc,
                                          0x05db3534, 132));
    assert(query.hit_parameter == 2101);
    assert(query.hit_distance == 9);
    const RawVec3 expected_contact{-4098777, -618090, -16652057};
    assert(query.hit_position == expected_contact);
    assert(query.hit_body == 0x05f2dcfc);
    assert(query.hit_face_record == 0x05db3534);
    assert(query.hit_model_index == 132);

    const auto raw_query = to_query_layout(query);
    const auto round_trip = from_query_layout(raw_query);
    assert(round_trip.start == query.start);
    assert(round_trip.end == query.end);
    assert(round_trip.line_basis == query.line_basis);
    assert(round_trip.hit_position == query.hit_position);
    assert(round_trip.hit_parameter == query.hit_parameter);

    QueryRecord triangle_query;
    triangle_query.start = {1 << 12, 1 << 12, 4096 << 12};
    triangle_query.end = {1 << 12, 1 << 12, -4096 << 12};
    prepare(triangle_query);
    FaceGeometry triangle;
    triangle.vertex0 = {0, 0, 0};
    triangle.vertex1 = {10, 0, 0};
    triangle.vertex2 = {0, 10, 0};
    triangle.plane_normal = {0, 0, 1};
    triangle.is_triangle = true;
    assert(record_nearest_face_candidate(triangle_query, triangle, {0, 0, 0},
                                         0x10, 0x20, 7));
    assert(triangle_query.hit_parameter == 8192);
    assert(triangle_query.hit_distance == 4096);
    const RawVec3 expected_triangle_contact{4096, 4096, 0};
    assert(triangle_query.hit_position == expected_triangle_contact);

    QueryRecord quad_query;
    quad_query.start = triangle_query.start;
    quad_query.end = triangle_query.end;
    prepare(quad_query);
    FaceGeometry quad = triangle;
    quad.vertex2 = {10, 10, 0};
    quad.vertex3 = {0, 10, 0};
    quad.is_triangle = false;
    assert(record_nearest_face_candidate(quad_query, quad, {0, 0, 0},
                                         0x11, 0x21, 8));
    assert(quad_query.hit_parameter == 8192);

    QueryRecord arithmetic_shift;
    arithmetic_shift.start = {0, 0, 0};
    arithmetic_shift.end = {0, 0, -1};
    prepare(arithmetic_shift);
    assert(arithmetic_shift.line_length == 1);
    assert(arithmetic_shift.bounds_min[2] == -1);
    assert(arithmetic_shift.bounds_max[2] == 0);

    QueryRecord vertical;
    vertical.start = {0, 0, 0};
    vertical.end = {0, 4096, 0};
    prepare(vertical);
    assert(vertical.line_length == 1);
    assert(vertical.direction_flag == 1);
    const std::array<std::int16_t, 9> expected_vertical_basis{
        0x1000, 0, 0, 0, 0, -0x1000, 0, 0x1000, 0};
    assert(vertical.line_basis == expected_vertical_basis);

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
    put_vertex(1, 10, 0, 0);
    put_vertex(2, 0, 10, 0);
    put_vertex(3, 10, 10, 0);
    put_i16(vertex_base + 4 * 8, 0);
    put_i16(vertex_base + 4 * 8 + 2, 0);
    put_i16(vertex_base + 4 * 8 + 4, 4096);
    const auto face_offset = vertex_base + 4 * 8 + 8;
    put16(face_offset, 0x0010);       // triangle bit
    put16(face_offset + 2, 0x000c);   // 12-byte payload; 16-byte record
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
    assert(model_view.face(0));
    assert(model_view.face(0)->record_offset == face_offset);
    std::vector<CollisionFaceAabbRecord> model_aabbs;
    visit_model_face_aabbs(model_view, {100, 200, 300},
                           [&model_aabbs](std::uint16_t, const CollisionFaceAabbRecord& aabb) {
                               model_aabbs.push_back(aabb);
                           });
    assert(model_aabbs.size() == 1);
    assert(model_aabbs[0].min_x == 100);
    assert(model_aabbs[0].max_y == 200 + 10 * 0x1000);
    assert(query_model_faces(model_query, model_view, {0, 0, 0}, 9) == 1);
    assert(model_query.hit_body == 0x1234);
    assert(model_query.hit_face_record == 0x5000u + face_offset);
    const RawVec3 expected_model_contact{4096, 4096, 0};
    assert(model_query.hit_position == expected_model_contact);

    CollisionZoneGrid zone{
        .min_x = -5000,
        .min_z = -5000,
        .max_x = 5000,
        .max_z = 5000,
        .cell_divisor = 10,
        .cell_count_x = 20,
        .cell_count_z = 10,
    };
    assert(zone_overlaps_query(zone, model_query));
    assert(zone_candidate_index(3, 4, 5, zone) == 3 * 0x198 + 4 * 0x14 + 5);
    assert(!zone_candidate_index(3, 20, 0, zone));

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
    assert(horizontal_cells.size() == 10);
    assert(horizontal_cells.front() == 2 * 0x198);
    assert(horizontal_cells.back() == 2 * 0x198 + 9 * 0x14);

    assert(arithmetic_shift_right_12(-33'554'432) == -8192);

    const std::array<std::int16_t, 3> airborne_normal{1, -2897, 2897};
    assert(finalize_hit(query, airborne_normal));
    assert(query.hit_normal == airborne_normal);
    const std::array<std::int16_t, 9> identity_basis{
        0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};
    assert(finalize_hit(query, identity_basis, airborne_normal));
    assert(query.hit_normal == airborne_normal);
    const std::array<std::int16_t, 3> zero_angles{0, 0, 0};
    assert(build_object_rotation_basis(zero_angles) == identity_basis);
    assert(finalize_hit(query, zero_angles, airborne_normal));
    assert(query.hit_normal == airborne_normal);
    const auto normal_squared =
        static_cast<std::uint64_t>(std::int64_t{airborne_normal[0]} * airborne_normal[0]) +
        static_cast<std::uint64_t>(std::int64_t{airborne_normal[1]} * airborne_normal[1]) +
        static_cast<std::uint64_t>(std::int64_t{airborne_normal[2]} * airborne_normal[2]);
    assert(normal_squared > std::uint64_t{4096} * 4096);
    assert(normal_squared < std::uint64_t{4098} * 4098);

    QueryRecord no_hit;
    assert(!finalize_hit(no_hit, airborne_normal));

    // q+0x8c is the nearest-candidate comparator, not just a last-hit field.
    assert(!record_nearest_plane_candidate(query, 0, 4202, 1, 2, 3));
    assert(record_nearest_plane_candidate(query, -15384, 1000, 4, 5, 6));
    assert(query.hit_parameter == 1000);

    const auto flags = decode_face_flags(0x80, 0x04200008);
    assert(!flags.surface_bit_40);
    assert(!flags.is_triangle);
    assert(flags.base_nonphysical);
    assert(!flags.surface_wallrideable);
    assert(!flags.surface_large_polygon);
    assert(flags.surface_skateable);
    assert(flags.inverse_bit_23);
    assert(flags.face_bit_80);
    assert(flags.inverse_bit_24);
    assert(flags.surface_class == 2);

    const auto surface_flags = decode_face_flags(0, 0x04700000);
    assert(surface_flags.surface_bit_40);
    assert(surface_flags.surface_large_polygon);
    assert(surface_flags.surface_wallrideable);
    assert(surface_flags.surface_class == 2);

    std::cout << "collision reference checks passed\n";
}
