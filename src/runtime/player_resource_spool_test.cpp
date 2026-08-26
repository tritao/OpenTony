#include "player_resource_spool.hpp"

#include <cassert>
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
    assert(psh == 0);
    assert(psx == 1);
    assert(spool.queued_count() == 2);
    assert(spool.name(0) == "HAWK2");
    assert(spool.kind(0)
        == opentony::runtime::PlayerSpoolResourceKind::PshRegion);
    assert(spool.heap_selector(0) == 1);
    assert(spool.request_flags(0) == 0xff);
    assert(spool.request_size_staging(0) == 0x10800);
    assert(!spool.processed(0));

    assert(spool.start_next());
    assert(spool.state() == 1);
    assert(spool.load_current(asset_root()) == 0);
    assert(spool.processed(0));
    assert(spool.loaded(0) != nullptr);
    assert(spool.loaded(0)->psh.has_value());
    assert(spool.loaded(0)->psh->parts().size() == 19);
    spool.complete_current();
    assert(spool.consume_index() == 1);
    assert(spool.state() == 2);

    assert(spool.start_next());
    assert(spool.load_current(asset_root()) == 1);
    assert(spool.loaded(1) != nullptr);
    assert(spool.loaded(1)->psx.has_value());
    assert(spool.loaded(1)->psx->objects().size() == 19);
    spool.complete_current();
    assert(spool.consume_index() == 2);
    assert(spool.state() == 0);
    assert(!spool.start_next());

    spool.reset();
    assert(spool.queued_count() == 0);
    assert(spool.consume_index() == 0);
    assert(spool.state() == 0);
    assert(spool.loaded(0) == nullptr);

    const std::filesystem::path package_path =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(package_path)) {
        const auto package = opentony::assets::PkrArchive::load(
            package_path.string());
        opentony::runtime::PlayerResourceSpool packaged;
        assert(packaged.enqueue(
            "HAWK2",
            opentony::runtime::PlayerSpoolResourceKind::PshRegion) == 0);
        assert(packaged.enqueue(
            "HAWK2",
            opentony::runtime::PlayerSpoolResourceKind::DirectPsx) == 1);
        assert(packaged.start_next());
        assert(packaged.load_current(package) == 0);
        assert(packaged.loaded(0) != nullptr);
        assert(packaged.loaded(0)->psh->parts().size() == 19);
        packaged.complete_current();
        assert(packaged.start_next());
        assert(packaged.load_current(package) == 1);
        assert(packaged.loaded(1) != nullptr);
        assert(packaged.loaded(1)->psx->objects().size() == 19);
    }

    std::cout << "Player resource spool tests passed\n";
}
