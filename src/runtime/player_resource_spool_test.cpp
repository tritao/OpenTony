#include "player_resource_spool.hpp"

#include "tests/test_check.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>

namespace {

void put16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
}

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

void name32(std::vector<std::byte>& bytes, std::size_t offset, std::string_view value) {
    for (std::size_t index = 0; index < value.size() && index < 32U; ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            static_cast<unsigned char>(value[index]));
    }
}

void append_u32(std::vector<std::byte>& bytes, std::uint32_t value) {
    const std::size_t offset = bytes.size();
    bytes.resize(offset + 4U);
    put32(bytes, offset, value);
}

void align4(std::vector<std::byte>& bytes) {
    while ((bytes.size() & 3U) != 0U) {
        bytes.push_back(std::byte{0});
    }
}

void append_pre_entry(
    std::vector<std::byte>& bytes,
    std::string_view name,
    std::span<const std::byte> payload) {
    for (const char character : name) {
        bytes.push_back(static_cast<std::byte>(
            static_cast<unsigned char>(character)));
    }
    bytes.push_back(std::byte{0});
    align4(bytes);
    append_u32(bytes, static_cast<std::uint32_t>(payload.size()));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    align4(bytes);
}

std::vector<std::byte> make_psh() {
    constexpr std::string_view text =
        "#define PLAYERPART_MODEL_BODY 0\n";
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(
            static_cast<unsigned char>(character)));
    }
    return bytes;
}

std::vector<std::byte> make_psx() {
    // Version 4, marker 2, one object pointing at one empty model. This is
    // the smallest valid PSX runtime object used by the spool boundary test.
    constexpr std::size_t tag_offset = 84U;
    std::vector<std::byte> bytes(tag_offset, std::byte{0});
    put16(bytes, 0, 4);
    put16(bytes, 2, 2);
    put32(bytes, 4, tag_offset);
    put32(bytes, 8, 1);
    put32(bytes, 48, 1);
    put32(bytes, 52, 56);
    return bytes;
}

std::vector<std::byte> make_pre(
    const std::vector<std::byte>& psh,
    const std::vector<std::byte>& psx) {
    std::vector<std::byte> bytes;
    append_u32(bytes, 2);
    append_pre_entry(bytes, "PLAYER.PSH", psh);
    append_pre_entry(bytes, "PLAYER.PSX", psx);
    return bytes;
}

void set_pkr_entry(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::string_view name,
    std::size_t payload_offset,
    std::size_t payload_size) {
    name32(bytes, offset, name);
    put32(bytes, offset + 32U, opentony::assets::kPkrRawMarker);
    put32(bytes, offset + 36U, static_cast<std::uint32_t>(payload_offset));
    put32(bytes, offset + 40U, static_cast<std::uint32_t>(payload_size));
    put32(bytes, offset + 44U, static_cast<std::uint32_t>(payload_size));
}

std::vector<std::byte> make_package(
    const std::vector<std::byte>& psh,
    const std::vector<std::byte>& psx) {
    constexpr std::size_t directory_table = 16U;
    constexpr std::size_t file_table = directory_table + 40U;
    std::vector<std::byte> bytes(file_table + 2U * 48U, std::byte{0});
    bytes[0] = std::byte{'P'};
    bytes[1] = std::byte{'K'};
    bytes[2] = std::byte{'R'};
    bytes[3] = std::byte{'2'};
    put32(bytes, 4, 1);
    put32(bytes, 8, 1);
    put32(bytes, 12, 2);
    name32(bytes, directory_table, "data");
    put32(bytes, directory_table + 32U, file_table);
    put32(bytes, directory_table + 36U, 2);

    const std::size_t psh_offset = bytes.size();
    bytes.insert(bytes.end(), psh.begin(), psh.end());
    const std::size_t psx_offset = bytes.size();
    bytes.insert(bytes.end(), psx.begin(), psx.end());
    set_pkr_entry(bytes, file_table, "PLAYER.PSH", psh_offset, psh.size());
    set_pkr_entry(bytes, file_table + 48U, "PLAYER.PSX", psx_offset, psx.size());
    return bytes;
}

[[nodiscard]] std::string asset_root() {
    return "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
}

} // namespace

