#pragma once

#include "level_trigger_state.hpp"
#include "../assets/psx_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace opentony::trg {

enum class LevelSceneEntityKind : std::uint8_t {
    StaticScene,
    TriggerObject,
    Pickup,
    LinkedObject,
    Special,
};

struct LevelSceneEntity {
    std::size_t entity{};
    LevelSceneEntityKind kind{LevelSceneEntityKind::StaticScene};
    std::size_t source_node{CommandPointRuntime::npos};
    std::vector<std::size_t> source_nodes;
    std::size_t psx_object_index{CommandPointRuntime::npos};
    std::size_t model_index{CommandPointRuntime::npos};
    std::uint32_t model_name{};
    std::uint16_t subtype{};
    TriggerSpawnFamily spawn_family{TriggerSpawnFamily::Unknown};
    std::string factory_resource;
    std::uint32_t factory_model_selector{};
    bool has_factory_model_selector{};
    std::vector<std::uint8_t> spawn_options;
    std::vector<std::byte> factory_node_bytes;
    std::uint32_t factory_cursor_offset{};
    bool has_factory_cursor_offset{};
    bool has_spawn_option_2{};
    bool has_spawn_option_4{};
    bool factory_requires_environment_registration{};
    bool factory_clears_object_flag_2{};
    bool factory_sets_object_flag_4{};
    std::uint16_t trigger_flags{};
    std::uint8_t trigger_state{};
    std::uint8_t trigger_mode{};
    bool has_trigger_runtime{};
    std::uint8_t special_runtime_owner{};
    std::uint32_t special_runtime_control{};
    bool has_special_runtime_context{};
    bool has_special_runtime{};
    bool special_runtime_active{};
    std::string factory_asset_path;
    bool factory_asset_available{};
    bool factory_asset_loaded{};
    std::size_t factory_asset_object_count{};
    std::size_t factory_asset_model_count{};
    std::array<std::int32_t, 3> position{};
    std::array<std::uint16_t, 3> orientation{};
    bool has_orientation{};
    std::uint32_t asset_flags{};
    std::uint8_t special_asset_flags_or{};
    std::uint32_t special_asset_marker{};
    bool has_special_asset_state{};
    std::uint16_t gameplay_flags{};
    bool active{true};
    bool suspended{};
    bool alive{true};
    bool killed{};
    bool visible_commanded{};
};

struct LevelSceneBinding {
    std::size_t trigger_node{};
    std::size_t model_index{CommandPointRuntime::npos};
    std::uint32_t model_name{};
    bool bound_to_psx{};
    std::vector<std::size_t> entities;
};

// Renderer-independent composition of the two level-side asset systems. It
// deliberately does not decide how models are uploaded or how collisions are
// built; it gives those systems stable entity IDs and exact source mappings.
class LevelSceneRegistry final {
public:
    void build(const LevelTriggerState& state, const assets::PsxArchive& archive);
    void resolve_factory_assets(const assets::PsxAssetCatalog& catalog);
    void sync(const LevelTriggerState& state);

    [[nodiscard]] const std::vector<LevelSceneEntity>& entities() const noexcept {
        return entities_;
    }
    [[nodiscard]] const std::vector<LevelSceneBinding>& bindings() const noexcept {
        return bindings_;
    }
    [[nodiscard]] const LevelSceneBinding* binding(std::size_t trigger_node) const noexcept;
    [[nodiscard]] const LevelSceneEntity* entity(std::size_t entity_index) const noexcept;
    [[nodiscard]] std::size_t static_entity_count() const noexcept { return static_entity_count_; }
    [[nodiscard]] std::size_t trigger_entity_count() const noexcept {
        return entities_.size() - static_entity_count_;
    }
    [[nodiscard]] std::size_t bound_trigger_count() const noexcept { return bound_trigger_count_; }
    [[nodiscard]] std::size_t unresolved_trigger_count() const noexcept {
        return unresolved_trigger_count_;
    }

private:
    std::vector<LevelSceneEntity> entities_;
    std::vector<LevelSceneBinding> bindings_;
    std::size_t static_entity_count_{};
    std::size_t bound_trigger_count_{};
    std::size_t unresolved_trigger_count_{};

    [[nodiscard]] static LevelSceneEntityKind kind_for(TriggerObjectKind kind) noexcept;
    [[nodiscard]] static const TriggerObjectState* find_state(
        const LevelTriggerState& state,
        std::size_t node) noexcept;
    static void copy_source_metadata(
        LevelSceneEntity& entity,
        const TriggerObjectState& source);
    void sync_binding(const LevelTriggerState& state, const LevelSceneBinding& binding);
};

} // namespace opentony::trg
