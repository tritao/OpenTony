#include "level_runtime.hpp"
#include "../runtime/level_frame.hpp"
#include "../runtime/psx_collision_probe.hpp"

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
    runtime.pulse_node(141);
    assert(runtime.triggers().command_point(141)->pulse_count == 1);

    std::cout << "level runtime ok\n";
}
