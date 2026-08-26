#include "collision_reference.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace opentony::collision_reference;

int main() {
    CollisionModelHeader model;
    model.vertex_count = 14;
    model.normal_count = 6;
    model.face_count = 6;
    assert(model_normal_offset(model) == 0x8c);
    assert(model_face_offset(model) == 0xbc);
    assert(model_record_offset(13) == 13u * 8u);

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

    assert(arithmetic_shift_right_12(-33'554'432) == -8192);

    const std::array<std::int16_t, 3> airborne_normal{1, -2897, 2897};
    assert(finalize_hit(query, airborne_normal));
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
