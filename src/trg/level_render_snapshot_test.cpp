#include "level_render_snapshot.hpp"
#include "level_runtime.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] std::string asset_path(const char* relative) {
    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    return (root / relative).string();
}

} // namespace

int main() {
    const std::string trg = asset_path("SKWARE_T.TRG");
    const std::string psx = asset_path("SKWARE.PSX");
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::trg::LevelRuntime level(trg, psx, asset_path(""));
    level.initialize();
    const opentony::trg::LevelRenderSnapshot snapshot =
        opentony::trg::LevelRenderSnapshot::build(
            level.scene(),
            level.scene_asset());

    assert(snapshot.entities().size() == level.scene().entities().size());
    std::size_t expected_faces = 0;
    for (const auto& entity : snapshot.entities()) {
        if (entity.psx_object_index == opentony::trg::CommandPointRuntime::npos) {
            assert(entity.face_count == 0);
            continue;
        }
        assert(entity.first_face != opentony::trg::CommandPointRuntime::npos);
        expected_faces += level.scene_asset().models()[entity.model_index].faces.size();
    }
    assert(expected_faces == snapshot.faces().size());
    assert(!snapshot.faces().empty());
    assert(snapshot.faces()[0].vertex_count == 3
        || snapshot.faces()[0].vertex_count == 4);

    std::cout << "Level render snapshot tests passed\n";
}
