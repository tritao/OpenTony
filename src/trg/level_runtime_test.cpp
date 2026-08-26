#include "level_runtime.hpp"
#include "../runtime/level_frame.hpp"
#include "../runtime/psx_collision_probe.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

class Observer final : public opentony::runtime::LevelFrameObserver {
public:
    void on_input_frame(
        std::uint64_t frame,
        const opentony::runtime::InputState& input) override {
        assert(frame == 1);
        assert(input.pressed(opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left)));
        ++input_calls;
    }

    void on_level_tick(
        std::uint64_t frame,
        std::uint32_t milliseconds,
        const opentony::runtime::InputState&,
        const opentony::trg::LevelRuntime& level) override {
        assert(frame == 1);
        assert(milliseconds == 16);
        assert(level.state().time_ms() == 32);
        ++tick_calls;
    }

    std::size_t input_calls{};
    std::size_t tick_calls{};
};

[[nodiscard]] std::string asset_path(const char* relative) {
    const std::filesystem::path root = "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    return (root / relative).string();
}

} // namespace

int main() {
    const std::string trg = asset_path("SKWARE_T.TRG");
    const std::string psx = asset_path("SKWARE.PSX");
    const std::string asset_root = "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    if (!std::filesystem::is_regular_file(trg) || !std::filesystem::is_regular_file(psx)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::trg::LevelRuntime runtime(trg, psx, asset_root);
    assert(!runtime.initialized());
    runtime.initialize();
    assert(runtime.initialized());
    assert(runtime.file().nodes().size() == 313);
    assert(runtime.triggers().command_points().size() == 109);
    assert(runtime.scene().static_entity_count() == 252);
    assert(runtime.scene().bound_trigger_count() == 95);
    assert(runtime.collision().grids().size() == 1);
    assert(runtime.collision().referenced_object_count() == 252);
    assert(runtime.collision().face_count() == 1345);
    const auto& first_face = runtime.collision().faces().front();
    const opentony::runtime::FixedPosition face_center{
        static_cast<std::int32_t>((static_cast<std::int64_t>(first_face.vertices[0][0])
            + first_face.vertices[1][0] + first_face.vertices[2][0]) / 3),
        static_cast<std::int32_t>((static_cast<std::int64_t>(first_face.vertices[0][1])
            + first_face.vertices[1][1] + first_face.vertices[2][1]) / 3),
        static_cast<std::int32_t>((static_cast<std::int64_t>(first_face.vertices[0][2])
            + first_face.vertices[1][2] + first_face.vertices[2][2]) / 3),
    };
    const opentony::runtime::FixedPosition face_probe_start{
        face_center[0] + first_face.normal[0],
        face_center[1] + first_face.normal[1],
        face_center[2] + first_face.normal[2],
    };
    const opentony::runtime::FixedPosition face_probe_end{
        face_center[0] - first_face.normal[0],
        face_center[1] - first_face.normal[1],
        face_center[2] - first_face.normal[2],
    };
    const opentony::runtime::PsxPositionCollisionProbe face_probe(
        runtime.collision(),
        face_probe_start);
    const auto face_hit = face_probe.hit(face_probe_end);
    assert(face_hit.has_value());
    assert(face_hit->object_index == first_face.object_index);
    assert(face_hit->surface_flags == first_face.surface_flags);
    assert(runtime.asset_catalog() != nullptr);
    assert(runtime.pre_catalog() != nullptr);
    assert(runtime.pre_catalog()->size() == 19);
    assert(runtime.pre_catalog()->load("LEVEL").entries().size() > 0);
    assert(runtime.resources().size() == 2);
    for (const auto& resource : runtime.resources()) {
        assert(resource.asset_available);
        assert(resource.asset_loaded);
        if (resource.request.name == "SkWare_O") {
            assert(resource.asset_object_count == 25);
            assert(resource.asset_model_count == 25);
        } else {
            // SKWARE_L.PSX is a valid empty level-side resource in this
            // build; successful parsing is the contract, not nonzero counts.
            assert(resource.asset_object_count == 0);
            assert(resource.asset_model_count == 0);
        }
    }
    assert(runtime.asset_catalog()->contains("ITEMS"));
    assert(runtime.asset_catalog()->contains("SKMEDALS"));
    const auto* warehouse_pickup = runtime.state().object(17);
    assert(warehouse_pickup != nullptr);
    assert(warehouse_pickup->node_type == 5);
    assert(warehouse_pickup->subtype == 6);
    assert(warehouse_pickup->pickup_resource == "items");
    assert(warehouse_pickup->pickup_model_checksum == 0x2ebf22ca);
    assert(warehouse_pickup->pickup_model_resolved);
    assert(warehouse_pickup->pickup_model_index == 5);
    assert(warehouse_pickup->pickup_visual_state_d1 == 1);
    assert(warehouse_pickup->pickup_motion_state_d2 == 0);
    assert(warehouse_pickup->pickup_motion_substate_d3 == 0);
    assert(!warehouse_pickup->pickup_glow_present);
    assert(warehouse_pickup->pickup_update_calls == 0);
    assert(warehouse_pickup->position[0] == 7673 * 4096);
    assert(warehouse_pickup->position[1] == -427 * 4096);
    assert(warehouse_pickup->position[2] == 12132 * 4096);
    const auto* pickup_binding = runtime.scene().binding(17);
    assert(pickup_binding != nullptr);
    assert(!pickup_binding->bound_to_psx);
    const auto* pickup_entity = runtime.scene().entity(pickup_binding->entities.front());
    assert(pickup_entity != nullptr);
    assert(pickup_entity->kind == opentony::trg::LevelSceneEntityKind::Pickup);
    assert(pickup_entity->pickup_resource == "items");
    assert(pickup_entity->pickup_model_checksum == 0x2ebf22ca);
    assert(pickup_entity->pickup_model_resolved);
    assert(pickup_entity->pickup_model_index == 5);
    assert(pickup_entity->factory_asset_loaded);
    assert(pickup_entity->factory_asset_model_count == 13);
    std::size_t pickup_count = 0;
    std::size_t resolved_pickup_count = 0;
    for (const auto& object : runtime.state().objects()) {
        if (object.spawn_family != opentony::trg::TriggerSpawnFamily::Pickup) {
            continue;
        }
        ++pickup_count;
        if (object.pickup_model_resolved) {
            ++resolved_pickup_count;
        }
    }
    assert(pickup_count == 12);
    // The recovered retail table is intentionally partial: unclassified
    // Warehouse subtypes remain source-visible rather than receiving a guess.
    assert(resolved_pickup_count > 0);
    std::size_t loaded_factory_assets = 0;
    for (const auto& entity : runtime.scene().entities()) {
        if (!entity.factory_resource.empty()) {
            assert(entity.factory_asset_loaded == entity.factory_asset_available);
            if (entity.factory_asset_loaded) {
                ++loaded_factory_assets;
                assert(entity.factory_asset_model_count > 0);
            }
        }
    }
    assert(loaded_factory_assets == 0);

    // Warehouse node 120 is a type-12 linked special object. Its TRG key
    // resolves into the SKWARE model-name namespace, and a pulse carries the
    // verified FUN_004bdc40 asset-side writes into the scene binding.
    const auto* warehouse_special = runtime.state().object(120);
    assert(warehouse_special != nullptr);
    assert(warehouse_special->has_special_runtime);
    assert(warehouse_special->special_runtime_allocation_size == 0x18);
    assert(warehouse_special->special_runtime_vtable == 0x0051982c);
    assert(warehouse_special->special_runtime_list
        == opentony::trg::TriggerSpecialRuntimeList::Type12Type14);
    const auto* warehouse_binding = runtime.scene().binding(120);
    assert(warehouse_binding != nullptr);
    assert(warehouse_binding->bound_to_psx);
    runtime.pulse_node(120);
    warehouse_special = runtime.state().object(120);
    assert(warehouse_special->special_runtime_active);
    assert(warehouse_special->special_asset_flags_or == 4);
    assert(warehouse_special->special_asset_marker == 0x202020);
    assert(runtime.scene().entity(warehouse_binding->entities.front())
        ->has_special_asset_state);
    assert(runtime.scene().entity(warehouse_binding->entities.front())
        ->has_special_runtime);
    assert(runtime.scene().entity(warehouse_binding->entities.front())
        ->special_runtime_allocation_size == 0x18);
    assert(runtime.scene().entity(warehouse_binding->entities.front())
        ->special_runtime_vtable == 0x0051982c);
    assert(runtime.scene().entity(warehouse_binding->entities.front())
        ->special_runtime_active);

    // SKWARE node 2 is a recovered subtype-0xcb factory bridge. Node 0xcf
    // in the visible sample is a separate type-2 linked scene object.
    const auto* warehouse_object = runtime.state().object(2);
    assert(warehouse_object != nullptr);
    assert(warehouse_object->spawn_family
        == opentony::trg::TriggerSpawnFamily::ObjectCb);
    assert(warehouse_object->factory_allocation_size == 0x1f4);
    assert(warehouse_object->factory_vtable == 0x005183b0);
    assert(warehouse_object->factory_list
        == opentony::trg::TriggerFactoryList::CommonObject);
    assert(warehouse_object->has_factory_initial_activation_byte);
    assert(warehouse_object->factory_initial_activation_byte == 1);
    const auto* warehouse_object_binding = runtime.scene().binding(2);
    assert(warehouse_object_binding != nullptr);
    const auto* warehouse_object_entity = runtime.scene().entity(
        warehouse_object_binding->entities.front());
    assert(warehouse_object_entity != nullptr);
    assert(warehouse_object_entity->factory_allocation_size == 0x1f4);
    assert(warehouse_object_entity->factory_vtable == 0x005183b0);

    const std::string special_trg = asset_path("SKPH_T.TRG");
    const std::string special_psx = asset_path("SKPH.PSX");
    if (std::filesystem::is_regular_file(special_trg)
        && std::filesystem::is_regular_file(special_psx)) {
        opentony::trg::LevelRuntime special(special_trg, special_psx, asset_root);
        special.initialize();
        std::size_t special_factory_assets = 0;
        for (const auto& entity : special.scene().entities()) {
            if (entity.factory_resource.empty()) {
                continue;
            }
            assert(entity.factory_asset_loaded);
            assert(entity.factory_asset_object_count > 0);
            assert(entity.factory_asset_model_count > 0);
            ++special_factory_assets;
        }
        assert(special_factory_assets == 10);
    }

    runtime.tick(16);
    assert(runtime.state().time_ms() == 16);
    warehouse_pickup = runtime.state().object(17);
    assert(warehouse_pickup->pickup_update_calls == 1);
    assert(warehouse_pickup->pickup_glow_present);
    assert(runtime.scene().entity(pickup_binding->entities.front())
        ->pickup_update_calls == 1);

    // The retail type-12 update writes the live asset's +0x24 color field;
    // exercise the explicit palette-mode service and verify the scene bridge
    // receives that asset-side mutation.
    runtime.state().set_special_runtime_palette_mask(120, 0);
    runtime.state().set_special_runtime_animation_mode(
        opentony::trg::TriggerSpecialAnimationMode::PaletteTriangle);
    runtime.tick(0);
    const auto* animated_special_entity = runtime.scene().entity(
        warehouse_binding->entities.front());
    assert(animated_special_entity != nullptr);
    assert(animated_special_entity->special_asset_marker
        == runtime.state().object(120)->special_asset_marker);

    opentony::runtime::LevelFrameScheduler scheduler(runtime);
    Observer observer;
    scheduler.step(16, opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left), &observer);
    assert(scheduler.frame_index() == 1);
    assert(observer.input_calls == 1);
    assert(observer.tick_calls == 1);

    const auto* point = runtime.triggers().command_point(141);
    assert(point != nullptr);
    const std::size_t event_count_before_visible = runtime.state().events().size();
    runtime.pulse_node(141);
    assert(runtime.triggers().command_point(141)->pulse_count == 1);
    const auto* visible_target = runtime.state().object(0x00cf);
    assert(visible_target != nullptr);
    assert(visible_target->visible_commanded);
    assert((visible_target->flags & 0x0041U) == 0);
    assert(runtime.state().events().size() > event_count_before_visible);
    const bool saw_cf_visible = std::any_of(
        runtime.state().events().begin()
            + static_cast<std::ptrdiff_t>(event_count_before_visible),
        runtime.state().events().end(),
        [](const opentony::trg::TriggerEvent& event) {
            return event.kind == opentony::trg::TriggerEvent::Kind::Visible
                && event.source_node == 141
                && event.target_node == 0x00cf
                && event.opcode == 0x000d
                && event.value == 1;
        });
    assert(saw_cf_visible);

    std::cout << "level runtime ok\n";
}
