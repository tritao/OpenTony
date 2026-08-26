#include "level_runtime.hpp"
#include "../runtime/level_frame.hpp"
#include "../runtime/psx_collision_probe.hpp"
#include "../assets/pkr_asset.hpp"

#include <algorithm>
#include <array>
#include "tests/test_check.hpp"
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
        CHECK(frame == 1);
        CHECK(input.pressed(opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left)));
        ++input_calls;
    }

    void on_level_tick(
        std::uint64_t frame,
        std::uint32_t milliseconds,
        const opentony::runtime::InputState&,
        const opentony::trg::LevelRuntime& level) override {
        CHECK(frame == 1);
        CHECK(milliseconds == 16);
        CHECK(level.state().time_ms() == 32);
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
    CHECK(!runtime.initialized());
    runtime.initialize();
    CHECK(runtime.initialized());
    CHECK(runtime.file().nodes().size() == 313);
    CHECK(runtime.triggers().command_points().size() == 109);
    CHECK(runtime.scene().static_entity_count() == 252);
    CHECK(runtime.scene().bound_trigger_count() == 95);
    CHECK(runtime.scene_runtime().object_count() == 252);
    CHECK(runtime.scene_runtime().model_count() == 288);
    CHECK(runtime.scene_runtime().allocation_size() == 4 + 252 * 0x4c);
    CHECK(runtime.scene_runtime().object_record_offset(17) == 4 + 17 * 0x4c);
    CHECK(runtime.scene_runtime().object(17).model_index() == 17);
    CHECK(&runtime.scene_runtime().model_for_object(17)
        == &runtime.scene_asset().models()[17]);
    CHECK(runtime.camera_points().entries().size() == 9);
    CHECK(runtime.rails().records().size() == 65);
    CHECK(runtime.powerups().records().size() == 12);
    CHECK(runtime.traffic().records().empty());
    CHECK(runtime.factory_objects().records().size() == 53);
    const auto* warehouse_factory = runtime.factory_objects().record_for_node(2);
    CHECK(warehouse_factory != nullptr);
    CHECK(warehouse_factory->kind()
        == opentony::trg::TriggerFactoryRuntimeKind::ObjectCb);
    CHECK(warehouse_factory->allocation_size() == 0x1f4);
    CHECK(warehouse_factory->position() == runtime.file().node_position(2));
    const auto* warehouse_object192 = runtime.factory_objects().record_for_node(131);
    CHECK(warehouse_object192 != nullptr);
    CHECK(warehouse_object192->kind()
        == opentony::trg::TriggerFactoryRuntimeKind::Object192);
    CHECK(warehouse_object192->allocation_size() == 0x218);
    const auto* warehouse_powerup = runtime.powerups().record_for_node(17);
    CHECK(warehouse_powerup != nullptr);
    CHECK(warehouse_powerup->subtype() == 6);
    CHECK(warehouse_powerup->resource() == "items");
    CHECK(warehouse_powerup->model_name_checksum() == 0x2ebf22caU);
    CHECK(warehouse_powerup->model_index().has_value());
    CHECK(*warehouse_powerup->model_index() == 5);
    CHECK(warehouse_powerup->position() == runtime.file().node_position(17));
    CHECK(warehouse_powerup->raw_record().size() == 0x100);
    CHECK(runtime.scene_runtime().materials().records().size() == 89);
    const auto* material_50 = runtime.scene_runtime().materials()
        .material_for_texture(50);
    const auto* material_51 = runtime.scene_runtime().materials()
        .material_for_texture(51);
    CHECK(material_50 != nullptr);
    CHECK(material_51 != nullptr);
    CHECK(material_50->checksum() == 0xd783cf21U);
    CHECK(material_51->checksum() == 0x288ca4c4U);
    CHECK(material_50->reference_count() > 0);
    CHECK(material_51->reference_count() > 0);
    CHECK(material_50->raw_record().size() == 0x2c);
    CHECK(runtime.texture_runtime() != nullptr);
    std::size_t warehouse_material_index =
        opentony::trg::CommandPointRuntime::npos;
    for (std::size_t index = 0;
         index < runtime.scene_runtime().materials().records().size();
         ++index) {
        if (runtime.scene_runtime().materials().record(index).checksum()
            == 0x032bbb26U) {
            warehouse_material_index = index;
            break;
        }
    }
    CHECK(warehouse_material_index != opentony::trg::CommandPointRuntime::npos);
    const auto* warehouse_texture = runtime.texture_runtime()
        ->record_for_material(warehouse_material_index, 0x032bbb26U);
    CHECK(warehouse_texture != nullptr);
    CHECK(warehouse_texture->source_kind()
        == opentony::assets::PcTextureSourceKind::ExternalBitmap);
    CHECK(warehouse_texture->image().width == 128);
    CHECK(warehouse_texture->image().height == 128);
    CHECK(warehouse_texture->ready());
    CHECK(runtime.bits_runtime() != nullptr);
    CHECK(runtime.bits_runtime()->groups().size() == 5);
    CHECK(runtime.bits_runtime()->find("shadow") != nullptr);
    CHECK(runtime.bits_runtime()->find("smoke") != nullptr);
    CHECK(runtime.collision().grids().size() == 1);
    CHECK(runtime.collision().referenced_object_count() == 252);
    CHECK(runtime.collision().face_count() == 1345);
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
    CHECK(face_hit.has_value());
    CHECK(face_hit->object_index == first_face.object_index);
    CHECK(face_hit->surface_flags == first_face.surface_flags);
    CHECK(runtime.asset_catalog() != nullptr);
    CHECK(runtime.pre_catalog() != nullptr);
    CHECK(runtime.pre_catalog()->size() == 19);
    CHECK(runtime.pre_catalog()->load("LEVEL").entries().size() > 0);
    CHECK(runtime.resources().size() == 2);
    for (const auto& resource : runtime.resources()) {
        CHECK(resource.asset_available);
        CHECK(resource.asset_loaded);
        if (resource.request.name == "SkWare_O") {
            CHECK(resource.asset_object_count == 25);
            CHECK(resource.asset_model_count == 25);
        } else {
            // SKWARE_L.PSX is a valid empty level-side resource in this
            // build; successful parsing is the contract, not nonzero counts.
            CHECK(resource.asset_object_count == 0);
            CHECK(resource.asset_model_count == 0);
        }
    }
    CHECK(runtime.asset_catalog()->contains("ITEMS"));
    CHECK(runtime.asset_catalog()->contains("SKMEDALS"));
    const auto* warehouse_pickup = runtime.state().object(17);
    CHECK(warehouse_pickup != nullptr);
    CHECK(warehouse_pickup->node_type == 5);
    CHECK(warehouse_pickup->subtype == 6);
    CHECK(warehouse_pickup->pickup_resource == "items");
    CHECK(warehouse_pickup->pickup_model_checksum == 0x2ebf22ca);
    CHECK(warehouse_pickup->pickup_model_resolved);
    CHECK(warehouse_pickup->pickup_model_index == 5);
    CHECK(warehouse_pickup->pickup_visual_state_d1 == 1);
    CHECK(warehouse_pickup->pickup_motion_state_d2 == 0);
    CHECK(warehouse_pickup->pickup_motion_substate_d3 == 0);
    CHECK(!warehouse_pickup->pickup_glow_present);
    CHECK(warehouse_pickup->pickup_update_calls == 0);
    CHECK(warehouse_pickup->position[0] == 7673 * 4096);
    CHECK(warehouse_pickup->position[1] == -427 * 4096);
    CHECK(warehouse_pickup->position[2] == 12132 * 4096);
    const auto* pickup_binding = runtime.scene().binding(17);
    CHECK(pickup_binding != nullptr);
    CHECK(!pickup_binding->bound_to_psx);
    const auto* pickup_entity = runtime.scene().entity(pickup_binding->entities.front());
    CHECK(pickup_entity != nullptr);
    CHECK(pickup_entity->kind == opentony::trg::LevelSceneEntityKind::Pickup);
    CHECK(pickup_entity->pickup_resource == "items");
    CHECK(pickup_entity->pickup_model_checksum == 0x2ebf22ca);
    CHECK(pickup_entity->pickup_model_resolved);
    CHECK(pickup_entity->pickup_model_index == 5);
    CHECK(pickup_entity->factory_asset_loaded);
    CHECK(pickup_entity->factory_asset_model_count == 13);
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
    CHECK(pickup_count == 12);
    // The recovered retail table is intentionally partial: unclassified
    // Warehouse subtypes remain source-visible rather than receiving a guess.
    CHECK(resolved_pickup_count > 0);
    std::size_t loaded_factory_assets = 0;
    for (const auto& entity : runtime.scene().entities()) {
        if (!entity.factory_resource.empty()) {
            CHECK(entity.factory_asset_loaded == entity.factory_asset_available);
            if (entity.factory_asset_loaded) {
                ++loaded_factory_assets;
                CHECK(entity.factory_asset_model_count > 0);
            }
        }
    }
    CHECK(loaded_factory_assets == 0);

    // Warehouse node 120 is a type-12 linked special object. Its TRG key
    // resolves into the SKWARE model-name namespace, and a pulse carries the
    // verified FUN_004bdc40 asset-side writes into the scene binding.
    const auto* warehouse_special = runtime.state().object(120);
    CHECK(warehouse_special != nullptr);
    CHECK(warehouse_special->has_special_runtime);
    CHECK(warehouse_special->special_runtime_allocation_size == 0x18);
    CHECK(warehouse_special->special_runtime_vtable == 0x0051982c);
    CHECK(warehouse_special->special_runtime_list
        == opentony::trg::TriggerSpecialRuntimeList::Type12Type14);
    const auto* warehouse_binding = runtime.scene().binding(120);
    CHECK(warehouse_binding != nullptr);
    CHECK(warehouse_binding->bound_to_psx);
    runtime.pulse_node(120);
    warehouse_special = runtime.state().object(120);
    CHECK(warehouse_special->special_runtime_active);
    CHECK(warehouse_special->special_asset_flags_or == 4);
    CHECK(warehouse_special->special_asset_marker == 0x202020);
    CHECK(runtime.scene().entity(warehouse_binding->entities.front())
        ->has_special_asset_state);
    CHECK(runtime.scene().entity(warehouse_binding->entities.front())
        ->has_special_runtime);
    CHECK(runtime.scene().entity(warehouse_binding->entities.front())
        ->special_runtime_allocation_size == 0x18);
    CHECK(runtime.scene().entity(warehouse_binding->entities.front())
        ->special_runtime_vtable == 0x0051982c);
    CHECK(runtime.scene().entity(warehouse_binding->entities.front())
        ->special_runtime_active);

    // SKWARE node 2 is a recovered subtype-0xcb factory bridge. Node 0xcf
    // in the visible sample is a separate type-2 linked scene object.
    const auto* warehouse_object = runtime.state().object(2);
    CHECK(warehouse_object != nullptr);
    CHECK(warehouse_object->spawn_family
        == opentony::trg::TriggerSpawnFamily::ObjectCb);
    CHECK(warehouse_object->factory_allocation_size == 0x1f4);
    CHECK(warehouse_object->factory_vtable == 0x005183b0);
    CHECK(warehouse_object->factory_list
        == opentony::trg::TriggerFactoryList::CommonObject);
    CHECK(warehouse_object->has_factory_initial_activation_byte);
    CHECK(warehouse_object->factory_initial_activation_byte == 1);
    const auto* warehouse_object_binding = runtime.scene().binding(2);
    CHECK(warehouse_object_binding != nullptr);
    const auto* warehouse_object_entity = runtime.scene().entity(
        warehouse_object_binding->entities.front());
    CHECK(warehouse_object_entity != nullptr);
    CHECK(warehouse_object_entity->factory_allocation_size == 0x1f4);
    CHECK(warehouse_object_entity->factory_vtable == 0x005183b0);

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
            CHECK(entity.factory_asset_loaded);
            CHECK(entity.factory_asset_object_count > 0);
            CHECK(entity.factory_asset_model_count > 0);
            ++special_factory_assets;
        }
        CHECK(special_factory_assets == 10);
    }

    runtime.tick(16);
    CHECK(runtime.state().time_ms() == 16);
    warehouse_pickup = runtime.state().object(17);
    CHECK(warehouse_pickup->pickup_update_calls == 1);
    CHECK(warehouse_pickup->pickup_glow_present);
    CHECK(runtime.scene().entity(pickup_binding->entities.front())
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
    CHECK(animated_special_entity != nullptr);
    CHECK(animated_special_entity->special_asset_marker
        == runtime.state().object(120)->special_asset_marker);

    opentony::runtime::LevelFrameScheduler scheduler(runtime);
    Observer observer;
    scheduler.step(16, opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left), &observer);
    CHECK(scheduler.frame_index() == 1);
    CHECK(observer.input_calls == 1);
    CHECK(observer.tick_calls == 1);

    const auto* point = runtime.triggers().command_point(141);
    CHECK(point != nullptr);
    const std::size_t event_count_before_visible = runtime.state().events().size();
    runtime.pulse_node(141);
    CHECK(runtime.triggers().command_point(141)->pulse_count == 1);
    const auto* visible_target = runtime.state().object(0x00cf);
    CHECK(visible_target != nullptr);
    CHECK(visible_target->visible_commanded);
    CHECK((visible_target->flags & 0x0041U) == 0);
    CHECK(runtime.state().events().size() > event_count_before_visible);
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
    CHECK(saw_cf_visible);

    const std::string package_path =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(package_path)) {
        const auto package = opentony::assets::PkrArchive::load(package_path);
        opentony::trg::LevelRuntime packaged(
            package, "data/SKWARE_T.TRG", "data/SKWARE.PSX", asset_root);
        packaged.initialize();
        CHECK(packaged.file().nodes().size() == 313);
        CHECK(packaged.scene_runtime().object_count() == 252);
        CHECK(packaged.scene_runtime().model_count() == 288);
        CHECK(packaged.scene().bound_trigger_count() == 95);

        // The package-only constructor must retain the same item-region
        // runtime objects even when no loose-file catalog is supplied.
        opentony::trg::LevelRuntime packaged_only(
            package, "data/SKWARE_T.TRG", "data/SKWARE.PSX");
        packaged_only.initialize();
        CHECK(packaged_only.item_runtime() != nullptr);
        CHECK(packaged_only.item_runtime()->model_count() == 13);
        CHECK(packaged_only.bits_runtime() != nullptr);
        CHECK(packaged_only.bits_runtime()->groups().size() == 5);
        CHECK(packaged_only.resources().size() == 2);
        for (const auto& resource : packaged_only.resources()) {
            CHECK(resource.asset_available);
            CHECK(!resource.asset_path.empty());
        }
        const auto* packaged_powerup =
            packaged_only.powerups().record_for_node(17);
        CHECK(packaged_powerup != nullptr);
        CHECK(packaged_powerup->model_index().has_value());
        CHECK(*packaged_powerup->model_index() == 5);
    }

    std::cout << "level runtime ok\n";
}
