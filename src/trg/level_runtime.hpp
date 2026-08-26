#pragma once

#include "level_scene_registry.hpp"
#include "trg_runtime.hpp"
#include "../assets/psx_collision.hpp"
#include "../assets/pre_catalog.hpp"

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

private:
    LevelTriggerState state_;
    TriggerRuntime runtime_;
    assets::PsxArchive archive_;
    assets::PsxCollisionWorld collision_;
    std::optional<assets::PsxAssetCatalog> catalog_;
    std::optional<assets::PreAssetCatalog> pre_catalog_;
    LevelSceneRegistry scene_;
    std::vector<LevelResourceBinding> resources_;
    bool initialized_{};

    void refresh_resource_bindings();
};

} // namespace opentony::trg
