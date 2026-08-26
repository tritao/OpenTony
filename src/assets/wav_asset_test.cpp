#include "wav_asset.hpp"

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

void fourcc(std::vector<std::byte>& bytes, std::size_t offset, const char* value) {
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::byte>(value[index]);
    }
}

} // namespace

int main() {
    // RIFF/WAVE with an unknown chunk before the observed PCM fmt/data pair.
    constexpr std::size_t fmt_offset = 20;
    constexpr std::size_t data_header = fmt_offset + 8 + 16;
    constexpr std::size_t data_offset = data_header + 8;
    constexpr std::size_t file_size = data_offset + 5;
    std::vector<std::byte> bytes(file_size, std::byte{0});
    fourcc(bytes, 0, "RIFF");
    put32(bytes, 4, static_cast<std::uint32_t>(file_size - 8));
    fourcc(bytes, 8, "WAVE");
    fourcc(bytes, 12, "JUNK");
    put32(bytes, 16, 0);
    fourcc(bytes, fmt_offset, "fmt ");
    put32(bytes, fmt_offset + 4, 16);
    put16(bytes, fmt_offset + 8, 1);
    put16(bytes, fmt_offset + 10, 1);
    put32(bytes, fmt_offset + 12, 44100);
    put32(bytes, fmt_offset + 16, 88200);
    put16(bytes, fmt_offset + 20, 2);
    put16(bytes, fmt_offset + 22, 16);
    fourcc(bytes, data_header, "data");
    put32(bytes, data_header + 4, 5);
    for (std::size_t index = 0; index < 5; ++index) {
        bytes[data_offset + index] = static_cast<std::byte>(0x30 + index);
    }

    const opentony::assets::WavPcmAsset asset =
        opentony::assets::WavPcmAsset::parse(std::move(bytes), "sample.wav");
    CHECK(asset.format().format_tag == 1);
    CHECK(asset.format().channels == 1);
    CHECK(asset.format().samples_per_second == 44100);
    CHECK(asset.format().bits_per_sample == 16);
    CHECK(asset.sample_size() == 5);
    CHECK(asset.allocated_sample_size() == 8);
    CHECK(asset.samples()[4] == std::byte{0x34});
    CHECK(asset.samples()[5] == std::byte{0});

    bool rejected = false;
    try {
        (void)opentony::assets::WavPcmAsset::parse(
            {std::byte{'N'}, std::byte{'O'}, std::byte{'P'}}, "bad.wav");
    } catch (const opentony::assets::WavFormatError&) {
        rejected = true;
    }
    CHECK(rejected);

    const std::filesystem::path retail_wav =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data/audio/BULLROAR2.WAV";
    if (std::filesystem::is_regular_file(retail_wav)) {
        const auto retail = opentony::assets::WavPcmAsset::load(retail_wav.string());
        CHECK(retail.format().format_tag == 1);
        CHECK(retail.format().channels == 1);
        CHECK(retail.format().samples_per_second == 44100);
        CHECK(retail.format().bits_per_sample == 16);
        CHECK(retail.sample_size() == 100664);
        CHECK(retail.allocated_sample_size() == 100664);
    }
    return 0;
}
