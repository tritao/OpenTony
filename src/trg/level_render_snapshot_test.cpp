#include "level_render_snapshot.hpp"
#include "level_runtime.hpp"

#include <algorithm>
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
            level.scene_asset(),
            level.asset_catalog());

    assert(snapshot.entities().size() == level.scene().entities().size());
    std::size_t expected_faces = 0;
    for (const auto& entity : snapshot.entities()) {
        if (entity.psx_object_index == opentony::trg::CommandPointRuntime::npos) {
            if (entity.kind == opentony::trg::LevelSceneEntityKind::Pickup
                && !entity.model_resource.empty()) {
                assert(entity.face_count > 0);
                expected_faces += level.asset_catalog()->load(
                    entity.model_resource).models()[entity.resource_model_index].faces.size();
            } else {
                assert(entity.face_count == 0);
            }
            continue;
        }
        assert(entity.first_face != opentony::trg::CommandPointRuntime::npos);
        expected_faces += level.scene_asset().models()[entity.model_index].faces.size();
    }
    assert(expected_faces == snapshot.faces().size());
    assert(!snapshot.faces().empty());
    assert(snapshot.faces()[0].vertex_count == 3
        || snapshot.faces()[0].vertex_count == 4);

    const auto* pickup_binding = level.scene().binding(17);
    assert(pickup_binding != nullptr);
    const auto* pickup_source = level.scene().entity(pickup_binding->entities.front());
    assert(pickup_source != nullptr);
    const auto pickup_render = std::find_if(
        snapshot.entities().begin(),
        snapshot.entities().end(),
        [pickup_source](const opentony::trg::LevelRenderEntitySnapshot& entity) {
            return entity.entity == pickup_source->entity;
        });
    assert(pickup_render != snapshot.entities().end());
    assert(pickup_render->model_resource == "items");
    assert(pickup_render->resource_model_index == 5);
    assert(pickup_render->model_index == 5);
    assert(pickup_render->face_count
        == level.asset_catalog()->load("ITEMS").models()[5].faces.size());

    std::cout << "Level render snapshot tests passed\n";
}
