#include "psx_animation.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <utility>
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

opentony::assets::PsxArchive synthetic_archive() {
    constexpr std::size_t object_offset = 12;
    constexpr std::size_t model_count_offset = object_offset + 36;
    constexpr std::size_t model_offset_table = model_count_offset + 4;
    constexpr std::size_t model_offset = model_offset_table + 4;
    constexpr std::size_t model_size = 28;
    constexpr std::size_t tag_offset = model_offset + model_size;
    constexpr std::size_t animation_payload_offset = tag_offset + 8;
    constexpr std::size_t animation_payload_size = 17;
    constexpr std::size_t hierarchy_tag =
        animation_payload_offset + animation_payload_size;
    constexpr std::size_t hierarchy_payload_offset = hierarchy_tag + 8;
    constexpr std::size_t terminator = hierarchy_payload_offset + 2;
    constexpr std::size_t post_tags = terminator + 4;
    std::vector<std::byte> bytes(post_tags + 20, std::byte{0});
    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    put32(bytes, 4, static_cast<std::uint32_t>(tag_offset));
    put32(bytes, 8, 1);
    put32(bytes, object_offset + 4, 0x1000);
    put32(bytes, model_count_offset, 1);
    put32(bytes, model_offset_table, static_cast<std::uint32_t>(model_offset));
    put16(bytes, model_offset, 0);
    put16(bytes, model_offset + 2, 0);
    put16(bytes, model_offset + 4, 0);
    put16(bytes, model_offset + 6, 0);
    put32(bytes, tag_offset, 0x2c);
    put32(bytes, tag_offset + 4, animation_payload_size);
    put32(bytes, animation_payload_offset, 1);
    put32(bytes, animation_payload_offset + 4, 12);
    put32(bytes, animation_payload_offset + 8, 5);
    bytes[animation_payload_offset + 16] = static_cast<std::byte>(0xaa);
    put32(bytes, hierarchy_tag, 0x52454948U);
    put32(bytes, hierarchy_tag + 4, 2);
    put16(bytes, hierarchy_payload_offset, 0);
    put32(bytes, terminator, 0xffffffffU);
    put32(bytes, post_tags, 0x12345678U);
    put32(bytes, post_tags + 4, 0);
    put32(bytes, post_tags + 8, 0);
    put32(bytes, post_tags + 12, 0);
    put32(bytes, post_tags + 16, 0);
    return opentony::assets::PsxArchive::parse(
        std::move(bytes),
        "synthetic-animation.psx");
}

void test_synthetic_packet() {
    const auto table = opentony::assets::PsxAnimationTable::parse(
        synthetic_archive());
    assert(table.animation_count() == 1);
    assert(table.part_count() == 1);
    assert(table.record(0).relative_data_offset == 12);
    assert(table.record(0).frame_count() == 5);
    assert(table.record(0).flags() == 0);
    assert(table.stream(0).size() == 5);
    assert(std::to_integer<std::uint8_t>(table.stream(0).back()) == 0xaa);
    assert(table.hierarchy_words().size() == 1);
}

void test_retail_packet_when_available() {
    const std::filesystem::path path =
        std::filesystem::path(OPENTONY_SOURCE_DIR)
        / "build/assets/all-pkr/files/data/SK2ANIM.PSX";
    if (!std::filesystem::is_regular_file(path)) {
        return;
    }
    const auto table = opentony::assets::PsxAnimationTable::load(path.string());
    assert(table.animation_count() == 218);
    assert(table.part_count() == 19);
    assert(table.payload_size() == 0x8f1b0);
    assert(table.record(0).relative_data_offset == 0x6d4);
    assert(table.record(0).frame_count() == 12);
    assert(table.record(1).frame_count() == 10);
    assert(table.record(6).frame_count() == 23);
    assert(table.record(10).frame_count() == 28);
    assert(table.record(217).frame_count() == 7);
    assert(table.hierarchy_words().size() == 20);
    for (std::uint16_t id = 0; id < table.animation_count(); ++id) {
        assert(table.record(id).frame_count() >= 5);
        assert(table.record(id).frame_count() <= 97);
        assert(table.record(id).flags() == 0);
        assert(!table.stream(id).empty());
    }
}

} // namespace

int main() {
    test_synthetic_packet();
    test_retail_packet_when_available();
    std::cout << "PSX animation table tests passed\n";
}
