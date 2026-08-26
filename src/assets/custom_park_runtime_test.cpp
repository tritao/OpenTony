#include "custom_park_runtime.hpp"

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
    using namespace opentony::assets;
    constexpr std::size_t cell_count = 16 * 16;
    constexpr std::size_t items_offset = 0x0c + cell_count * 8;
    constexpr std::size_t trailing_offset = items_offset + 10 * 0x24;
    std::vector<std::byte> bytes(0xa00, std::byte{0});
    put32(bytes, 0, kCustomParkMagic);
    put32(bytes, 4, 0);
    put32(bytes, 8, 1);
    bytes[0x0c] = std::byte{7};
    bytes[0x0d] = std::byte{0xff};
    put16(bytes, 0x12, 1);
    bytes[items_offset + 0] = std::byte{2};
    bytes[items_offset + 1] = std::byte{10};
    bytes[items_offset + 2] = std::byte{4};
    bytes[items_offset + 3] = std::byte{4};
    bytes[items_offset + 4] = std::byte{14};
    bytes[items_offset + 5] = std::byte{14};
    put16(bytes, items_offset + 6, 0x0330);
    put16(bytes, items_offset + 8, 0x0178);
    bytes[items_offset + 10] = std::byte{6};
    const char name[] = "YOU'RE ALL OVER THE MALL";
    for (std::size_t index = 0; name[index] != '\0'; ++index) {
        bytes[items_offset + 11 + index] = static_cast<std::byte>(name[index]);
    }
    bytes[trailing_offset] = std::byte{0xa5};

    const auto archive = CustomParkArchive::parse(std::move(bytes), "park0.prk");
    auto runtime = CustomParkRuntimeRegion::build(archive);
    CHECK(runtime.source_archive() == &archive);
    CHECK(runtime.cells().size() == 256);
    CHECK(runtime.cells()[0].translated_reference(0) == 7);
    CHECK(runtime.cells()[0].translated_reference(1) == 0xffff);
    CHECK(runtime.items().size() == 10);
    CHECK(runtime.items()[0].item_byte() == 6);
    CHECK(runtime.items()[0].item_name() == name);

    const auto first_object = runtime.append_generated_piece({
        .grid_position = {2, 4, 14},
        .source_dimensions = {8, 9, 10},
        .source_height_cells = 3,
        .model_index = 0x23,
        .slot = 1,
    });
    const auto second_object = runtime.append_generated_piece({
        .grid_position = {10, 4, 14},
        .source_dimensions = {8, 9, 10},
        .source_height_cells = 3,
        .model_index = 0x24,
        .slot = 1,
    });
    CHECK(first_object == 0);
    const std::array<std::int32_t, 3> expected_position{2 << 12, 4 << 12, 14 << 12};
    CHECK(runtime.published_objects()[0].position() == expected_position);
    CHECK(runtime.published_objects()[0].model_index() == 0x23);
    CHECK(runtime.published_objects()[0].slot() == 1);
    CHECK(runtime.generated_pieces()[0][0x0b] == std::byte{3});

    runtime.append_gap_pair({
        .source_item = 0,
        .first_model_index = 0x23,
        .second_model_index = 0x24,
        .first_object = first_object,
        .second_object = second_object,
        .active = true,
    });
    CHECK(runtime.gap_members().size() == 2);
    CHECK(runtime.gap_members()[0].source_item() == 0);
    CHECK(runtime.gap_members()[0].partner_index() == 1);
    CHECK(runtime.gap_members()[1].partner_index() == 0);
    CHECK(runtime.gap_members()[0].published_object_index() == 0);
    CHECK(runtime.gap_members()[1].published_object_index() == 1);
    CHECK(runtime.gap_members()[0].active());
    return 0;
}
