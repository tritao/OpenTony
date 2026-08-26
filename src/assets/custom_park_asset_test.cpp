#include "custom_park_asset.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
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
    constexpr std::size_t cell_count = 16 * 16;
    constexpr std::size_t items_offset = 0x0c + cell_count * 8;
    constexpr std::size_t trailing_offset = items_offset + 10 * 0x24;
    std::vector<std::byte> bytes(0xa00, std::byte{0});
    put32(bytes, 0, opentony::assets::kCustomParkMagic);
    put32(bytes, 4, 0);
    put32(bytes, 8, 1);
    bytes[0x0c] = std::byte{7};
    bytes[0x0d] = std::byte{0xff};
    bytes[0x11] = std::byte{0x33};
    put16(bytes, 0x12, 0x0001);
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

    const auto park = opentony::assets::CustomParkArchive::parse(
        std::move(bytes), "park0.prk");
    CHECK(park.magic() == opentony::assets::kCustomParkMagic);
    CHECK(park.version() == 0);
    CHECK(park.map_variant() == 1);
    CHECK(park.dimensions().width == 16);
    CHECK(park.dimensions().depth == 16);
    CHECK(park.cells().size() == 256);
    CHECK(park.cells()[0].translated_references[0] == 7);
    CHECK(park.cells()[0].translated_references[1] == 0xffff);
    CHECK(park.cells()[0].unpacked_cell_values[0] == 3);
    CHECK(park.cells()[0].unpacked_cell_values[1] == 0);
    CHECK(park.cells()[0].unpacked_cell_values[4] == 1);
    CHECK(park.items().size() == 10);
    const std::array<std::uint8_t, 2> expected_axis_0{2, 10};
    const std::array<std::uint8_t, 2> expected_axis_1{4, 4};
    const std::array<std::uint8_t, 2> expected_axis_2{14, 14};
    CHECK(park.items()[0].endpoint_axis_index_0 == expected_axis_0);
    CHECK(park.items()[0].endpoint_axis_index_1 == expected_axis_1);
    CHECK(park.items()[0].endpoint_axis_index_2 == expected_axis_2);
    CHECK(park.items()[0].item_byte == 6);
    CHECK(park.items()[0].item_name == name);
    CHECK(park.serialized_content_size() == 0x9b4);
    CHECK(park.aligned_allocation_size() == 0xa00);

    const std::filesystem::path retail_park =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data/PARK0.PRK";
    if (std::filesystem::is_regular_file(retail_park)) {
        const auto retail = opentony::assets::CustomParkArchive::load(retail_park.string());
        CHECK(retail.magic() == opentony::assets::kCustomParkMagic);
        CHECK(retail.version() == 0);
        CHECK(retail.map_variant() == 1);
        CHECK(retail.dimensions().width == 16);
        CHECK(retail.dimensions().depth == 16);
        CHECK(retail.cells().size() == 256);
        CHECK(retail.items().size() == 10);
        CHECK(retail.items()[0].item_name == name);
        CHECK(retail.items()[0].item_byte == 6);
        CHECK(retail.aligned_allocation_size() == 0xa00);
    }
    return 0;
}
