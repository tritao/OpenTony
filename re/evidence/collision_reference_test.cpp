#include "collision_reference.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace opentony::collision_reference;

int main() {
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

    QueryRecord arithmetic_shift;
    arithmetic_shift.start = {0, 0, 0};
    arithmetic_shift.end = {0, 0, -1};
    prepare(arithmetic_shift);
    assert(arithmetic_shift.line_length == 1);
    assert(arithmetic_shift.bounds_min[2] == -1);
    assert(arithmetic_shift.bounds_max[2] == 0);

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
    assert(flags.inverse_bit_23);
    assert(flags.face_bit_80);
    assert(flags.inverse_bit_24);
    assert(flags.surface_class == 2);

    const auto surface_flags = decode_face_flags(0, 0x04600000);
    assert(surface_flags.surface_bit_40);

    std::cout << "collision reference checks passed\n";
}
