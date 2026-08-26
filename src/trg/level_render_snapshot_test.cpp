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
            level.scene_runtime(),
            &level.powerups(),
            level.item_runtime(),
            level.medal_runtime());

    assert(snapshot.entities().size() == level.scene().entities().size());
    assert(level.item_runtime() != nullptr);
    std::size_t rendered_pickups = 0;
    for (const auto& entity : snapshot.entities()) {
        if (entity.kind != opentony::trg::LevelSceneEntityKind::Pickup) {
            continue;
        }
        ++rendered_pickups;
        assert(entity.source_node != opentony::trg::CommandPointRuntime::npos);
        if (entity.model_index == opentony::trg::CommandPointRuntime::npos) {
            assert(entity.first_face == opentony::trg::CommandPointRuntime::npos);
            assert(entity.face_count == 0);
        } else {
            assert(entity.first_face != opentony::trg::CommandPointRuntime::npos);
            assert(entity.face_count > 0);
        }
    }
    assert(rendered_pickups == level.powerups().records().size());
    std::size_t expected_faces = 0;
    for (const auto& entity : snapshot.entities()) {
        if (entity.face_count == 0) {
            assert(entity.first_face == opentony::trg::CommandPointRuntime::npos);
            continue;
        }
        assert(entity.first_face != opentony::trg::CommandPointRuntime::npos);
        expected_faces += entity.face_count;
    }
    assert(expected_faces == snapshot.faces().size());
    assert(!snapshot.faces().empty());
    assert(snapshot.faces()[0].vertex_count == 3
        || snapshot.faces()[0].vertex_count == 4);
    std::size_t textured_faces = 0;
    for (const auto& face : snapshot.faces()) {
        if (!face.has_texture) {
            assert(face.runtime_material_index
                == opentony::trg::CommandPointRuntime::npos);
            continue;
        }
        ++textured_faces;
        assert(face.runtime_material_index
            != opentony::trg::CommandPointRuntime::npos);
        assert(face.material_checksum != 0);
        if (face.object_index != opentony::trg::CommandPointRuntime::npos) {
            assert(level.scene_runtime().materials().record(
                face.runtime_material_index).checksum() == face.material_checksum);
        }
    }
    assert(textured_faces > 0);

    std::cout << "Level render snapshot tests passed\n";
}
