#include "bmp_asset.hpp"

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
    // Bottom-up 2x2 BGR24 with four bytes of row padding.
    std::vector<std::byte> bytes(14 + 40 + 16, std::byte{0});
    bytes[0] = std::byte{'B'};
    bytes[1] = std::byte{'M'};
    put32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
    put32(bytes, 10, 54);
    put32(bytes, 14, 40);
    put32(bytes, 18, 2);
    put32(bytes, 22, 2);
    put16(bytes, 26, 1);
    put16(bytes, 28, 24);
    put32(bytes, 34, 16);
    // Bottom row: blue, white. Top row: red, green.
    bytes[54] = std::byte{0xff};
    bytes[55] = std::byte{0};
    bytes[56] = std::byte{0};
    bytes[57] = std::byte{0xff};
    bytes[58] = std::byte{0xff};
    bytes[59] = std::byte{0xff};
    bytes[62] = std::byte{0};
    bytes[63] = std::byte{0};
    bytes[64] = std::byte{0xff};
    bytes[65] = std::byte{0};
    bytes[66] = std::byte{0xff};
    bytes[67] = std::byte{0};
    bytes[68] = std::byte{0};
    bytes[69] = std::byte{0};
    const auto asset = opentony::assets::BmpAsset::parse(std::move(bytes), "test.bmp");
    CHECK(asset.image().width == 2);
    CHECK(asset.image().height == 2);
    CHECK(!asset.image().top_down);
    CHECK(asset.image().rgb == std::vector<std::uint8_t>({
        0xff, 0, 0, 0, 0xff, 0,
        0, 0, 0xff, 0xff, 0xff, 0xff}));

    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/newtex";
    const struct Witness {
        const char* name;
        std::uint32_t width;
        std::uint32_t height;
    } witnesses[] = {
        {"032BBB26.BMP", 128, 128},
        {"559F8A4B.BMP", 256, 128},
        {"7F9ACEA9.BMP", 128, 64},
        {"E75D1EF6.BMP", 64, 32},
    };
    for (const auto& witness : witnesses) {
        const auto path = root / witness.name;
        if (std::filesystem::is_regular_file(path)) {
            const auto real = opentony::assets::BmpAsset::load(path.string());
            CHECK(real.image().width == witness.width);
            CHECK(real.image().height == witness.height);
            CHECK(real.image().rgb.size() ==
                static_cast<std::size_t>(witness.width) * witness.height * 3U);
        }
    }
    return 0;
}
