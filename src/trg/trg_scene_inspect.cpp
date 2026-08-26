#include "level_scene_registry.hpp"
#include "camera_point_registry.hpp"
#include "rail_runtime.hpp"
#include "powerup_runtime.hpp"
#include "../assets/psx_bits_runtime.hpp"
#include "../assets/psx_runtime.hpp"

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
        const opentony::assets::PsxRuntimeEnvironment scene_runtime =
            opentony::assets::PsxRuntimeEnvironment::build(archive);
        opentony::trg::CameraPointRegistry camera_points;
        camera_points.build(runtime.file());
        opentony::trg::RailRuntimeList rails;
        rails.build(runtime.file());
        state.bind_psx_models(archive);
        opentony::trg::LevelSceneRegistry scene;
        scene.build(state, archive);
        const opentony::assets::PsxAssetCatalog catalog =
            opentony::assets::PsxAssetCatalog::scan(argv[3]);
        scene.resolve_factory_assets(catalog);
        opentony::trg::PowerupRuntimeList powerups;
        powerups.build(runtime.file(), &catalog);
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
                  << " runtime_objects=" << scene_runtime.object_count()
                  << " runtime_models=" << scene_runtime.model_count()
                  << " runtime_bytes=" << scene_runtime.allocation_size()
                  << " runtime_materials=" << scene_runtime.materials().records().size()
                  << " animation_products=" << scene_runtime.animations().products().size()
                  << " bits_groups=";
        if (catalog.contains("bits")) {
            opentony::assets::PsxBitsRuntime bits;
            bits.build(catalog.load("bits"));
            std::cout << bits.groups().size();
        } else {
            std::cout << 0;
        }
        std::cout
                  << " camera_points=" << camera_points.entries().size()
                  << " rail_records=" << rails.records().size()
                  << " powerup_records=" << powerups.records().size()
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
