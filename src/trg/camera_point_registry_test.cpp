#include "camera_point_registry.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
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

void write_point(std::vector<std::byte>& bytes, std::size_t offset,
    std::int32_t x, std::int32_t y, std::int32_t z,
    std::uint16_t mode, std::uint16_t variant) {
    put16(bytes, offset, 13);
    put16(bytes, offset + 2, 0);
    put32(bytes, offset + 4, static_cast<std::uint32_t>(x));
    put32(bytes, offset + 8, static_cast<std::uint32_t>(y));
    put32(bytes, offset + 12, static_cast<std::uint32_t>(z));
    put16(bytes, offset + 16, mode);
    put16(bytes, offset + 18, variant);
}

} // namespace

int main() {
    // Header/table: two 20-byte type-13 nodes and a two-byte terminator.
    std::vector<std::byte> bytes(70, std::byte{0});
    put32(bytes, 0, 0x4752545fU);
    put32(bytes, 4, 2);
    put32(bytes, 8, 3);
    put32(bytes, 12, 28);
    put32(bytes, 16, 48);
    put32(bytes, 20, 68);
    write_point(bytes, 28, 100, 200, 300, 1, 7);
    write_point(bytes, 48, -100, -200, -300, 2, 9);
    put16(bytes, 68, 0xff);

    const opentony::trg::TrgFile file =
        opentony::trg::TrgFile::parse(bytes);
    const auto first = file.camera_point(0);
    const std::array<std::int32_t, 3> expected_position{100 << 12, 200 << 12, 300 << 12};
    assert(first.position == expected_position);
    assert(first.selection_state_word == 1);
    assert(first.transition_variant == 7);

    opentony::trg::CameraPointRegistry registry;
    registry.build(file);
    assert(registry.entries().size() == 2);
    const auto selected = registry.select_nearest(
        [](const opentony::trg::CameraPointEntry& entry)
            -> std::optional<std::int32_t> {
            return entry.source_node == 0 ? 400 : 100;
        });
    assert(selected.has_value());
    assert(selected->source_node == 1);
    assert(selected->camera_mode == 2);
    assert(selected->camera_state_bit == 0x800U);
    assert(selected->transition_variant == 9);
    assert(selected->distance == 100);

    const auto strict_boundary = registry.select_nearest(
        [](const opentony::trg::CameraPointEntry&)
            -> std::optional<std::int32_t> {
            return opentony::trg::CameraPointRegistry::kRetailDistanceLimit;
        });
    assert(!strict_boundary.has_value());
    return 0;
}
