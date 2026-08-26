#include "level_runtime.hpp"
#include "../runtime/level_frame.hpp"
#include "../runtime/psx_collision_probe.hpp"
#include "../assets/pkr_asset.hpp"

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
    assert(runtime.scene_runtime().object_count() == 252);
    assert(runtime.scene_runtime().model_count() == 288);
    assert(runtime.scene_runtime().allocation_size() == 4 + 252 * 0x4c);
    assert(runtime.scene_runtime().object_record_offset(17) == 4 + 17 * 0x4c);
    assert(runtime.scene_runtime().object(17).model_index() == 17);
    assert(&runtime.scene_runtime().model_for_object(17)
        == &runtime.scene_asset().models()[17]);
    assert(runtime.camera_points().entries().size() == 9);
    assert(runtime.rails().records().size() == 65);
    assert(runtime.powerups().records().size() == 12);
    assert(runtime.traffic().records().empty());
    assert(runtime.factory_objects().records().size() == 53);
    const auto* warehouse_factory = runtime.factory_objects().record_for_node(2);
    assert(warehouse_factory != nullptr);
    assert(warehouse_factory->kind()
        == opentony::trg::TriggerFactoryRuntimeKind::ObjectCb);
    assert(warehouse_factory->allocation_size() == 0x1f4);
    assert(warehouse_factory->position() == runtime.file().node_position(2));
    const auto* warehouse_object192 = runtime.factory_objects().record_for_node(131);
    assert(warehouse_object192 != nullptr);
    assert(warehouse_object192->kind()
        == opentony::trg::TriggerFactoryRuntimeKind::Object192);
    assert(warehouse_object192->allocation_size() == 0x218);
    const auto* warehouse_powerup = runtime.powerups().record_for_node(17);
    assert(warehouse_powerup != nullptr);
    assert(warehouse_powerup->subtype() == 6);
    assert(warehouse_powerup->resource() == "items");
    assert(warehouse_powerup->model_name_checksum() == 0x2ebf22caU);
    assert(warehouse_powerup->model_index().has_value());
    assert(*warehouse_powerup->model_index() == 5);
    assert(warehouse_powerup->position() == runtime.file().node_position(17));
    assert(warehouse_powerup->raw_record().size() == 0x100);
    assert(runtime.scene_runtime().materials().records().size() == 89);
    const auto* material_50 = runtime.scene_runtime().materials()
        .material_for_texture(50);
    const auto* material_51 = runtime.scene_runtime().materials()
        .material_for_texture(51);
    assert(material_50 != nullptr);
    assert(material_51 != nullptr);
    assert(material_50->checksum() == 0xd783cf21U);
    assert(material_51->checksum() == 0x288ca4c4U);
    assert(material_50->reference_count() > 0);
    assert(material_51->reference_count() > 0);
    assert(material_50->raw_record().size() == 0x2c);
    assert(runtime.texture_runtime() != nullptr);
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
    assert(warehouse_material_index != opentony::trg::CommandPointRuntime::npos);
    const auto* warehouse_texture = runtime.texture_runtime()
        ->record_for_material(warehouse_material_index, 0x032bbb26U);
    assert(warehouse_texture != nullptr);
    assert(warehouse_texture->source_kind()
        == opentony::assets::PcTextureSourceKind::ExternalBitmap);
    assert(warehouse_texture->image().width == 128);
    assert(warehouse_texture->image().height == 128);
    assert(warehouse_texture->ready());
    assert(runtime.bits_runtime() != nullptr);
    assert(runtime.bits_runtime()->groups().size() == 5);
    assert(runtime.bits_runtime()->find("shadow") != nullptr);
    assert(runtime.bits_runtime()->find("smoke") != nullptr);
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
    }
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
        ->special_runtime_active);

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

    const std::string package_path =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(package_path)) {
        const auto package = opentony::assets::PkrArchive::load(package_path);
        opentony::trg::LevelRuntime packaged(
            package, "data/SKWARE_T.TRG", "data/SKWARE.PSX", asset_root);
        packaged.initialize();
        assert(packaged.file().nodes().size() == 313);
        assert(packaged.scene_runtime().object_count() == 252);
        assert(packaged.scene_runtime().model_count() == 288);
        assert(packaged.scene().bound_trigger_count() == 95);

        // The package-only constructor must retain the same item-region
        // runtime objects even when no loose-file catalog is supplied.
        opentony::trg::LevelRuntime packaged_only(
            package, "data/SKWARE_T.TRG", "data/SKWARE.PSX");
        packaged_only.initialize();
        assert(packaged_only.item_runtime() != nullptr);
        assert(packaged_only.item_runtime()->model_count() == 13);
        assert(packaged_only.bits_runtime() != nullptr);
        assert(packaged_only.bits_runtime()->groups().size() == 5);
        assert(packaged_only.resources().size() == 2);
        for (const auto& resource : packaged_only.resources()) {
            assert(resource.asset_available);
            assert(!resource.asset_path.empty());
        }
        const auto* packaged_powerup =
            packaged_only.powerups().record_for_node(17);
        assert(packaged_powerup != nullptr);
        assert(packaged_powerup->model_index().has_value());
        assert(*packaged_powerup->model_index() == 5);
    }

    std::cout << "level runtime ok\n";
}
