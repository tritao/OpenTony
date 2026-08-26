#include "pkr_asset.hpp"
#include "psx_asset.hpp"

#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <zlib.h>

namespace {

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

void name32(std::vector<std::byte>& bytes, std::size_t offset, const char* value) {
    for (std::size_t index = 0; value[index] != '\0' && index < 32; ++index) {
        bytes[offset + index] = static_cast<std::byte>(value[index]);
    }
}

std::size_t append(std::vector<std::byte>& bytes, const std::vector<std::byte>& data) {
    const std::size_t offset = bytes.size();
    bytes.insert(bytes.end(), data.begin(), data.end());
    return offset;
}

void set_entry(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    const char* name,
    std::uint32_t marker,
    std::size_t payload,
    std::uint32_t stored,
    std::uint32_t decoded) {
    name32(bytes, offset, name);
    put32(bytes, offset + 32, marker);
    put32(bytes, offset + 36, static_cast<std::uint32_t>(payload));
    put32(bytes, offset + 40, stored);
    put32(bytes, offset + 44, decoded);
}

} // namespace

int main() {
    constexpr std::size_t directory_table = 16;
    constexpr std::size_t file_table = directory_table + 40;
    std::vector<std::byte> bytes(file_table + 4 * 48, std::byte{0});
    bytes[0] = std::byte{'P'};
    bytes[1] = std::byte{'K'};
    bytes[2] = std::byte{'R'};
    bytes[3] = std::byte{'2'};
    put32(bytes, 4, 1);
    put32(bytes, 8, 1);
    put32(bytes, 12, 4);
    name32(bytes, directory_table, "data");
    put32(bytes, directory_table + 32, static_cast<std::uint32_t>(file_table));
    put32(bytes, directory_table + 36, 4);

    const std::size_t raw_offset = append(bytes, {
        std::byte{0x10}, std::byte{0x11}, std::byte{0x12}});
    const std::size_t bibd_offset = append(bytes, {
        std::byte{3}, std::byte{'a'}, std::byte{2}, std::byte{'b'}});
    const std::size_t wibd_offset = append(bytes, {
        std::byte{2}, std::byte{0}, std::byte{'c'}});
    const std::vector<std::byte> zlib_source{
        std::byte{'h'}, std::byte{'e'}, std::byte{'l'}, std::byte{'l'}, std::byte{'o'}};
    uLongf compressed_size = compressBound(static_cast<uLong>(zlib_source.size()));
    std::vector<std::byte> compressed(compressed_size);
    const int compressed_result = compress(
        reinterpret_cast<Bytef*>(compressed.data()),
        &compressed_size,
        reinterpret_cast<const Bytef*>(zlib_source.data()),
        static_cast<uLong>(zlib_source.size()));
    CHECK(compressed_result == Z_OK);
    compressed.resize(static_cast<std::size_t>(compressed_size));
    const std::size_t zlib_offset = append(bytes, compressed);

    set_entry(bytes, file_table + 0 * 48, "RAW.BIN",
        opentony::assets::kPkrRawMarker, raw_offset, 3, 3);
    set_entry(bytes, file_table + 1 * 48, "BYTE.BIN", 0, bibd_offset, 4, 5);
    set_entry(bytes, file_table + 2 * 48, "WORD.BIN", 1, wibd_offset, 3, 2);
    set_entry(bytes, file_table + 3 * 48, "ZLIB.BIN", 2, zlib_offset,
        static_cast<std::uint32_t>(compressed.size()), 5);

    const opentony::assets::PkrArchive archive =
        opentony::assets::PkrArchive::parse(std::move(bytes), "all.pkr");
    CHECK(archive.version() == 1);
    CHECK(archive.directories().size() == 1);
    CHECK(archive.entries().size() == 4);
    CHECK(archive.find("data/RAW.BIN") != nullptr);
    CHECK(archive.decode("data/RAW.BIN").size() == 3);
    const auto byte_decoded = archive.decode(1);
    CHECK(byte_decoded == std::vector<std::byte>({
        std::byte{'a'}, std::byte{'a'}, std::byte{'a'}, std::byte{'b'}, std::byte{'b'}}));
    CHECK(archive.decode(2) == std::vector<std::byte>({std::byte{'c'}, std::byte{'c'}}));
    CHECK(archive.decode(3) == zlib_source);

    const std::filesystem::path retail_package =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(retail_package)) {
        const auto retail = opentony::assets::PkrArchive::load(retail_package.string());
        CHECK(retail.version() == 1);
        CHECK(retail.directories().size() == 21);
        CHECK(retail.entries().size() == 3771);
        const auto* warehouse = retail.find("data/SKWARE.PSX");
        CHECK(warehouse != nullptr);
        CHECK(warehouse->marker == opentony::assets::kPkrRawMarker);
        CHECK(warehouse->stored_size == 0x20034);
        CHECK(warehouse->decoded_size == 0x20034);
        const auto warehouse_bytes = retail.decode("data/SKWARE.PSX");
        CHECK(warehouse_bytes.size() == 0x20034);
        const auto scene = opentony::assets::PsxArchive::parse(
            warehouse_bytes, "ALL.PKR/data/SKWARE.PSX");
        CHECK(scene.objects().size() == 252);
        CHECK(scene.models().size() == 288);
    }
    return 0;
}
