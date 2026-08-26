#include "save_asset.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

int main() {
    std::array<std::array<std::byte, opentony::assets::kSaveHeaderBlockSize>,
        opentony::assets::kSaveHeaderBlockCount> blocks{};
    blocks[0][0] = std::byte{0x12};
    blocks[1][1] = std::byte{0x34};
    blocks[2][2] = std::byte{0x56};
    std::vector<std::byte> career(opentony::assets::kCareerRegisteredSize, std::byte{0});
    career[0] = std::byte{0x00};
    career[1] = std::byte{0x15};
    career[2] = std::byte{0x10};
    career[3] = std::byte{0x00};
    career[0x1870] = std::byte{4};
    career[0x1871] = std::byte{5};
    career[0x1872] = std::byte{2};
    career[0x1873] = std::byte{3};
    const std::size_t record = opentony::assets::kCareerRecordOffset;
    career[record + 0x34] = std::byte{0x78};
    career[record + 0x35] = std::byte{0x56};
    career[record + 0x36] = std::byte{0x34};
    career[record + 0x37] = std::byte{0x12};
    career[record + 0xcc] = std::byte{0xa1};

    const auto save = opentony::assets::SaveGameFile::build(
        opentony::assets::SaveActionType::career,
        "TEST",
        blocks,
        career,
        "career.sav");
    assert(save.bytes().size() == 0x2000);
    assert(save.card_block_count() == 1);
    assert(save.header().action_type == opentony::assets::SaveActionType::career);
    assert(std::string(save.header().name.data(), 4) == "TEST");
    assert(save.header().blocks[0][0] == std::byte{0x12});
    assert(save.payload().size() == opentony::assets::kCareerRegisteredSize);

    const auto image = opentony::assets::CareerSaveImage::parse(
        save.payload(), "career.sav payload");
    assert(image.format_magic() == opentony::assets::kCareerFormatMagic);
    const std::array<std::uint8_t, 2> expected_skaters{4, 5};
    const std::array<std::uint8_t, 2> expected_costumes{2, 3};
    assert(image.active_skater_selectors() == expected_skaters);
    assert(image.active_costume_selectors() == expected_costumes);
    assert(image.skater_records().size() == 20);
    assert(image.skater_records()[0].trick_unlock_bits() == 0x12345678);
    assert(image.skater_records()[0].custom_appearance()[0] == std::byte{0xa1});
    assert(image.image_bytes().size() == opentony::assets::kCareerImageSize);

    const auto reparsed = opentony::assets::SaveGameFile::parse(
        save.bytes(), "career.sav");
    assert(reparsed.payload().size() == career.size());
    assert(reparsed.payload()[0x1870] == std::byte{4});
    return 0;
}
