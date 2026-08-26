#include "psx_bits_runtime.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
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

void name8(std::vector<std::byte>& bytes, std::size_t offset, const char* value) {
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[offset + index] = value[index] == '\0'
            ? std::byte{0}
            : static_cast<std::byte>(value[index]);
    }
}

} // namespace

int main() {
    constexpr std::size_t tag_offset = 0x10;
    constexpr std::size_t payload_offset = tag_offset + 8;
    constexpr std::size_t payload_size = 4 + (8 + 4 + 8) + (8 + 4 + 16);
    std::vector<std::byte> bytes(payload_offset + payload_size + 4, std::byte{0});
    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    put32(bytes, 4, static_cast<std::uint32_t>(tag_offset));
    put32(bytes, tag_offset, 0x45);
    put32(bytes, tag_offset + 4, static_cast<std::uint32_t>(payload_size));
    put32(bytes, payload_offset, 2);

    std::size_t cursor = payload_offset + 4;
    name8(bytes, cursor, "Shadow");
    cursor += 8;
    put32(bytes, cursor, 1);
    cursor += 4;
    for (std::size_t index = 0; index < 8; ++index) {
        bytes[cursor + index] = static_cast<std::byte>(index + 1);
    }
    cursor += 8;
    name8(bytes, cursor, "SMOKE");
    cursor += 8;
    put32(bytes, cursor, 2);
    cursor += 4;
    for (std::size_t index = 0; index < 16; ++index) {
        bytes[cursor + index] = static_cast<std::byte>(0xa0 + index);
    }
    cursor += 16;
    put32(bytes, cursor, 0xffffffffU);

    const opentony::assets::PsxArchive archive =
        opentony::assets::PsxArchive::parse(std::move(bytes), "bits.psx");
    opentony::assets::PsxBitsRuntime runtime;
    runtime.build(archive);
    assert(runtime.groups().size() == 2);
    assert(runtime.groups()[0].name == "Shadow");
    assert(runtime.groups()[0].entries.size() == 1);
    assert(runtime.groups()[0].entries[0][0] == std::byte{1});
    assert(runtime.groups()[1].name == "SMOKE");
    assert(runtime.groups()[1].entries.size() == 2);
    assert(runtime.find("shadow") == &runtime.groups()[0]);
    assert(runtime.find("SmOkE") == &runtime.groups()[1]);
    assert(runtime.find("font") == nullptr);
    return 0;
}
