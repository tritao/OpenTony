#include "fnt_asset.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

namespace {

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

std::vector<std::byte> make_font() {
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
    return bytes;
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        bytes.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
    }
}

void align4(std::vector<std::byte>& bytes) {
    while ((bytes.size() & 3U) != 0U) {
        bytes.push_back(std::byte{0});
    }
}

std::vector<std::byte> make_pre_with_font(const std::vector<std::byte>& font) {
    std::vector<std::byte> bytes;
    append_u32(bytes, 1);
    for (const char* cursor = "S2TRICKS.FNT"; *cursor != '\0'; ++cursor) {
        bytes.push_back(static_cast<std::byte>(*cursor));
    }
    bytes.push_back(std::byte{0});
    align4(bytes);
    append_u32(bytes, static_cast<std::uint32_t>(font.size()));
    bytes.insert(bytes.end(), font.begin(), font.end());
    align4(bytes);
    return bytes;
}

} // namespace

int main() {
    std::vector<std::byte> bytes = make_font();

    const opentony::assets::FntRuntimeFont font =
        opentony::assets::FntRuntimeFont::parse(
            std::move(bytes), "font.fnt", 0x1234);
    CHECK(font.glyph_count() == 2);
    CHECK(font.records()[0].words[0] == 5);
    CHECK(font.records()[0].words[3] == 19);
    CHECK(font.palette_bytes()[0] == std::byte{0x11});
    CHECK(font.packed_glyph_data().size() == 5);
    CHECK(font.packed_glyph_data()[4] == std::byte{0xe4});
    CHECK(font.entries().size() == 3);
    CHECK(font.entries()[0].source_byte_0 == 19);
    CHECK(font.entries()[0].source_byte_1 == 14);
    CHECK(font.entries()[0].source_byte_2 == 14);
    CHECK(!font.entries()[0].sentinel);
    CHECK(font.entries()[2].sentinel);
    CHECK(font.atlas_format_word() == 0x1234);
    CHECK(font.font_allocation_size() == 0x14c);
    CHECK(font.entry_table_allocation_size() == 3 * 8);
    CHECK(font.glyph_image_allocation_size() == 3 * 0x5c);

    opentony::assets::PreRuntimeManager pre;
    pre.load("PANEL.PRE", make_pre_with_font(make_font()));
    opentony::assets::FntRuntimeManager fonts;
    const std::size_t slot = fonts.load_embedded(
        pre, "s2tricks.fnt", "S2TRICKS.FNT", 0x1234);
    CHECK(slot == 0);
    CHECK(fonts.loaded_count() == 1);
    const auto* found = fonts.find("S2TRICKS.FNT");
    CHECK(found != nullptr);
    CHECK(found->glyph_count() == 2);
    const auto text = fonts.text_view("s2tricks.fnt");
    CHECK(text.glyph_count == 2);
    CHECK(text.atlas_format_word == 0x1234);
    CHECK(text.entries.size() == 3);
    CHECK(text.entries[0].source_byte_0 == 19);
    CHECK(text.entries[2].sentinel);
    CHECK(&fonts.font(slot) == found);
    CHECK(fonts.font_name(slot) == "s2tricks.fnt");
    pre.unload("PANEL.PRE");
    CHECK(fonts.find("S2TRICKS.FNT") == found);
    CHECK(fonts.text_view("s2tricks.fnt").entries.back().sentinel);
    fonts.unload("S2TRICKS.FNT");
    CHECK(fonts.loaded_count() == 0);

    const std::filesystem::path temp_path =
        std::filesystem::temp_directory_path() / "opentony_fnt_runtime_test.fnt";
    const std::vector<std::byte> disk_bytes = make_font();
    {
        std::ofstream output(temp_path, std::ios::binary);
        CHECK(output);
        output.write(
            reinterpret_cast<const char*>(disk_bytes.data()),
            static_cast<std::streamsize>(disk_bytes.size()));
    }
    const std::size_t direct_slot = fonts.load_file(
        "direct.fnt", temp_path.string(), 0x4321);
    CHECK(fonts.font(direct_slot).atlas_format_word() == 0x4321);
    fonts.unload("DIRECT.FNT");
    std::filesystem::remove(temp_path);

    bool rejected = false;
    opentony::assets::FntRuntimeManager capacity;
    for (unsigned index = 0; index < opentony::assets::kRuntimeFontSlotCount; ++index) {
        capacity.load("F" + std::to_string(index), make_font());
    }
    CHECK(capacity.loaded_count() == opentony::assets::kRuntimeFontSlotCount);
    rejected = false;
    try {
        capacity.load("overflow", make_font());
    } catch (const opentony::assets::FntFormatError&) {
        rejected = true;
    }
    CHECK(rejected);

    rejected = false;
    try {
        std::vector<std::byte> malformed = make_font();
        put32(malformed, 0, 0xffffffffU);
        (void)opentony::assets::FntRuntimeFont::parse(
            std::move(malformed), "malformed.fnt");
    } catch (const opentony::assets::FntFormatError&) {
        rejected = true;
    }
    CHECK(rejected);

    const std::filesystem::path retail_font =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data/S2TRICKS.FNT";
    if (std::filesystem::is_regular_file(retail_font)) {
        const auto retail = opentony::assets::FntRuntimeFont::load(retail_font.string());
        CHECK(retail.glyph_count() == 59);
        const std::array<std::uint32_t, 4> first_record{5, 14, 14, 19};
        CHECK(retail.records()[0].words == first_record);
        CHECK(retail.packed_glyph_data().size() == 9104 - 0x3d4);
        CHECK(retail.entries().size() == 60);
    }
    return 0;
}
