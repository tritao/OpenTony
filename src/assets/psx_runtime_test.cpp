#include "psx_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
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

} // namespace

int main() {
    constexpr std::size_t object_table = 12;
    constexpr std::size_t model_table = object_table + 36;
    constexpr std::size_t model_offset = model_table + 8;
    constexpr std::size_t tag_offset = model_offset + 28;
    std::vector<std::byte> bytes(tag_offset + 24, std::byte{0});

    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    put32(bytes, 4, static_cast<std::uint32_t>(tag_offset));
    put32(bytes, 8, 1);
    put32(bytes, object_table + 0x00, 0x12345678U);
    put32(bytes, object_table + 0x04, 0x01020304U);
    put32(bytes, object_table + 0x08, 0xf0001000U);
    put32(bytes, object_table + 0x0c, 0x55667788U);
    put32(bytes, object_table + 0x10, 0x11223344U);
    put16(bytes, object_table + 0x14, 0x1357);
    put16(bytes, object_table + 0x16, 0);
    put16(bytes, object_table + 0x18, 0xfffe);
    put16(bytes, object_table + 0x1a, 0x0003);
    put32(bytes, object_table + 0x1c, 0xdecafbadU);
    put32(bytes, object_table + 0x20, 0xaabbccddU);

    put32(bytes, model_table, 1);
    put32(bytes, model_table + 4, static_cast<std::uint32_t>(model_offset));
    put16(bytes, model_offset + 0, 0x0008);
    put16(bytes, model_offset + 2, 0);
    put16(bytes, model_offset + 4, 0);
    put16(bytes, model_offset + 6, 0);
    put32(bytes, model_offset + 8, 0);
    put32(bytes, model_offset + 24, 0);
    put32(bytes, tag_offset, 0xffffffffU);
    put32(bytes, tag_offset + 4, 0xcafebabeU);

    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(std::move(bytes), "runtime.psx");
    const opentony::assets::PsxRuntimeEnvironment runtime =
        opentony::assets::PsxRuntimeEnvironment::build(archive, 6);
    CHECK(runtime.slot() == 6);
    CHECK(runtime.object_count() == 1);
    CHECK(runtime.model_count() == 1);
    CHECK(runtime.allocation_size() ==
        4 + opentony::assets::kPsxRuntimeObjectStride);
    const std::array<std::int32_t, 3> expected_position{
        0x01020304, static_cast<std::int32_t>(0xf0001000U), 0x55667788};
    CHECK(runtime.object_record_offset(0) == 4);
    CHECK(runtime.object(0).flags() == 0x12345678U);
    CHECK(runtime.object(0).position() == expected_position);
    CHECK(runtime.object(0).model_index() == 0);
    CHECK(runtime.object(0).source_word_at_14() == 0x11223344U);
    CHECK(runtime.object(0).source_transform_component() == 0x1357);
    CHECK(runtime.object(0).source_transform_tail() == 0x0003fffeU);
    CHECK(runtime.object(0).source_rgbx() == 0xaabbccddU);
    const std::array<std::uint16_t, 3> expected_scale{0x1000, 0x1000, 0x1000};
    CHECK(runtime.object(0).runtime_scale_q12() == expected_scale);
    CHECK(runtime.model_pointer(0) == &archive.models()[0]);
    CHECK(&runtime.model_for_object(0) == &archive.models()[0]);

    // Two textured faces share one checksum even though they address
    // different source texture-name entries. The finalizer's hash table
    // should therefore expose one 0x2c-byte material record with two users.
    constexpr std::size_t textured_object_table = 12;
    constexpr std::size_t textured_model_table = textured_object_table + 36;
    constexpr std::size_t textured_model_offset = textured_model_table + 8;
    constexpr std::size_t textured_model_size = 28 + 3 * 8 + 8 + 2 * 20;
    constexpr std::size_t textured_tag_offset =
        textured_model_offset + textured_model_size;
    std::vector<std::byte> textured_bytes(textured_tag_offset + 32, std::byte{0});
    put16(textured_bytes, 0, 4);
    put16(textured_bytes, 2, 2);
    put32(textured_bytes, 4, static_cast<std::uint32_t>(textured_tag_offset));
    put32(textured_bytes, 8, 1);
    put32(textured_bytes, textured_model_table, 1);
    put32(textured_bytes, textured_model_table + 4,
        static_cast<std::uint32_t>(textured_model_offset));
    put16(textured_bytes, textured_model_offset + 0, 0);
    put16(textured_bytes, textured_model_offset + 2, 3);
    put16(textured_bytes, textured_model_offset + 4, 1);
    put16(textured_bytes, textured_model_offset + 6, 2);
    put32(textured_bytes, textured_model_offset + 8, 0);
    put32(textured_bytes, textured_model_offset + 24, 0);
    const std::size_t textured_vertices = textured_model_offset + 28;
    for (std::size_t vertex = 0; vertex < 3; ++vertex) {
        put16(textured_bytes, textured_vertices + vertex * 8, static_cast<std::uint16_t>(vertex));
    }
    const std::size_t textured_normal = textured_vertices + 3 * 8;
    put16(textured_bytes, textured_normal, 0);
    const std::size_t textured_faces = textured_normal + 8;
    for (std::size_t face = 0; face < 2; ++face) {
        const std::size_t offset = textured_faces + face * 20;
        put16(textured_bytes, offset, 2);
        put16(textured_bytes, offset + 2, 20);
        textured_bytes[offset + 4] = std::byte{0};
        textured_bytes[offset + 5] = std::byte{1};
        textured_bytes[offset + 6] = std::byte{2};
        textured_bytes[offset + 7] = std::byte{0};
        put16(textured_bytes, offset + 12, 0);
        put16(textured_bytes, offset + 14, 0);
        put32(textured_bytes, offset + 16, static_cast<std::uint32_t>(face));
    }
    put32(textured_bytes, textured_tag_offset, 0xffffffffU);
    put32(textured_bytes, textured_tag_offset + 4, 0x12345678U);
    put32(textured_bytes, textured_tag_offset + 8, 2);
    put32(textured_bytes, textured_tag_offset + 12, 0xabcdef01U);
    put32(textured_bytes, textured_tag_offset + 16, 0xabcdef01U);
    put32(textured_bytes, textured_tag_offset + 20, 0);
    put32(textured_bytes, textured_tag_offset + 24, 0);
    put32(textured_bytes, textured_tag_offset + 28, 0);
    const opentony::assets::PsxArchive textured_archive =
        opentony::assets::PsxArchive::parse(
            std::move(textured_bytes), "textured-runtime.psx");
    const opentony::assets::PsxRuntimeEnvironment textured_runtime =
        opentony::assets::PsxRuntimeEnvironment::build(textured_archive);
    CHECK(textured_runtime.materials().records().size() == 1);
    CHECK(textured_runtime.materials().material_index_for_texture(0) == 0);
    CHECK(textured_runtime.materials().material_index_for_texture(1) == 0);
    CHECK(textured_runtime.materials().texture_index_for_material(0) == 0);
    const auto& textured_material = textured_runtime.materials().record(0);
    CHECK(textured_material.checksum() == 0xabcdef01U);
    CHECK(textured_material.reference_count() == 2);
    CHECK(textured_material.raw_record().size() == 0x2c);
    return 0;
}
