#include "pre_asset.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

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

void append_entry(
    std::vector<std::byte>& bytes,
    const char* name,
    std::span<const std::byte> payload) {
    for (const char* cursor = name; *cursor != '\0'; ++cursor) {
        bytes.push_back(static_cast<std::byte>(*cursor));
    }
    bytes.push_back(std::byte{0});
    align4(bytes);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    align4(bytes);
}

} // namespace

int main() {
    std::vector<std::byte> bytes;
    append_u32(bytes, 2);
    const std::array<std::byte, 3> first{std::byte{1}, std::byte{2}, std::byte{3}};
    const std::array<std::byte, 2> second{std::byte{0xa}, std::byte{0xb}};
    append_entry(bytes, "ONE.BIN", first);
    append_entry(bytes, "DIR/TWO.BIN", second);

    const opentony::assets::PreArchive archive =
        opentony::assets::PreArchive::parse(std::move(bytes), "synthetic.pre");
    assert(archive.entries().size() == 2);
    assert(archive.entry(0).name == "ONE.BIN");
    assert(archive.payload("ONE.BIN").size() == 3);
    assert(archive.find("DIR/TWO.BIN") != nullptr);
    assert(std::to_integer<std::uint8_t>(archive.payload(1)[1]) == 0xb);
    std::cout << "PRE asset tests passed\n";
}
