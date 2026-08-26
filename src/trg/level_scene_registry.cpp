#include "level_scene_registry.hpp"

#include <algorithm>

namespace opentony::trg {

void LevelSceneRegistry::build(
    const LevelTriggerState& state,
    const assets::PsxArchive& archive) {
    entities_.clear();
    bindings_.clear();
    static_entity_count_ = 0;
    bound_trigger_count_ = 0;
    unresolved_trigger_count_ = 0;

    entities_.reserve(archive.objects().size() + state.objects().size());
    for (std::size_t psx_index = 0; psx_index < archive.objects().size(); ++psx_index) {
        const assets::PsxObject& source = archive.objects()[psx_index];
        LevelSceneEntity entity{};
        entity.entity = entities_.size();
        entity.psx_object_index = psx_index;
        entity.model_index = source.model_index;
        entity.position = source.position;
        entity.asset_flags = source.flags;
        if (source.model_index < archive.model_names().size()) {
            entity.model_name = archive.model_names()[source.model_index];
        }
        entities_.push_back(std::move(entity));
    }
    static_entity_count_ = entities_.size();

    for (const TriggerObjectState& source : state.objects()) {
        if (source.kind == TriggerObjectKind::CommandPoint) {
            continue;
        }
        LevelSceneBinding binding{};
        binding.trigger_node = source.node;
        binding.model_name = source.link_key;

        std::size_t model_index = source.asset_model_index;
        if (model_index == CommandPointRuntime::npos && source.link_key != 0) {
            const auto found = std::find(
                archive.model_names().begin(),
                archive.model_names().end(),
                source.link_key);
            if (found != archive.model_names().end()) {
                model_index = static_cast<std::size_t>(
                    std::distance(archive.model_names().begin(), found));
            }
        }
        binding.model_index = model_index;
        binding.bound_to_psx = model_index != CommandPointRuntime::npos;
        if (binding.bound_to_psx) {
            for (std::size_t psx_index = 0; psx_index < archive.objects().size(); ++psx_index) {
                if (archive.objects()[psx_index].model_index != model_index) {
                    continue;
                }
                binding.entities.push_back(psx_index);
                LevelSceneEntity& entity = entities_[psx_index];
                entity.source_nodes.push_back(source.node);
                if (entity.source_node == CommandPointRuntime::npos) {
                    entity.source_node = source.node;
                }
                entity.kind = kind_for(source.kind);
                entity.model_index = model_index;
                entity.model_name = source.link_key;
                entity.subtype = source.subtype;
                entity.spawn_family = source.spawn_family;
                copy_source_metadata(entity, source);
            }
            if (!binding.entities.empty()) {
                ++bound_trigger_count_;
            } else {
                binding.bound_to_psx = false;
            }
        }

        if (!binding.bound_to_psx) {
            ++unresolved_trigger_count_;
            LevelSceneEntity entity{};
            entity.entity = entities_.size();
            entity.kind = kind_for(source.kind);
            entity.source_node = source.node;
            entity.source_nodes.push_back(source.node);
            entity.subtype = source.subtype;
            entity.spawn_family = source.spawn_family;
            entity.factory_resource = source.factory_resource;
            entity.factory_model_selector = source.factory_model_selector;
            entity.has_factory_model_selector = source.has_factory_model_selector;
            copy_source_metadata(entity, source);
            entity.position = source.position;
            entity.orientation = source.orientation;
            entity.has_orientation = source.has_orientation;
            entity.gameplay_flags = source.flags;
            entity.special_asset_flags_or = source.special_asset_flags_or;
            entity.special_asset_marker = source.special_asset_marker;
            entity.has_special_asset_state = source.has_special_asset_state;
            entity.active = source.active;
            entity.suspended = source.suspended;
            entity.alive = source.alive;
            entity.killed = source.killed;
            entity.visible_commanded = source.visible_commanded;
            binding.entities.push_back(entity.entity);
            entities_.push_back(std::move(entity));
        }
        bindings_.push_back(std::move(binding));
    }
    sync(state);
}

void LevelSceneRegistry::sync(const LevelTriggerState& state) {
    for (const LevelSceneBinding& binding : bindings_) {
        sync_binding(state, binding);
    }
}

void LevelSceneRegistry::resolve_factory_assets(const assets::PsxAssetCatalog& catalog) {
    for (LevelSceneEntity& entity : entities_) {
        entity.factory_asset_path.clear();
        entity.factory_asset_available = false;
        entity.factory_asset_loaded = false;
        entity.factory_asset_object_count = 0;
        entity.factory_asset_model_count = 0;
        if (entity.factory_resource.empty()) {
            continue;
        }
        if (const std::string* path = catalog.path_for(entity.factory_resource);
            path != nullptr) {
            entity.factory_asset_path = *path;
            entity.factory_asset_available = true;
            const assets::PsxArchive& archive = catalog.load(entity.factory_resource);
            entity.factory_asset_loaded = true;
            entity.factory_asset_object_count = archive.objects().size();
            entity.factory_asset_model_count = archive.models().size();
        }
    }
}

const LevelSceneBinding* LevelSceneRegistry::binding(std::size_t trigger_node) const noexcept {
    const auto found = std::find_if(
        bindings_.begin(),
        bindings_.end(),
        [trigger_node](const LevelSceneBinding& value) {
            return value.trigger_node == trigger_node;
        });
    return found == bindings_.end() ? nullptr : &*found;
}

const LevelSceneEntity* LevelSceneRegistry::entity(std::size_t entity_index) const noexcept {
    return entity_index < entities_.size() ? &entities_[entity_index] : nullptr;
}

LevelSceneEntityKind LevelSceneRegistry::kind_for(TriggerObjectKind kind) noexcept {
    switch (kind) {
    case TriggerObjectKind::Object:
        return LevelSceneEntityKind::TriggerObject;
    case TriggerObjectKind::Pickup:
        return LevelSceneEntityKind::Pickup;
    case TriggerObjectKind::LinkedObject:
        return LevelSceneEntityKind::LinkedObject;
    case TriggerObjectKind::Special:
        return LevelSceneEntityKind::Special;
    case TriggerObjectKind::CommandPoint:
        return LevelSceneEntityKind::StaticScene;
    }
    return LevelSceneEntityKind::StaticScene;
}

const TriggerObjectState* LevelSceneRegistry::find_state(
    const LevelTriggerState& state,
    std::size_t node) noexcept {
    return state.object(node);
}

void LevelSceneRegistry::copy_source_metadata(
    LevelSceneEntity& entity,
    const TriggerObjectState& source) {
    entity.spawn_options = source.spawn_options;
    entity.factory_node_bytes = source.factory_node_bytes;
    entity.has_spawn_option_2 = source.has_spawn_option_2;
    entity.has_spawn_option_4 = source.has_spawn_option_4;
    entity.factory_requires_environment_registration =
        source.factory_requires_environment_registration;
    entity.factory_clears_object_flag_2 = source.factory_clears_object_flag_2;
    entity.factory_sets_object_flag_4 = source.factory_sets_object_flag_4;
    entity.trigger_flags = source.trigger_flags;
    entity.trigger_state = source.trigger_state;
    entity.trigger_mode = source.trigger_mode;
    entity.has_trigger_runtime = source.has_trigger_runtime;
    entity.special_runtime_owner = source.special_runtime_owner;
    entity.special_runtime_control = source.special_runtime_control;
    entity.has_special_runtime_context = source.has_special_runtime_context;
    entity.has_special_runtime = source.has_special_runtime;
    entity.special_runtime_active = source.special_runtime_active;
}

void LevelSceneRegistry::sync_binding(
    const LevelTriggerState& state,
    const LevelSceneBinding& binding) {
    const TriggerObjectState* source = find_state(state, binding.trigger_node);
    if (source == nullptr) {
        return;
    }
    for (const std::size_t entity_index : binding.entities) {
        if (entity_index >= entities_.size()) {
            continue;
        }
        LevelSceneEntity& entity = entities_[entity_index];
        copy_source_metadata(entity, *source);
        entity.gameplay_flags = source->flags;
        entity.special_asset_flags_or = source->special_asset_flags_or;
        entity.special_asset_marker = source->special_asset_marker;
        entity.has_special_asset_state = source->has_special_asset_state;
        entity.active = source->active;
        entity.suspended = source->suspended;
        entity.alive = source->alive;
        entity.killed = source->killed;
        entity.visible_commanded = source->visible_commanded;
        if (entity.psx_object_index == CommandPointRuntime::npos) {
            entity.position = source->position;
            entity.orientation = source->orientation;
            entity.has_orientation = source->has_orientation;
        }
    }
}

} // namespace opentony::trg