int main() {
    const std::vector<std::byte> psh_bytes = make_psh();
    const std::vector<std::byte> psx_bytes = make_psx();

    bool rejected = false;
    try {
        opentony::runtime::PlayerResourceSpool invalid_heap;
        (void)invalid_heap.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::PshRegion,
            2,
            opentony::runtime::kPlayerSpoolNoPadSize);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    CHECK(rejected);

    // A package load owns its parsed runtime result after the package image
    // goes away. The source kind records which side of the common resource
    // boundary supplied the bytes.
    {
        opentony::runtime::PlayerResourceSpool packaged;
        CHECK(packaged.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::PshRegion,
            0,
            opentony::runtime::kPlayerSpoolNoPadSize) == 0);
        CHECK(packaged.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::DirectPsx,
            1,
            0x10800) == 1);
        {
            const auto package = opentony::assets::PkrArchive::parse(
                make_package(psh_bytes, psx_bytes), "player.pkr");
            CHECK(packaged.start_next());
            CHECK(packaged.load_current(package) == 0);
            CHECK(packaged.loaded(0)->source_kind
                == opentony::assets::ResourceSourceKind::PkrPackage);
            CHECK(packaged.loaded(0)->psh->parts().size() == 1);
            packaged.complete_current();
            CHECK(packaged.start_next());
            CHECK(packaged.load_current(package) == 1);
            CHECK(packaged.loaded(1)->source_kind
                == opentony::assets::ResourceSourceKind::PkrPackage);
            CHECK(packaged.loaded(1)->psx->objects().size() == 1);
            CHECK(packaged.loaded(1)->allocation_size == 0x10800);
            packaged.complete_current();
        }
        CHECK(packaged.state() == 0);
        CHECK(packaged.loaded(0) != nullptr);
        CHECK(packaged.loaded(1) != nullptr);
        packaged.release(0);
        packaged.release(1);
        CHECK(packaged.loaded(0) == nullptr);
        CHECK(packaged.loaded(1) == nullptr);
        CHECK(packaged.processed(0));
        CHECK(!packaged.processed(1));
        packaged.reset();
        CHECK(packaged.queued_count() == 0);
        CHECK(packaged.state() == 0);
        CHECK(packaged.processed(0));
    }

    // PRE payload spans are borrowed only during the copy. The parsed PSH and
    // PSX objects remain usable after their container is unloaded.
    {
        opentony::assets::PreRuntimeManager pre;
        pre.load("PLAYER.PRE", make_pre(psh_bytes, psx_bytes));
        opentony::runtime::PlayerResourceSpool from_pre;
        CHECK(from_pre.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::PshRegion,
            0,
            opentony::runtime::kPlayerSpoolNoPadSize) == 0);
        CHECK(from_pre.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::DirectPsx,
            1,
            0x10800) == 1);
        CHECK(from_pre.start_next());
        CHECK(from_pre.load_current("", &pre) == 0);
        CHECK(from_pre.loaded(0)->source_kind
            == opentony::assets::ResourceSourceKind::PreEmbedded);
        from_pre.complete_current();
        CHECK(from_pre.start_next());
        CHECK(from_pre.load_current("", &pre) == 1);
        CHECK(from_pre.loaded(1)->source_kind
            == opentony::assets::ResourceSourceKind::PreEmbedded);
        CHECK(from_pre.loaded(1)->allocation_size == 0x10800);
        from_pre.complete_current();
        pre.unload("player.pre");
        CHECK(from_pre.loaded(0)->psh->parts().front().name == "BODY");
        CHECK(from_pre.loaded(1)->psx->objects().size() == 1);
        from_pre.release(0);
        from_pre.release(1);
    }

    // A malformed PRE payload must not publish an object or advance the
    // request. Once the bad container is replaced, the same request remains
    // retryable; reset/release still provide the explicit cleanup boundary.
    {
        const std::vector<std::byte> malformed_psh{
            std::byte{'#'}, std::byte{'d'}, std::byte{'e'}, std::byte{'f'}};
        opentony::assets::PreRuntimeManager pre;
        pre.load("PLAYER.PRE", make_pre(malformed_psh, psx_bytes));
        opentony::runtime::PlayerResourceSpool failed;
        CHECK(failed.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::PshRegion,
            0,
            opentony::runtime::kPlayerSpoolNoPadSize) == 0);
        CHECK(failed.start_next());
        bool rejected = false;
        try {
            (void)failed.load_current("", &pre);
        } catch (const opentony::assets::PshFormatError&) {
            rejected = true;
        }
        CHECK(rejected);
        CHECK(failed.state() == 1);
        CHECK(!failed.processed(0));
        CHECK(failed.loaded(0) == nullptr);

        pre.unload("player.pre");
        pre.load("PLAYER.PRE", make_pre(psh_bytes, psx_bytes));
        CHECK(failed.load_current("", &pre) == 0);
        CHECK(failed.loaded(0)->psh.has_value());
        failed.complete_current();
        failed.release(0);
    }

    // PKR entry decode can succeed while the contained PSX image is malformed;
    // this must have the same unpublished/retryable result as the PRE path.
    {
        const std::vector<std::byte> malformed_psx{
            std::byte{'n'}, std::byte{'o'}, std::byte{'t'}, std::byte{'p'}};
        const auto package = opentony::assets::PkrArchive::parse(
            make_package(psh_bytes, malformed_psx), "bad-player.pkr");
        opentony::runtime::PlayerResourceSpool failed;
        CHECK(failed.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::DirectPsx,
            1,
            0x10800) == 0);
        CHECK(failed.start_next());
        bool rejected = false;
        try {
            (void)failed.load_current(package);
        } catch (const opentony::assets::PsxFormatError&) {
            rejected = true;
        }
        CHECK(rejected);
        CHECK(failed.state() == 1);
        CHECK(!failed.processed(0));
        CHECK(failed.loaded(0) == nullptr);
        failed.reset();
        CHECK(failed.state() == 0);
        CHECK(failed.loaded(0) == nullptr);
    }

    // A direct resource whose file size exceeds the requested pad/capacity
    // fails before allocation/publication, matching the spool assertion path.
    {
        const auto package = opentony::assets::PkrArchive::parse(
            make_package(psh_bytes, psx_bytes), "small-pad.pkr");
        opentony::runtime::PlayerResourceSpool failed;
        CHECK(failed.enqueue(
            "PLAYER",
            opentony::runtime::PlayerSpoolResourceKind::DirectPsx,
            1,
            4) == 0);
        CHECK(failed.start_next());
        rejected = false;
        try {
            (void)failed.load_current(package);
        } catch (const opentony::assets::ResourceRuntimeError&) {
            rejected = true;
        }
        CHECK(rejected);
        CHECK(failed.state() == 1);
        CHECK(!failed.processed(0));
        CHECK(failed.loaded(0) == nullptr);
        failed.reset();
    }

    if (std::filesystem::is_regular_file(
            std::filesystem::path(asset_root()) / "HAWK2.PSH")
        && std::filesystem::is_regular_file(
            std::filesystem::path(asset_root()) / "HAWK2.PSX")) {

    opentony::runtime::PlayerResourceSpool spool;
    const std::size_t psh = spool.enqueue(
        "HAWK2",
        opentony::runtime::PlayerSpoolResourceKind::PshRegion,
        0,
        opentony::runtime::kPlayerSpoolNoPadSize);
    const std::size_t psx = spool.enqueue(
        "HAWK2",
        opentony::runtime::PlayerSpoolResourceKind::DirectPsx,
        1,
        0x10800);
    CHECK(psh == 0);
    CHECK(psx == 1);
    CHECK(spool.queued_count() == 2);
    CHECK(spool.name(0) == "HAWK2");
    CHECK(spool.kind(0)
        == opentony::runtime::PlayerSpoolResourceKind::PshRegion);
    CHECK(spool.heap_selector(0) == 0);
    CHECK(spool.request_size_staging(0)
        == opentony::runtime::kPlayerSpoolNoPadSize);
    CHECK(!spool.processed(0));

    CHECK(spool.start_next());
    CHECK(spool.state() == 1);
    CHECK(spool.load_current(asset_root()) == 0);
    CHECK(spool.processed(0));
    CHECK(spool.loaded(0) != nullptr);
    CHECK(spool.loaded(0)->psh.has_value());
    CHECK(spool.loaded(0)->psh->parts().size() == 19);
    spool.complete_current();
    CHECK(spool.consume_index() == 1);
    CHECK(spool.state() == 2);

    CHECK(spool.start_next());
    CHECK(spool.load_current(asset_root()) == 1);
    CHECK(spool.loaded(1) != nullptr);
    CHECK(spool.loaded(1)->psx.has_value());
    CHECK(spool.loaded(1)->psx->objects().size() == 19);
    spool.complete_current();
    CHECK(spool.consume_index() == 2);
    CHECK(spool.state() == 0);
    CHECK(!spool.start_next());

    spool.reset();
    CHECK(spool.queued_count() == 0);
    CHECK(spool.consume_index() == 0);
    CHECK(spool.state() == 0);
    CHECK(spool.loaded(0) == nullptr);

    const std::filesystem::path package_path =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(package_path)) {
        const auto package = opentony::assets::PkrArchive::load(
            package_path.string());
        opentony::runtime::PlayerResourceSpool packaged;
        CHECK(packaged.enqueue(
            "HAWK2",
            opentony::runtime::PlayerSpoolResourceKind::PshRegion) == 0);
        CHECK(packaged.enqueue(
            "HAWK2",
            opentony::runtime::PlayerSpoolResourceKind::DirectPsx) == 1);
        CHECK(packaged.start_next());
        CHECK(packaged.load_current(package) == 0);
        CHECK(packaged.loaded(0) != nullptr);
        CHECK(packaged.loaded(0)->psh->parts().size() == 19);
        packaged.complete_current();
        CHECK(packaged.start_next());
        CHECK(packaged.load_current(package) == 1);
        CHECK(packaged.loaded(1) != nullptr);
        CHECK(packaged.loaded(1)->psx->objects().size() == 19);
    }
    }

    std::cout << "Player resource spool tests passed\n";
}
