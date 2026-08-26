#include "psx_collision.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void put16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    put16(bytes, offset, static_cast<std::uint16_t>(value));
    put16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

void put_i16(std::vector<std::byte>& bytes, std::size_t offset, std::int16_t value) {
    put16(bytes, offset, static_cast<std::uint16_t>(value));
}

void put_i32(std::vector<std::byte>& bytes, std::size_t offset, std::int32_t value) {
    put32(bytes, offset, static_cast<std::uint32_t>(value));
}

std::vector<std::byte> synthetic_archive() {
    constexpr std::size_t object_offset = 12;
    constexpr std::size_t model_count_offset = object_offset + 36;
    constexpr std::size_t model_table_offset = model_count_offset + 4;
    constexpr std::size_t model_offset = model_table_offset + 4;
    constexpr std::size_t model_size = 28 + 4 * 8 + 8 + 16;
    constexpr std::size_t tag_offset = model_offset + model_size;
    constexpr std::size_t blockmap_size = 40;
    constexpr std::size_t terminator_offset = tag_offset + 8 + blockmap_size;
    std::vector<std::byte> bytes(terminator_offset + 4, std::byte{0});

    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    put32(bytes, 4, static_cast<std::uint32_t>(tag_offset));
    put32(bytes, 8, 1);

    // One placed object at the origin, using model zero.
    put16(bytes, object_offset + 26, 0);

    put32(bytes, model_count_offset, 1);
    put32(bytes, model_table_offset, static_cast<std::uint32_t>(model_offset));
    put16(bytes, model_offset + 0, 0);
    put16(bytes, model_offset + 2, 4);
    put16(bytes, model_offset + 4, 1);
    put16(bytes, model_offset + 6, 1);

    const std::array<std::array<std::int16_t, 3>, 4> vertices{
        std::array<std::int16_t, 3>{-4096, 0, -4096},
        std::array<std::int16_t, 3>{4096, 0, -4096},
        std::array<std::int16_t, 3>{4096, 0, 4096},
        std::array<std::int16_t, 3>{-4096, 0, 4096},
    };
    std::size_t cursor = model_offset + 28;
    for (const auto& vertex : vertices) {
        put_i16(bytes, cursor, vertex[0]);
        put_i16(bytes, cursor + 2, vertex[1]);
        put_i16(bytes, cursor + 4, vertex[2]);
        cursor += 8;
    }
    put_i16(bytes, cursor, 0);
    put_i16(bytes, cursor + 2, 4096);
    put_i16(bytes, cursor + 4, 0);
    cursor += 8;

    put16(bytes, cursor, 0);
    put16(bytes, cursor + 2, 16);
    bytes[cursor + 4] = static_cast<std::byte>(0);
    bytes[cursor + 5] = static_cast<std::byte>(1);
    bytes[cursor + 6] = static_cast<std::byte>(2);
    bytes[cursor + 7] = static_cast<std::byte>(3);
    put16(bytes, cursor + 12, 0);
    put16(bytes, cursor + 14, 0x1234);

    put32(bytes, tag_offset, 0x0000000aU);
    put32(bytes, tag_offset + 4, blockmap_size);
    put_i32(bytes, tag_offset + 8, 0);
    put_i32(bytes, tag_offset + 12, 0);
    put_i32(bytes, tag_offset + 16, 40960);
    put_i32(bytes, tag_offset + 20, 40960);
    put16(bytes, tag_offset + 24, 1);
    put16(bytes, tag_offset + 26, 1);
    put32(bytes, tag_offset + 28, 0);
    put32(bytes, tag_offset + 32, 0);
    put32(bytes, tag_offset + 36, 1);
    put32(bytes, tag_offset + 40, 0);
    put32(bytes, tag_offset + 44, 0);
    put32(bytes, terminator_offset, 0xffffffffU);
    return bytes;
}

} // namespace

int main() {
    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(synthetic_archive(), "collision.psx");
    const opentony::assets::PsxCollisionWorld world =
        opentony::assets::PsxCollisionWorld::build(archive);
    assert(world.referenced_object_count() == 1);
    assert(world.faces().size() == 1);
    assert(world.grids().size() == 1);
    assert(world.grids()[0].cells[0].faces.size() == 1);
    const std::array<std::int32_t, 3> expected_vertex{
        -0x1000000,
        0,
        -0x1000000,
    };
    assert(world.faces()[0].vertices[0] == expected_vertex);
    assert(world.candidate_faces({0, 0, 0}, 0).size() == 1);
    const auto hit = world.trace_segment({0, 4096, 0}, {0, -4096, 0});
    assert(hit.has_value());
    assert(hit->face_index == 0);
    assert(hit->position[1] == 0);
    assert(hit->hit_parameter_q14 == 0x2000);
    assert(hit->raw_collision_word == 0x12340000U);
    const auto mask = hit->collision_mask();
    assert(mask.normal_index == 0);
    assert(mask.surface_flags == 0x1234);
    assert(!mask.surface_bit_6);
    assert(mask.surface_bit_7_clear);
    assert(mask.surface_bit_8_clear);
    assert(mask.raw_type_bits_9_12 == 9);
    assert(!mask.face_flag_80);
    opentony::assets::PsxCollisionQueryOptions retail_filter{};
    retail_filter.apply_retail_face_filter = true;
    assert(opentony::assets::accepts_retail_collision_face(
        0x12340000U,
        retail_filter));
    assert(!opentony::assets::accepts_retail_collision_face(
        0x00010000U,
        retail_filter));
    assert(!opentony::assets::accepts_retail_collision_face(
        0x00020000U,
        retail_filter));
    retail_filter.include_trigger_faces = true;
    assert(opentony::assets::accepts_retail_collision_face(
        0x00020000U,
        retail_filter));
    retail_filter.reject_mask = 0x12340000U;
    assert(!opentony::assets::accepts_retail_collision_face(
        0x12340000U,
        retail_filter));
    retail_filter.reject_mask = 0;
    retail_filter.accept_mask = 0xfffffffeU;
    assert(!opentony::assets::accepts_retail_collision_face(
        0x12340000U,
        retail_filter));
    assert(opentony::assets::accepts_retail_collision_face(
        0x12340001U,
        retail_filter));
    assert(!world.trace_segment(
        {0, 4096, 0},
        {0, -4096, 0},
        [] {
            opentony::assets::PsxCollisionQueryOptions options{};
            options.apply_retail_face_filter = true;
            options.reject_mask = 0x12340000U;
            return options;
        }()).has_value());
    opentony::assets::PsxCollisionQueryOptions retail_geometry{};
    retail_geometry.apply_retail_face_filter = true;
    retail_geometry.apply_retail_plane_test = true;
    assert(!world.trace_segment(
        {0, 4096, 0},
        {0, -4096, 0},
        retail_geometry).has_value());
    assert(world.trace_segment(
        {0, -4096, 0},
        {0, 4096, 0},
        retail_geometry).has_value());
    const auto quantized_hit = world.trace_segment({0, 4097, 0}, {0, -4095, 0});
    assert(quantized_hit.has_value());
    assert(quantized_hit->hit_parameter_q14 == 0x2000);
    assert(quantized_hit->position[1] == 1);
    assert(hit->fraction > 0.49 && hit->fraction < 0.51);
    std::cout << "PSX collision tests passed\n";
}
