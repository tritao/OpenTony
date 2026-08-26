#include "level_runtime.hpp"

#include "../assets/resource_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <vector>

namespace {

[[nodiscard]] std::string upper_copy(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return value;
}

[[nodiscard]] std::optional<std::size_t> package_psx_entry(
    const opentony::assets::PkrArchive& package,
    std::string_view stem) {
    const std::string wanted = upper_copy(std::string(stem) + ".PSX");
    for (std::size_t index = 0; index < package.entries().size(); ++index) {
        const auto& entry = package.entries()[index];
        if (upper_copy(entry.name) == wanted) {
            return index;
        }
    }
    return std::nullopt;
}

[[nodiscard]] const opentony::assets::PkrFileEntry* package_resource_entry(
    const opentony::assets::PkrArchive& package,
    std::string_view requested) {
    const std::string wanted = upper_copy(std::string(requested));
    const std::string wanted_stem = upper_copy(
        std::filesystem::path(std::string(requested)).stem().string());
    for (const opentony::assets::PkrFileEntry& entry : package.entries()) {
        const std::string entry_name = upper_copy(entry.name);
        const std::string entry_stem = upper_copy(
            std::filesystem::path(entry.name).stem().string());
        if (entry_name == wanted || entry_stem == wanted_stem) {
            return &entry;
        }
    }
    return nullptr;
}

[[nodiscard]] std::vector<std::byte> load_package_resource(
    const opentony::assets::PkrArchive& package,
    std::string_view name) {
    opentony::assets::ResourceBackend backend(package);
    opentony::assets::ResourceLoader loader(backend);
    return loader.load_owned(name);
}

} // namespace

