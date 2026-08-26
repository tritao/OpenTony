#include "level_render_snapshot.hpp"
#include "level_runtime.hpp"

#include <algorithm>
#include "tests/test_check.hpp"
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

    CHECK(snapshot.entities().size() == level.scene().entities().size());
    std::size_t expected_faces = 0;
    for (const auto& entity : snapshot.entities()) {
        if (entity.psx_object_index == opentony::trg::CommandPointRuntime::npos) {
            if (entity.kind == opentony::trg::LevelSceneEntityKind::Pickup
                && !entity.model_resource.empty()) {
                CHECK(entity.face_count > 0);
                expected_faces += level.asset_catalog()->load(
                    entity.model_resource).models()[entity.resource_model_index].faces.size();
            } else {
                CHECK(entity.face_count == 0);
            }
            continue;
        }
        CHECK(entity.first_face != opentony::trg::CommandPointRuntime::npos);
        expected_faces += level.scene_asset().models()[entity.model_index].faces.size();
    }
    CHECK(expected_faces == snapshot.faces().size());
    CHECK(!snapshot.faces().empty());
    CHECK(snapshot.faces()[0].vertex_count == 3
        || snapshot.faces()[0].vertex_count == 4);

    const auto* pickup_binding = level.scene().binding(17);
    CHECK(pickup_binding != nullptr);
    const auto* pickup_source = level.scene().entity(pickup_binding->entities.front());
    CHECK(pickup_source != nullptr);
    const auto pickup_render = std::find_if(
        snapshot.entities().begin(),
        snapshot.entities().end(),
        [pickup_source](const opentony::trg::LevelRenderEntitySnapshot& entity) {
            return entity.entity == pickup_source->entity;
        });
    CHECK(pickup_render != snapshot.entities().end());
    CHECK(pickup_render->model_resource == "items");
    CHECK(pickup_render->resource_model_index == 5);
    CHECK(pickup_render->model_index == 5);
    CHECK(pickup_render->face_count
        == level.asset_catalog()->load("ITEMS").models()[5].faces.size());

    std::cout << "Level render snapshot tests passed\n";
}
