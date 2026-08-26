#pragma once

#include "skater_runtime.hpp"
#include "../assets/psh_asset.hpp"
#include "../assets/psx_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace opentony::runtime {

// Native bridge for the confirmed player resource path:
// <base>.PSH part manifest -> name-based animation/model remap -> PSX region
// object/model table -> 0x3538 skater allocation. It owns the parsed PSX
// image so the runtime environment's model pointers remain stable.
class SkaterAssetRuntime final {
public:
    SkaterAssetRuntime(
        const std::string& animation_psh_path,
        const std::string& model_psh_path,
        const std::string& model_psx_path,
        std::uint8_t region_slot = 0);

    SkaterAssetRuntime(const SkaterAssetRuntime&) = delete;
    SkaterAssetRuntime& operator=(const SkaterAssetRuntime&) = delete;
    SkaterAssetRuntime(SkaterAssetRuntime&&) = delete;
    SkaterAssetRuntime& operator=(SkaterAssetRuntime&&) = delete;

    [[nodiscard]] std::uint8_t region_slot() const noexcept {
        return runtime_.slot();
    }
    [[nodiscard]] const assets::PshManifest& animation_manifest() const noexcept {
        return animation_manifest_;
    }
    [[nodiscard]] const assets::PshManifest& model_manifest() const noexcept {
        return model_manifest_;
    }
    [[nodiscard]] const assets::PsxArchive& archive() const noexcept {
        return archive_;
    }
    [[nodiscard]] const assets::PsxRuntimeEnvironment& runtime() const noexcept {
        return runtime_;
    }
    [[nodiscard]] const std::vector<assets::PshPartMatch>& part_matches() const noexcept {
        return part_matches_;
    }

    [[nodiscard]] std::optional<assets::PshPartMatch> match_for_animation_part(
        std::uint32_t animation_part_index) const noexcept;
    [[nodiscard]] const assets::PsxRuntimeEnvironmentObject& runtime_object_for_part(
        std::uint32_t animation_part_index) const;
    [[nodiscard]] const assets::PsxModel& model_for_part(
        std::uint32_t animation_part_index) const;

    // Writes the recovered +0x1f region and +0x1a model fields into an
    // existing gameplay skater object. Missing PSH matches are rejected rather
    // than silently selecting a positional part.
    void bind(
        SkaterRuntimeObject& skater,
        std::uint32_t animation_part_index) const;

private:
    assets::PshManifest animation_manifest_;
    assets::PshManifest model_manifest_;
    assets::PsxArchive archive_;
    assets::PsxRuntimeEnvironment runtime_;
    std::vector<assets::PshPartMatch> part_matches_;
};

} // namespace opentony::runtime
