#pragma once

#include "level_scene_registry.hpp"
#include "camera_point_registry.hpp"
#include "rail_runtime.hpp"
#include "powerup_runtime.hpp"
#include "traffic_runtime.hpp"
#include "trigger_factory_runtime.hpp"
#include "trg_runtime.hpp"
#include "../assets/psx_collision.hpp"
#include "../assets/psx_bits_runtime.hpp"
#include "../assets/pre_catalog.hpp"
#include "../assets/pkr_asset.hpp"
#include "../assets/pc_texture_runtime.hpp"
#include "../assets/psx_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::trg {

struct LevelResourceBinding {
    TriggerResourceRequest request;
    std::string asset_path;
    bool asset_available{};
    bool asset_loaded{};
    std::size_t asset_object_count{};
    std::size_t asset_model_count{};
};

// End-to-end, renderer-independent level runtime. This is the integration
// boundary used by the eventual game loop: TRG owns script execution, the
// state service owns gameplay-visible mutations, and the scene registry owns
// stable PSX-backed entity IDs.
class LevelRuntime final {
public:
    LevelRuntime(
        const std::string& trg_path,
        const std::string& psx_path,
        const std::string& asset_root = {});

    // Package-backed counterpart of the file-path constructor. The package
    // is only needed while the owned TRG/PSX images are decoded; the runtime
    // keeps those parsed values, matching the game's allocated read buffer.
    LevelRuntime(
        const assets::PkrArchive& package,
        std::string_view trg_entry,
        std::string_view psx_entry,
        const std::string& asset_root = {});

    LevelRuntime(const LevelRuntime&) = delete;
    LevelRuntime& operator=(const LevelRuntime&) = delete;
    LevelRuntime(LevelRuntime&&) = delete;
    LevelRuntime& operator=(LevelRuntime&&) = delete;

    // The ordering mirrors the retail load path. The optional gap table is
    // retained by the state service and is consulted by opcode 0xc9.
    void initialize(bool two_player = false, const GapTable* gap_table = nullptr);
    void tick(std::uint32_t milliseconds);
    void pulse_node(std::size_t node);
    void pulse_checksum(std::uint32_t checksum);
    void execute_restart(std::size_t node);
    void execute_restart(std::string_view name);

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }
    [[nodiscard]] const TrgFile& file() const noexcept { return runtime_.file(); }
    [[nodiscard]] const TriggerRuntime& triggers() const noexcept { return runtime_; }
    [[nodiscard]] TriggerRuntime& triggers() noexcept { return runtime_; }
    [[nodiscard]] const LevelTriggerState& state() const noexcept { return state_; }
    [[nodiscard]] LevelTriggerState& state() noexcept { return state_; }
    [[nodiscard]] const assets::PsxArchive& scene_asset() const noexcept { return archive_; }
    [[nodiscard]] const assets::PsxRuntimeEnvironment& scene_runtime() const noexcept {
        return scene_runtime_;
    }
    [[nodiscard]] const assets::PcTextureRuntime* texture_runtime() const noexcept {
        return texture_runtime_.has_value() ? &*texture_runtime_ : nullptr;
    }
    [[nodiscard]] const assets::PcTextureRuntime* item_texture_runtime() const noexcept {
        return item_texture_runtime_.has_value() ? &*item_texture_runtime_ : nullptr;
    }
    [[nodiscard]] const assets::PcTextureRuntime* medal_texture_runtime() const noexcept {
        return medal_texture_runtime_.has_value() ? &*medal_texture_runtime_ : nullptr;
    }
    [[nodiscard]] const CameraPointRegistry& camera_points() const noexcept {
        return camera_points_;
    }
    [[nodiscard]] const RailRuntimeList& rails() const noexcept {
        return rails_;
    }
    [[nodiscard]] const PowerupRuntimeList& powerups() const noexcept {
        return powerups_;
    }
    [[nodiscard]] const TrafficRuntimeList& traffic() const noexcept {
        return traffic_;
    }
    [[nodiscard]] const TriggerFactoryRuntimeList& factory_objects() const noexcept {
        return factory_objects_;
    }
    [[nodiscard]] const assets::PsxCollisionWorld& collision() const noexcept {
        return collision_;
    }
    [[nodiscard]] const LevelSceneRegistry& scene() const noexcept { return scene_; }
    [[nodiscard]] LevelSceneRegistry& scene() noexcept { return scene_; }
    [[nodiscard]] const std::vector<LevelResourceBinding>& resources() const noexcept {
        return resources_;
    }
    [[nodiscard]] const assets::PsxAssetCatalog* asset_catalog() const noexcept;
    [[nodiscard]] const assets::PreAssetCatalog* pre_catalog() const noexcept;
    [[nodiscard]] const assets::PsxBitsRuntime* bits_runtime() const noexcept {
        return bits_runtime_.has_value() ? &*bits_runtime_ : nullptr;
    }
    [[nodiscard]] const assets::PsxRuntimeEnvironment* item_runtime() const noexcept {
        return item_runtime_.has_value() ? &*item_runtime_ : nullptr;
    }
    [[nodiscard]] const assets::PsxRuntimeEnvironment* medal_runtime() const noexcept {
        return medal_runtime_.has_value() ? &*medal_runtime_ : nullptr;
    }

private:
    LevelTriggerState state_;
    TriggerRuntime runtime_;
    assets::PsxArchive archive_;
    assets::PsxRuntimeEnvironment scene_runtime_;
    assets::PsxCollisionWorld collision_;
    std::string asset_root_;
    std::optional<assets::PsxAssetCatalog> catalog_;
    std::optional<assets::PreAssetCatalog> pre_catalog_;
    std::optional<assets::PsxBitsRuntime> bits_runtime_;
    std::optional<assets::PsxRuntimeEnvironment> item_runtime_;
    std::optional<assets::PsxRuntimeEnvironment> medal_runtime_;
    std::optional<assets::PsxArchive> bits_archive_;
    LevelSceneRegistry scene_;
    CameraPointRegistry camera_points_;
    RailRuntimeList rails_;
    PowerupRuntimeList powerups_;
    TrafficRuntimeList traffic_;
    TriggerFactoryRuntimeList factory_objects_;
    const assets::PkrArchive* package_{};
    std::optional<assets::PsxArchive> item_archive_;
    std::optional<assets::PsxArchive> medal_archive_;
    std::optional<assets::PcTextureRuntime> texture_runtime_;
    std::optional<assets::PcTextureRuntime> item_texture_runtime_;
    std::optional<assets::PcTextureRuntime> medal_texture_runtime_;
    std::vector<LevelResourceBinding> resources_;
    bool initialized_{};

    void refresh_resource_bindings();
};

} // namespace opentony::trg
