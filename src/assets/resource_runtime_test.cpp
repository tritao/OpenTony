#include "fnt_asset.hpp"
#include "pkr_asset.hpp"
#include "pre_runtime.hpp"
#include "resource_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::byte>(value >> 24U);
}

void name32(std::vector<std::byte>& bytes, std::size_t offset, const char* value) {
    for (std::size_t index = 0; value[index] != '\0' && index < 32; ++index) {
        bytes[offset + index] = static_cast<std::byte>(value[index]);
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4);
    put32(bytes, offset, value);
}

void align4(std::vector<std::byte>& bytes) {
    while ((bytes.size() & 3U) != 0U) {
        bytes.push_back(std::byte{0});
    }
}

std::vector<std::byte> make_font() {
    constexpr std::size_t count = 1;
    constexpr std::size_t palette_offset = 4 + count * 0x10;
    constexpr std::size_t glyph_data_offset = palette_offset + 0x20;
    std::vector<std::byte> bytes(glyph_data_offset + 3, std::byte{0});
    put32(bytes, 0, count);
    put32(bytes, 4, 5);
    put32(bytes, 8, 14);
    put32(bytes, 12, 14);
    put32(bytes, 16, 17);
    bytes[glyph_data_offset + 0] = std::byte{0x61};
    bytes[glyph_data_offset + 1] = std::byte{0x62};
    bytes[glyph_data_offset + 2] = std::byte{0x63};
    return bytes;
}

std::vector<std::byte> make_pre(const std::vector<std::byte>& payload) {
    std::vector<std::byte> bytes;
    append_u32(bytes, 1);
    for (const char* cursor = "FONT.FNT"; *cursor != '\0'; ++cursor) {
        bytes.push_back(static_cast<std::byte>(*cursor));
    }
    bytes.push_back(std::byte{0});
    align4(bytes);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    align4(bytes);
    return bytes;
}

std::vector<std::byte> make_package(const std::vector<std::byte>& payload) {
    constexpr std::size_t directory_table = 16;
    constexpr std::size_t file_table = directory_table + 40;
    std::vector<std::byte> bytes(file_table + 48, std::byte{0});
    bytes[0] = std::byte{'P'};
    bytes[1] = std::byte{'K'};
    bytes[2] = std::byte{'R'};
    bytes[3] = std::byte{'2'};
    put32(bytes, 4, 1);
    put32(bytes, 8, 1);
    put32(bytes, 12, 1);
    name32(bytes, directory_table, "data");
    put32(bytes, directory_table + 32, static_cast<std::uint32_t>(file_table));
    put32(bytes, directory_table + 36, 1);
    name32(bytes, file_table, "FONT.FNT");
    put32(bytes, file_table + 32, opentony::assets::kPkrRawMarker);
    const std::size_t payload_offset = bytes.size();
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    put32(bytes, file_table + 36, static_cast<std::uint32_t>(payload_offset));
    put32(bytes, file_table + 40, static_cast<std::uint32_t>(payload.size()));
    put32(bytes, file_table + 44, static_cast<std::uint32_t>(payload.size()));
    return bytes;
}

} // namespace

int main() {
    const std::vector<std::byte> font_bytes = make_font();
    const opentony::assets::PkrArchive package =
        opentony::assets::PkrArchive::parse(
            make_package(font_bytes), "runtime.pkr");
    opentony::assets::ResourceBackend package_backend(package);

    auto stream = opentony::assets::ResourceStream::open(
        package_backend, "data/FONT.FNT");
    CHECK(stream.size() == font_bytes.size());
    std::array<std::byte, 4> first_words{};
    CHECK(stream.read_elements(1, first_words.size(), first_words) == 4);
    CHECK(first_words[0] == std::byte{1});
    CHECK(stream.position() == 4);
    stream.seek(-2, opentony::assets::ResourceSeekOrigin::End);
    std::array<std::byte, 2> tail{};
    CHECK(stream.read(tail) == 2);
    CHECK(tail[0] == std::byte{0x62});
    CHECK(tail[1] == std::byte{0x63});
    const opentony::assets::ResourceHandle stream_handle = 0;
    CHECK(package_backend.is_open(stream_handle));
    stream.close();
    CHECK(!package_backend.is_open(stream_handle));

    opentony::assets::ResourceLoader package_loader(package_backend);
    const std::vector<std::byte> package_loaded =
        package_loader.load_owned("data/FONT.FNT");
    CHECK(!package_loader.active());
    CHECK(package_loaded == font_bytes);
    const auto package_font = opentony::assets::FntRuntimeFont::parse(
        package_loaded, "runtime.pkr/data/FONT.FNT");
    CHECK(package_font.glyph_count() == 1);
    CHECK(package_font.entries().back().sentinel);

    opentony::assets::PreRuntimeManager pre;
    pre.load("PANEL.PRE", make_pre(font_bytes));
    opentony::assets::ResourceLoader pre_loader(package_backend, &pre);
    CHECK(pre_loader.open("FONT.FNT") == font_bytes.size());
    CHECK(pre_loader.source_kind()
        == opentony::assets::ResourceSourceKind::PreEmbedded);
    std::vector<std::byte> pre_loaded(font_bytes.size());
    pre_loader.load(pre_loaded);
    CHECK(pre_loader.load_started());
    pre_loader.synchronize();
    pre.unload("panel.pre");
    CHECK(pre_loaded == font_bytes);
    const auto pre_font = opentony::assets::FntRuntimeFont::parse(
        std::move(pre_loaded), "PANEL.PRE/FONT.FNT");
    CHECK(pre_font.glyph_count() == 1);

    const std::filesystem::path direct_path =
        std::filesystem::temp_directory_path() / "opentony_resource_runtime.bin";
    {
        std::ofstream output(direct_path, std::ios::binary);
        CHECK(output);
        output.write("direct", 6);
    }
    opentony::assets::ResourceBackend direct_backend;
    opentony::assets::ResourceLoader direct_loader(direct_backend, &pre);
    CHECK(direct_loader.open(direct_path.string()) == 6);
    CHECK(direct_loader.source_kind()
        == opentony::assets::ResourceSourceKind::DirectFile);
    std::array<std::byte, 6> direct_bytes{};
    direct_loader.load(direct_bytes);
    direct_loader.synchronize();
    CHECK(std::string(reinterpret_cast<const char*>(direct_bytes.data()), 6)
        == "direct");
    std::filesystem::remove(direct_path);

    bool rejected = false;
    try {
        auto bad = opentony::assets::ResourceStream::open(
            package_backend, "data/FONT.FNT");
        bad.seek(1, opentony::assets::ResourceSeekOrigin::End);
    } catch (const opentony::assets::ResourceRuntimeError&) {
        rejected = true;
    }
    CHECK(rejected);
    rejected = false;
    try {
        auto bad = opentony::assets::ResourceStream::open(
            package_backend, "data/FONT.FNT");
        (void)bad.read_elements(
            std::numeric_limits<std::size_t>::max(), 2, {});
    } catch (const opentony::assets::ResourceRuntimeError&) {
        rejected = true;
    }
    CHECK(rejected);
    rejected = false;
    try {
        (void)package_loader.load_owned("missing.FNT");
    } catch (const opentony::assets::ResourceRuntimeError&) {
        rejected = true;
    }
    CHECK(rejected);
    return 0;
}
