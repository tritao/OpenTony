#include "skater_asset_runtime.hpp"

#include <algorithm>
#include <stdexcept>

namespace opentony::runtime {

SkaterAssetRuntime::SkaterAssetRuntime(
    const std::string& animation_psh_path,
    const std::string& model_psh_path,
    const std::string& model_psx_path,
    std::uint8_t region_slot)
    : animation_manifest_(assets::PshManifest::load(animation_psh_path)),
      model_manifest_(assets::PshManifest::load(model_psh_path)),
      archive_(assets::PsxArchive::load(model_psx_path)),
      runtime_(assets::PsxRuntimeEnvironment::build(archive_, region_slot)),
      part_matches_(assets::match_psh_parts(
          animation_manifest_, model_manifest_)) {}

std::optional<assets::PshPartMatch>
SkaterAssetRuntime::match_for_animation_part(
    std::uint32_t animation_part_index) const noexcept {
    const auto found = std::find_if(
        part_matches_.begin(),
        part_matches_.end(),
        [animation_part_index](const assets::PshPartMatch& match) {
            return match.animation_index == animation_part_index;
        });
    if (found == part_matches_.end()) {
        return std::nullopt;
    }
    return *found;
}

const assets::PsxRuntimeEnvironmentObject&
SkaterAssetRuntime::runtime_object_for_part(
    std::uint32_t animation_part_index) const {
    const auto match = match_for_animation_part(animation_part_index);
    if (!match.has_value()) {
        throw assets::PsxFormatError(
            "animation PSH part has no model PSH counterpart");
    }
    if (match->model_index >= runtime_.object_count()) {
        throw assets::PsxFormatError(
            "PSH model part is outside the PSX runtime object table");
    }
    return runtime_.object(match->model_index);
}

const assets::PsxModel& SkaterAssetRuntime::model_for_part(
    std::uint32_t animation_part_index) const {
    const auto match = match_for_animation_part(animation_part_index);
    if (!match.has_value()) {
        throw assets::PsxFormatError(
            "animation PSH part has no model PSH counterpart");
    }
    if (match->model_index >= runtime_.model_count()) {
        throw assets::PsxFormatError(
            "PSH model part is outside the PSX runtime model table");
    }
    return *runtime_.model_pointer(match->model_index);
}

void SkaterAssetRuntime::bind(
    SkaterRuntimeObject& skater,
    std::uint32_t animation_part_index) const {
    const auto match = match_for_animation_part(animation_part_index);
    if (!match.has_value()) {
        throw assets::PsxFormatError(
            "animation PSH part has no model PSH counterpart");
    }
    if (match->model_index > 0xffffU) {
        throw assets::PsxFormatError(
            "PSH model part index cannot fit the skater model field");
    }
    skater.set_psx_binding(
        runtime_.slot(),
        static_cast<std::uint16_t>(match->model_index));
}

} // namespace opentony::runtime
