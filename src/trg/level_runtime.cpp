#include "level_runtime.hpp"

namespace opentony::trg {

LevelRuntime::LevelRuntime(
    const std::string& trg_path,
    const std::string& psx_path,
    const std::string& asset_root)
    : state_(),
      runtime_(TrgFile::load(trg_path), state_),
      archive_(assets::PsxArchive::load(psx_path)),
      collision_(assets::PsxCollisionWorld::build(archive_)),
      catalog_(asset_root.empty()
          ? std::nullopt
          : std::optional<assets::PsxAssetCatalog>(assets::PsxAssetCatalog::scan(asset_root))),
      pre_catalog_(asset_root.empty()
          ? std::nullopt
          : std::optional<assets::PreAssetCatalog>(assets::PreAssetCatalog::scan(asset_root))),
      scene_() {}

void LevelRuntime::initialize(bool two_player, const GapTable* gap_table) {
    state_.reset();
    state_.set_gap_table(gap_table);
    runtime_.initialize(two_player);
    state_.bind_psx_models(archive_);
    scene_.build(state_, archive_);
    if (catalog_.has_value()) {
        scene_.resolve_factory_assets(*catalog_);
    }
    refresh_resource_bindings();
    initialized_ = true;
}

void LevelRuntime::tick(std::uint32_t milliseconds) {
    if (!initialized_) {
        throw FormatError("level runtime was ticked before initialize");
    }
    state_.advance_time(milliseconds);
    scene_.sync(state_);
}

void LevelRuntime::pulse_node(std::size_t node) {
    if (!initialized_) {
        throw FormatError("level runtime was pulsed before initialize");
    }
    runtime_.pulse_node(node);
    scene_.sync(state_);
    refresh_resource_bindings();
}

void LevelRuntime::pulse_checksum(std::uint32_t checksum) {
    if (!initialized_) {
        throw FormatError("level runtime was pulsed before initialize");
    }
    const CommandPointRuntime* point = runtime_.command_point_by_checksum(checksum);
    if (point == nullptr) {
        throw FormatError("no command point has the requested checksum");
    }
    pulse_node(point->source_node);
}

void LevelRuntime::execute_restart(std::size_t node) {
    if (!initialized_) {
        throw FormatError("level runtime executed a restart before initialize");
    }
    runtime_.execute_restart(node);
    scene_.sync(state_);
}

void LevelRuntime::execute_restart(std::string_view name) {
    if (!initialized_) {
        throw FormatError("level runtime executed a restart before initialize");
    }
    const std::size_t node = runtime_.find_restart_by_name(name);
    if (node == CommandPointRuntime::npos) {
        throw FormatError("restart name was not found in the level");
    }
    execute_restart(node);
}

const assets::PsxAssetCatalog* LevelRuntime::asset_catalog() const noexcept {
    return catalog_.has_value() ? &*catalog_ : nullptr;
}

const assets::PreAssetCatalog* LevelRuntime::pre_catalog() const noexcept {
    return pre_catalog_.has_value() ? &*pre_catalog_ : nullptr;
}

void LevelRuntime::refresh_resource_bindings() {
    resources_.clear();
    resources_.reserve(state_.resources().size());
    for (const TriggerResourceRequest& request : state_.resources()) {
        LevelResourceBinding binding{};
        binding.request = request;
        if (catalog_.has_value()) {
            if (const std::string* path = catalog_->path_for(request.name);
                path != nullptr) {
                binding.asset_path = *path;
                binding.asset_available = true;
            }
        }
        resources_.push_back(std::move(binding));
    }
}

} // namespace opentony::trg