namespace opentony::trg {

LevelRuntime::LevelRuntime(
    const std::string& trg_path,
    const std::string& psx_path,
    const std::string& asset_root)
    : state_(),
      runtime_(TrgFile::load(trg_path), state_),
      archive_(assets::PsxArchive::load(psx_path)),
      scene_runtime_(assets::PsxRuntimeEnvironment::build(archive_)),
      collision_(assets::PsxCollisionWorld::build(archive_)),
      asset_root_(asset_root),
      catalog_(asset_root.empty()
          ? std::nullopt
          : std::optional<assets::PsxAssetCatalog>(assets::PsxAssetCatalog::scan(asset_root))),
      pre_catalog_(asset_root.empty()
          ? std::nullopt
          : std::optional<assets::PreAssetCatalog>(assets::PreAssetCatalog::scan(asset_root))),
      scene_(),
      package_(nullptr) {}

LevelRuntime::LevelRuntime(
    const assets::PkrArchive& package,
    std::string_view trg_entry,
    std::string_view psx_entry,
    const std::string& asset_root)
    : state_(),
      runtime_(TrgFile::parse(load_package_resource(package, trg_entry)), state_),
      archive_(assets::PsxArchive::parse(
          load_package_resource(package, psx_entry), std::string(psx_entry))),
      scene_runtime_(assets::PsxRuntimeEnvironment::build(archive_)),
      collision_(assets::PsxCollisionWorld::build(archive_)),
      asset_root_(asset_root),
      catalog_(asset_root.empty()
          ? std::nullopt
          : std::optional<assets::PsxAssetCatalog>(assets::PsxAssetCatalog::scan(asset_root))),
      pre_catalog_(asset_root.empty()
          ? std::nullopt
          : std::optional<assets::PreAssetCatalog>(assets::PreAssetCatalog::scan(asset_root))),
      scene_(),
      package_(&package) {}

void LevelRuntime::initialize(bool two_player, const GapTable* gap_table) {
    state_.reset();
    state_.set_gap_table(gap_table);
    runtime_.initialize(two_player);
    camera_points_.build(runtime_.file());
    rails_.build(runtime_.file());
    rails_.synchronize_states(state_);
    if (catalog_.has_value()) {
        traffic_.build(runtime_.file(), &*catalog_);
    } else if (package_ != nullptr) {
        traffic_.build(runtime_.file(), *package_);
    } else {
        traffic_.build(runtime_.file());
    }
    factory_objects_.build(runtime_.file(), &state_);
    factory_objects_.synchronize(state_);
    powerups_.build(runtime_.file(), catalog_.has_value() ? &*catalog_ : nullptr);
    item_archive_.reset();
    medal_archive_.reset();
    bits_archive_.reset();
    item_runtime_.reset();
    medal_runtime_.reset();
    item_texture_runtime_.reset();
    medal_texture_runtime_.reset();
    if (catalog_.has_value() && catalog_->contains("items")) {
        item_runtime_ = assets::PsxRuntimeEnvironment::build(
            catalog_->load("items"));
    }
    if (catalog_.has_value() && catalog_->contains("skmedals")) {
        medal_runtime_ = assets::PsxRuntimeEnvironment::build(
            catalog_->load("skmedals"));
    }
    if (!catalog_.has_value() && package_ != nullptr) {
        if (const auto entry = package_psx_entry(*package_, "ITEMS");
            entry.has_value()) {
            item_archive_ = assets::PsxArchive::parse(
                package_->decode(*entry),
                package_->entry(*entry).archive_path());
            item_runtime_ = assets::PsxRuntimeEnvironment::build(*item_archive_);
        }
        if (const auto entry = package_psx_entry(*package_, "SKMEDALS");
            entry.has_value()) {
            medal_archive_ = assets::PsxArchive::parse(
                package_->decode(*entry),
                package_->entry(*entry).archive_path());
            medal_runtime_ = assets::PsxRuntimeEnvironment::build(*medal_archive_);
        }
    }
    const auto build_texture_region = [this](
        const std::optional<assets::PsxRuntimeEnvironment>& region)
        -> std::optional<assets::PcTextureRuntime> {
        if (!region.has_value()) {
            return std::nullopt;
        }
        std::optional<assets::PcTextureRuntime> textures;
        if (catalog_.has_value()) {
            textures = assets::PcTextureRuntime::build_external(
                *region, asset_root_);
        } else if (package_ != nullptr) {
            textures = assets::PcTextureRuntime::build_external(
                *region, *package_);
        } else {
            textures = assets::PcTextureRuntime::build_inline(*region);
        }
        textures->add_inline_missing(*region);
        return textures;
    };
    item_texture_runtime_ = build_texture_region(item_runtime_);
    medal_texture_runtime_ = build_texture_region(medal_runtime_);
    powerups_.resolve_runtime_models(
        item_runtime_.has_value() ? &*item_runtime_ : nullptr,
        medal_runtime_.has_value() ? &*medal_runtime_ : nullptr);
    bits_runtime_.reset();
    if (catalog_.has_value() && catalog_->contains("bits")) {
        bits_runtime_.emplace();
        bits_runtime_->build(catalog_->load("bits"));
    }
    if (!catalog_.has_value() && package_ != nullptr) {
        if (const auto entry = package_psx_entry(*package_, "BITS");
            entry.has_value()) {
            bits_archive_ = assets::PsxArchive::parse(
                package_->decode(*entry),
                package_->entry(*entry).archive_path());
            bits_runtime_.emplace();
            bits_runtime_->build(*bits_archive_);
        }
    }
    state_.bind_psx_models(archive_);
    if (catalog_.has_value()) {
        state_.resolve_pickup_assets(*catalog_);
    }
    scene_.build(state_, archive_);
    if (catalog_.has_value()) {
        texture_runtime_ = assets::PcTextureRuntime::build_external(
            scene_runtime_, asset_root_);
    } else if (package_ != nullptr) {
        texture_runtime_ = assets::PcTextureRuntime::build_external(
            scene_runtime_, *package_);
    } else {
        texture_runtime_ = assets::PcTextureRuntime::build_inline(scene_runtime_);
    }
    texture_runtime_->add_inline_missing(scene_runtime_);
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
    factory_objects_.synchronize(state_);
}

void LevelRuntime::pulse_node(std::size_t node) {
    if (!initialized_) {
        throw FormatError("level runtime was pulsed before initialize");
    }
    runtime_.pulse_node(node);
    rails_.synchronize_states(state_);
    scene_.sync(state_);
    factory_objects_.synchronize(state_);
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
    rails_.synchronize_states(state_);
    scene_.sync(state_);
    factory_objects_.synchronize(state_);
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
                // Keep the trigger resource boundary lazy at the catalog
                // level, but make a requested resource's parse observable at
                // the level-runtime boundary. PsxAssetCatalog owns the cache,
                // so repeated script requests do not duplicate the archive.
                const assets::PsxArchive& archive = catalog_->load(request.name);
                binding.asset_loaded = true;
                binding.asset_object_count = archive.objects().size();
                binding.asset_model_count = archive.model_names().size();
            }
        } else if (package_ != nullptr) {
            if (const assets::PkrFileEntry* entry = package_resource_entry(
                    *package_, request.name);
                entry != nullptr) {
                binding.asset_path = entry->archive_path();
                binding.asset_available = true;
            }
        }
        resources_.push_back(std::move(binding));
    }
}

} // namespace opentony::trg
