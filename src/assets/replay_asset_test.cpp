#include "replay_asset.hpp"

#include <array>
#include <cassert>
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

void put_bits(std::vector<std::byte>& bytes, std::size_t& bit, std::uint32_t value, unsigned count) {
    for (unsigned index = 0; index < count; ++index, ++bit) {
        if (((value >> index) & 1U) != 0) {
            bytes[bit / 8U] |= static_cast<std::byte>(1U << (bit % 8U));
        }
    }
}

} // namespace

int main() {
    std::vector<std::byte> bytes(0x200 + 64, std::byte{0});
    put32(bytes, 0x00, 12);
    put32(bytes, 0x04, 1);
    put32(bytes, 0x08, 9);
    put32(bytes, 0x0c, 22);
    put32(bytes, 0x2c, 1);
    bytes[0x2e] = std::byte{3};
    bytes[0x2f] = std::byte{4};
    put16(bytes, 0x32, 7);
    put16(bytes, 0x34, 3);
    std::size_t bit = 0x200U * 8U;
    put_bits(bytes, bit, 0xabcdeU, 20);
    put_bits(bytes, bit, 0xfffffU, 20);
    for (unsigned index = 2; index < 8; ++index) {
        put_bits(bytes, bit, index, 20);
    }
    put_bits(bytes, bit, 0xffffU, 16);
    for (unsigned index = 1; index < 8; ++index) {
        put_bits(bytes, bit, index, 16);
    }

    const auto replay = opentony::assets::ReplayAsset::parse(
        std::move(bytes), "demo.rec");
    assert(replay.header().num_frames == 12);
    assert(replay.header().highlights[0].start == 1);
    assert(replay.header().highlights[0].end == 9);
    assert(replay.header().num_skaters == 1);
    assert(replay.header().skater == 3);
    assert(replay.header().level == 7);
    assert(replay.header().game == 3);
    auto reader = replay.frame_reader();
    const auto frame = reader.read_frame();
    assert(frame.fixed_channels[0] == static_cast<std::int32_t>(0xabcdeU << 12));
    assert(frame.fixed_channels[1] == -4096);
    assert(frame.fixed_channels[2] == 2 << 12);
    assert(frame.narrow_channels[0] == -1);
    assert(frame.narrow_channels[7] == 7);
    assert(reader.bit_offset() == (0x200U * 8U + 8U * 20U + 8U * 16U));

    const std::filesystem::path retail_root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    const auto retail_path = retail_root / "DEMOA.REC";
    if (std::filesystem::is_regular_file(retail_path)) {
        const auto retail = opentony::assets::ReplayAsset::load(retail_path.string());
        assert(retail.bytes().size() == 24064);
        assert(retail.header().num_frames == 843);
        assert(retail.header().num_skaters == 1);
        assert(retail.header().skater == 12);
        assert(retail.header().level == 1);
        assert(retail.header().game == 3);
    }
    return 0;
}
