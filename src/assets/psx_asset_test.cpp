#include "psx_asset.hpp"

#include <cassert>
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
    // Version 4, one empty model, one fixed-point object and one model-name
    // entry. This exercises the exact table boundaries without depending on
    // a retail file in the build environment.
    std::vector<std::byte> bytes(12 + 36 + 4 + 4 + 28 + 4 + 4 + 16, std::byte{0});
    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    const std::size_t model_offset = 12 + 36 + 4 + 4;
    const std::size_t tag_offset = model_offset + 28;
    put32(bytes, 4, static_cast<std::uint32_t>(tag_offset));
    put32(bytes, 8, 1);

    put32(bytes, 12 + 4, 0x00001000U);
    put32(bytes, 12 + 8, 0x00002000U);
    put32(bytes, 12 + 12, 0xfffff000U);
    put16(bytes, 12 + 24, 0x1234);
    put16(bytes, 12 + 26, 0);
    put32(bytes, 12 + 32, 0xabcdef01U);

    put32(bytes, 12 + 36, 1);
    put32(bytes, 12 + 40, static_cast<std::uint32_t>(model_offset));
    put16(bytes, model_offset, 0);
    put16(bytes, model_offset + 2, 0);
    put16(bytes, model_offset + 4, 0);
    put16(bytes, model_offset + 6, 0);
    put32(bytes, model_offset + 8, 0);
    put32(bytes, model_offset + 24, 0);

    put32(bytes, tag_offset, 0xffffffffU);
    put32(bytes, tag_offset + 4, 0xdecafbadU);
    put32(bytes, tag_offset + 8, 0);
    put32(bytes, tag_offset + 12, 0);
    put32(bytes, tag_offset + 16, 0);
    put32(bytes, tag_offset + 20, 0);

    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(std::move(bytes), "synthetic.psx");
    assert(archive.version() == 4);
    assert(archive.objects().size() == 1);
    assert(archive.objects()[0].position[0] == 0x1000);
    assert(archive.objects()[0].model_index == 0);
    assert(archive.models().size() == 1);
    assert(archive.models()[0].size == 28);
    assert(archive.model_names().size() == 1);
    assert(archive.model_names()[0] == 0xdecafbadU);
    return 0;
}
