#include "psx_collision_probe.hpp"

#include "../assets/psx_collision.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
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

void check_packaged_scene(const char* path) {
    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::load(path);
    const auto scene = opentony::collision::PsxScene::parse(
        std::span<const std::byte>(
            archive.bytes().data(), archive.bytes().size()));
    assert(scene.has_value());

    const opentony::runtime::PsxScenePositionCollisionProbe probe(
        *scene, {-4100096, -8822784, 11472896});
    const auto hit = probe.query({-4100096, 23945216, 11472896});
    assert(hit.has_value());
    assert(hit->object_index == 170);
    assert(hit->model_index == 171);
    assert(hit->hit_parameter_q14 == 61);
    assert(hit->position == (opentony::runtime::FixedPosition{
        -4100096, -8700784, 11472896}));
    assert(hit->normal == (opentony::runtime::FixedPosition{
        1, -3867, -1351}));
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::byte> bytes = synthetic_archive();
    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(bytes, "probe.psx");
    const opentony::assets::PsxCollisionWorld world =
        opentony::assets::PsxCollisionWorld::build(archive);
    const opentony::runtime::PsxPositionCollisionProbe probe(world, {0, 4096, 0});

    const auto hit = probe.hit({0, -4096, 0});
    assert(hit.has_value());
    assert(hit->object_index == 0);
    assert(hit->model_index == 0);
    assert(hit->model_face_index == 0);
    assert(hit->normal[1] == 4096);
    assert(hit->surface_flags == 0x1234);
    const auto runtime_hit = probe.query({0, -4096, 0});
    assert(runtime_hit.has_value());
    assert(!runtime_hit->surface_bit_6);
    assert(runtime_hit->surface_bit_7_clear);
    assert(runtime_hit->surface_bit_8_clear);
    assert(runtime_hit->raw_type_bits_9_12 == 9);
    assert(!runtime_hit->face_flag_80);
    assert(probe({0, -4096, 0}));

    const opentony::runtime::PositionCommitResult result =
        opentony::runtime::PositionCommitter::commit(
            {0, 4096, 0},
            {0, -4096, 0},
            probe);
    assert(result.position[1] == 4096);
    assert(result.collided);
    assert(!result.blocked);
    assert(result.probes == 4);

    const auto recovered_scene = opentony::collision::PsxScene::parse(
        std::span<const std::byte>(bytes.data(), bytes.size()));
    assert(recovered_scene.has_value());
    const opentony::runtime::PsxScenePositionCollisionProbe recovered_probe(
        *recovered_scene, {0, 4096, 0});
    const auto recovered_hit = recovered_probe.query({0, -4096, 0});
    assert(recovered_hit.has_value());
    assert(recovered_hit->object_index == 0);
    assert(recovered_hit->model_index == 0);
    assert(recovered_hit->model_face_index == 0);
    assert(recovered_hit->normal[1] == 4096);
    assert(recovered_hit->surface_flags == 0x1234);
    assert(recovered_hit->hit_parameter_q14 == 0x2000);
    assert(recovered_hit->position[1] == 0);

    if (argc > 1) {
        check_packaged_scene(argv[1]);
    }

    std::cout << "PSX collision probe tests passed\n";
}
