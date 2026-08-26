#include "pre_runtime.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
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

std::vector<std::byte> make_pre() {
    std::vector<std::byte> bytes;
    append_u32(bytes, 2);
    const std::array<std::byte, 3> first{std::byte{1}, std::byte{2}, std::byte{3}};
    const std::array<std::byte, 2> second{std::byte{0xa}, std::byte{0xb}};
    append_entry(bytes, "s2switch.bmp", first);
    append_entry(bytes, "DIR/TWO.BIN", second);
    return bytes;
}

} // namespace

int main() {
    opentony::assets::PreRuntimeManager manager =
        opentony::assets::PreRuntimeManager::create();
    const std::size_t slot = manager.load("PANEL.PRE", make_pre());
    assert(slot == 0);
    assert(manager.loaded_count() == 1);
    assert(manager.allocation_size() == 0x144);
    assert(manager.container_name(slot) == "PANEL.PRE");
    assert(manager.container(slot).entries().size() == 2);

    const auto image = manager.find_embedded("S2SWITCH.BMP");
    assert(image.has_value());
    assert(image->container_slot == 0);
    assert(image->container_name == "PANEL.PRE");
    assert(image->resource_name == "s2switch.bmp");
    assert(image->payload.size() == 3);
    assert(std::to_integer<std::uint8_t>(image->payload[2]) == 3);

    const auto nested = manager.find_embedded("dir/two.bin");
    assert(nested.has_value());
    assert(nested->payload.size() == 2);
    assert(std::to_integer<std::uint8_t>(nested->payload[1]) == 0xb);
    assert(!manager.find_embedded("missing.fnt").has_value());

    manager.unload("panel.pre");
    assert(manager.loaded_count() == 0);
    assert(!manager.find_embedded("s2switch.bmp").has_value());

    const std::filesystem::path temp_path =
        std::filesystem::temp_directory_path() / "opentony_pre_runtime_test.pre";
    const std::vector<std::byte> disk_bytes = make_pre();
    {
        std::ofstream output(temp_path, std::ios::binary);
        assert(output);
        output.write(
            reinterpret_cast<const char*>(disk_bytes.data()),
            static_cast<std::streamsize>(disk_bytes.size()));
    }
    manager.load_file("PANEL.PRE", temp_path.string());
    assert(manager.find_embedded("s2switch.bmp").has_value());
    manager.unload("panel.pre");
    std::filesystem::remove(temp_path);

    bool rejected = false;
    try {
        manager.load("1234567890123456", make_pre());
    } catch (const opentony::assets::PreFormatError&) {
        rejected = true;
    }
    assert(rejected);

    for (unsigned index = 0; index < opentony::assets::kRuntimePreSlotCount; ++index) {
        manager.load("P" + std::to_string(index), make_pre());
    }
    assert(manager.loaded_count() == opentony::assets::kRuntimePreSlotCount);
    rejected = false;
    try {
        manager.load("overflow", make_pre());
    } catch (const opentony::assets::PreFormatError&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        manager.unload("not-loaded.pre");
    } catch (const opentony::assets::PreFormatError&) {
        rejected = true;
    }
    assert(rejected);
    return 0;
}
