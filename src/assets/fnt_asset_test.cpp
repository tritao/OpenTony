#include "fnt_asset.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace {

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

} // namespace

int main() {
    constexpr std::size_t count = 2;
    constexpr std::size_t palette_offset = 4 + count * 0x10;
    constexpr std::size_t glyph_data_offset = palette_offset + 0x20;
    std::vector<std::byte> bytes(glyph_data_offset + 5, std::byte{0});
    put32(bytes, 0, count);
    put32(bytes, 4, 5);
    put32(bytes, 8, 14);
    put32(bytes, 12, 14);
    put32(bytes, 16, 19);
    put32(bytes, 20, 6);
    put32(bytes, 24, 15);
    put32(bytes, 28, 16);
    put32(bytes, 32, 20);
    bytes[palette_offset + 0] = std::byte{0x11};
    for (std::size_t index = 0; index < 5; ++index) {
        bytes[glyph_data_offset + index] = static_cast<std::byte>(0xe0 + index);
    }

    const opentony::assets::FntRuntimeFont font =
        opentony::assets::FntRuntimeFont::parse(
            std::move(bytes), "font.fnt", 0x1234);
    assert(font.glyph_count() == 2);
    assert(font.records()[0].words[0] == 5);
    assert(font.records()[0].words[3] == 19);
    assert(font.palette_bytes()[0] == std::byte{0x11});
    assert(font.packed_glyph_data().size() == 5);
    assert(font.packed_glyph_data()[4] == std::byte{0xe4});
    assert(font.entries().size() == 3);
    assert(font.entries()[0].source_byte_0 == 19);
    assert(font.entries()[0].source_byte_1 == 14);
    assert(font.entries()[0].source_byte_2 == 14);
    assert(!font.entries()[0].sentinel);
    assert(font.entries()[2].sentinel);
    assert(font.atlas_format_word() == 0x1234);
    assert(font.font_allocation_size() == 0x14c);
    assert(font.entry_table_allocation_size() == 3 * 8);
    assert(font.glyph_image_allocation_size() == 3 * 0x5c);

    const std::filesystem::path retail_font =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data/S2TRICKS.FNT";
    if (std::filesystem::is_regular_file(retail_font)) {
        const auto retail = opentony::assets::FntRuntimeFont::load(retail_font.string());
        assert(retail.glyph_count() == 59);
        const std::array<std::uint32_t, 4> first_record{5, 14, 14, 19};
        assert(retail.records()[0].words == first_record);
        assert(retail.packed_glyph_data().size() == 9104 - 0x3d4);
        assert(retail.entries().size() == 60);
    }
    return 0;
}
