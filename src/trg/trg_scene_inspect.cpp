#include "level_scene_registry.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: opentony_trg_scene_inspect FILE.trg FILE.psx ASSET_ROOT\n";
        return 2;
    }
    try {
        opentony::trg::LevelTriggerState state;
        opentony::trg::TriggerRuntime runtime(
            opentony::trg::TrgFile::load(argv[1]),
            state);
        runtime.initialize();
        const opentony::assets::PsxArchive archive =
            opentony::assets::PsxArchive::load(argv[2]);
        state.bind_psx_models(archive);
        opentony::trg::LevelSceneRegistry scene;
        scene.build(state, archive);
        const opentony::assets::PsxAssetCatalog catalog =
            opentony::assets::PsxAssetCatalog::scan(argv[3]);
        scene.resolve_factory_assets(catalog);
        std::size_t factory_entities = 0;
        std::size_t resolved_factory_entities = 0;
        for (const opentony::trg::LevelSceneEntity& entity : scene.entities()) {
            if (entity.factory_resource.empty()) {
                continue;
            }
            ++factory_entities;
            if (entity.factory_asset_available) {
                ++resolved_factory_entities;
            }
        }
        std::cout << "scene_entities=" << scene.entities().size()
                  << " bound=" << scene.bound_trigger_count()
                  << " unresolved=" << scene.unresolved_trigger_count()
                  << " factory_entities=" << factory_entities
                  << " factory_assets=" << resolved_factory_entities << '\n';
    } catch (const opentony::trg::FormatError& error) {
        std::cerr << "TRG error: " << error.what() << '\n';
        return 1;
    } catch (const opentony::assets::PsxFormatError& error) {
        std::cerr << "PSX error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
