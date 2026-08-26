#include "level_trigger_state.hpp"
#include "level_scene_registry.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    if (argc < 2 || argc > 6) {
        std::cerr << "usage: opentony_trg_inspect FILE.trg [LEVEL.psx] [--warehouse-gaps] [--dispatch-all] [--print-position-witnesses]\n";
        return 2;
    }
    try {
        bool warehouse_gaps = false;
        bool dispatch_all = false;
        bool print_position_witnesses = false;
        for (int index = 2; index < argc; ++index) {
            const std::string_view argument = argv[index];
            if (argument == "--warehouse-gaps") {
                warehouse_gaps = true;
            } else if (argument == "--dispatch-all") {
                dispatch_all = true;
            } else if (argument == "--print-position-witnesses") {
                print_position_witnesses = true;
            }
        }
        const bool has_psx = argc >= 3
            && std::string_view(argv[2]).starts_with("--") == false;
        opentony::trg::TrgFile file = opentony::trg::TrgFile::load(argv[1]);
        opentony::trg::LevelTriggerState state;
        opentony::trg::LevelSceneRegistry scene;
        if (warehouse_gaps) {
            state.set_gap_table(&opentony::trg::retail_warehouse_gap_table());
        }
        opentony::trg::TriggerRuntime runtime(std::move(file), state);
        runtime.initialize();
        std::size_t dispatched = 0;
        if (dispatch_all) {
            for (const auto& node : runtime.file().nodes()) {
                if (node.type != 6) {
                    continue;
                }
                runtime.pulse_node(node.index);
                ++dispatched;
            }
        }
        if (has_psx) {
            const opentony::assets::PsxArchive archive =
                opentony::assets::PsxArchive::load(argv[2]);
            state.bind_psx_models(archive);
            scene.build(state, archive);
            if (print_position_witnesses) {
                for (const auto& object : state.objects()) {
                    if (!object.has_position
                        || object.spawn_family == opentony::trg::TriggerSpawnFamily::Unknown) {
                        continue;
                    }
                    std::size_t matches = 0;
                    std::size_t first_match = opentony::trg::CommandPointRuntime::npos;
                    for (std::size_t psx_index = 0;
                         psx_index < archive.objects().size(); ++psx_index) {
                        if (archive.objects()[psx_index].position
                            != object.position) {
                            continue;
                        }
                        if (matches == 0) {
                            first_match = psx_index;
                        }
                        ++matches;
                    }
                    if (object.spawn_family != opentony::trg::TriggerSpawnFamily::Object192
                        && object.spawn_family != opentony::trg::TriggerSpawnFamily::ObjectCb) {
                        continue;
                    }
                    std::cout << "position_witness node=" << object.node
                              << " subtype=0x" << std::hex << object.subtype
                              << std::dec << " family="
                              << static_cast<unsigned>(object.spawn_family)
                              << " psx_matches=" << matches;
                    if (matches == 1) {
                        const auto& witness = archive.objects()[first_match];
                        std::cout << " psx_object=" << first_match
                                  << " model=" << witness.model_index;
                    }
                    std::cout
                              << " position=" << object.position[0] << ","
                              << object.position[1] << ","
                              << object.position[2] << '\n';
                }
            }
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
                  << " dispatched=" << dispatched
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
