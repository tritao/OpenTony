#include "level_trigger_state.hpp"
#include "level_scene_registry.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4 || (argc == 4 && std::string_view(argv[3]) != "--warehouse-gaps")) {
        std::cerr << "usage: opentony_trg_inspect FILE.trg [LEVEL.psx] [--warehouse-gaps]\n";
        return 2;
    }
    try {
        opentony::trg::TrgFile file = opentony::trg::TrgFile::load(argv[1]);
        opentony::trg::LevelTriggerState state;
        opentony::trg::LevelSceneRegistry scene;
        if (argc == 4) {
            state.set_gap_table(&opentony::trg::retail_warehouse_gap_table());
        }
        opentony::trg::TriggerRuntime runtime(std::move(file), state);
        runtime.initialize();
        if (argc >= 3) {
            const opentony::assets::PsxArchive archive =
                opentony::assets::PsxArchive::load(argv[2]);
            state.bind_psx_models(archive);
            scene.build(state, archive);
        }
        const auto object_count = std::count_if(
            state.objects().begin(),
            state.objects().end(),
            [](const opentony::trg::TriggerObjectState& object) {
                return object.kind == opentony::trg::TriggerObjectKind::Object;
            });
        const auto pickup_count = std::count_if(
            state.objects().begin(),
            state.objects().end(),
            [](const opentony::trg::TriggerObjectState& object) {
                return object.kind == opentony::trg::TriggerObjectKind::Pickup;
            });
        const auto positioned_count = std::count_if(
            state.objects().begin(),
            state.objects().end(),
            [](const opentony::trg::TriggerObjectState& object) {
                return object.has_position;
            });
        const auto oriented_count = std::count_if(
            state.objects().begin(),
            state.objects().end(),
            [](const opentony::trg::TriggerObjectState& object) {
                return object.has_orientation;
            });
        std::cout << "nodes=" << runtime.file().nodes().size()
                  << " command_points=" << runtime.command_points().size()
                  << " objects=" << object_count
                  << " pickups=" << pickup_count
                  << " positioned=" << positioned_count
                  << " oriented=" << oriented_count
                  << " restarts=" << state.restarts().size()
                  << " resources=" << state.resources().size()
                  << " bound_models=" << state.bound_model_count()
                  << " scene_instances=" << state.bound_scene_instance_count()
                  << " scene_positioned=" << state.bound_scene_position_count()
                  << " scene_entities=" << scene.entities().size()
                  << " scene_static=" << scene.static_entity_count()
                  << " scene_trigger=" << scene.trigger_entity_count()
                  << " scene_bound=" << scene.bound_trigger_count()
                  << " scene_unresolved=" << scene.unresolved_trigger_count()
                  << " legacy=" << state.legacy_commands().size()
                  << " diagnostics=" << state.diagnostics().size() << '\n';
    } catch (const opentony::trg::FormatError& error) {
        std::cerr << "TRG error: " << error.what() << '\n';
        return 1;
    } catch (const opentony::assets::PsxFormatError& error) {
        std::cerr << "PSX error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
