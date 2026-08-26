#include "player_resource_spool.hpp"

#include "tests/test_check.hpp"
#include <filesystem>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] std::string asset_root() {
    return "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
}

} // namespace

int main() {
    if (!std::filesystem::is_regular_file(
            std::filesystem::path(asset_root()) / "HAWK2.PSH")
        || !std::filesystem::is_regular_file(
            std::filesystem::path(asset_root()) / "HAWK2.PSX")) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::runtime::PlayerResourceSpool spool;
    const std::size_t psh = spool.enqueue(
        "HAWK2",
        opentony::runtime::PlayerSpoolResourceKind::PshRegion,
        1,
        0x10800,
        0xff);
    const std::size_t psx = spool.enqueue(
        "HAWK2",
        opentony::runtime::PlayerSpoolResourceKind::DirectPsx,
        1,
        0x10800,
        0);
    CHECK(psh == 0);
    CHECK(psx == 1);
    CHECK(spool.queued_count() == 2);
    CHECK(spool.name(0) == "HAWK2");
    CHECK(spool.kind(0)
        == opentony::runtime::PlayerSpoolResourceKind::PshRegion);
    CHECK(spool.heap_selector(0) == 1);
    CHECK(spool.request_flags(0) == 0xff);
    CHECK(spool.request_size_staging(0) == 0x10800);
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

    std::cout << "Player resource spool tests passed\n";
}
