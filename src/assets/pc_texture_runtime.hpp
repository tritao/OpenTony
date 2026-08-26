#pragma once

#include "bmp_asset.hpp"
#include "pkr_asset.hpp"
#include "psx_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class PcTextureRuntimeError final : public std::runtime_error {
public:
    explicit PcTextureRuntimeError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kPcTextureRuntimeRecordSize = 0x2c;

enum class PcTextureSourceKind : std::uint8_t {
    ExternalBitmap,
    InlinePsx,
};

struct PcTextureImage final {
    std::uint32_t width{};
    std::uint32_t height{};
    // Top-left-origin RGB bytes. A presentation backend may convert this to
    // the device format after the runtime ownership boundary.
    std::vector<std::uint8_t> rgb;
};

// The device-independent portion of the PC texture allocation. The opaque
// Direct3D pointer and intrusive links stay zero in the raw image; the
// material/image association is retained as native side data.
class PcTextureRuntimeRecord final {
public:
    [[nodiscard]] std::size_t material_index() const noexcept {
        return material_index_;
    }
    [[nodiscard]] std::uint32_t material_checksum() const noexcept {
        return material_checksum_;
    }
    [[nodiscard]] PcTextureSourceKind source_kind() const noexcept {
        return source_kind_;
    }
    [[nodiscard]] const std::string& source_path() const noexcept {
        return source_path_;
    }
    [[nodiscard]] const PcTextureImage& image() const noexcept { return image_; }
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept {
        return raw_;
    }

    [[nodiscard]] std::uint32_t flags() const noexcept;
    [[nodiscard]] std::array<std::uint32_t, 2> declared_dimensions() const noexcept;
    [[nodiscard]] std::array<std::uint32_t, 2> normalized_dimensions() const noexcept;
    [[nodiscard]] std::array<std::uint32_t, 2> source_dimensions() const noexcept;
    [[nodiscard]] bool ready() const noexcept;

private:
    friend class PcTextureRuntime;

    std::array<std::byte, kPcTextureRuntimeRecordSize> raw_{};
    std::size_t material_index_{};
    std::uint32_t material_checksum_{};
    PcTextureSourceKind source_kind_{PcTextureSourceKind::ExternalBitmap};
    std::string source_path_;
    PcTextureImage image_;

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept;
    void put16(std::size_t offset, std::uint16_t value) noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

struct PcTextureRuntimeBuildOptions final {
    // 0x1a is the observed successful external-bitmap path; 0x12 keeps the
    // indexed/inline path free of the bitmap flag 0x08.
    std::uint32_t external_flags{0x1a};
    std::uint32_t inline_flags{0x12};
    // The retail device may normalize dimensions for hardware limits. The
    // default preserves source dimensions until a backend supplies policy.
    std::function<std::array<std::uint32_t, 2>(std::uint32_t, std::uint32_t)>
        normalize_dimensions;
};

// Material-to-image runtime manager. It implements the proven allocation,
// dimension, ready-marker, and source-selection boundary without fabricating
// a Direct3D object or 32-bit heap pointer.
class PcTextureRuntime final {
public:
    static PcTextureRuntime build_external(
        const PsxRuntimeEnvironment& runtime,
        const std::filesystem::path& asset_root,
        const PcTextureRuntimeBuildOptions& options = {});
    static PcTextureRuntime build_external(
        const PsxRuntimeEnvironment& runtime,
        const PkrArchive& package,
        const PcTextureRuntimeBuildOptions& options = {});
    static PcTextureRuntime build_inline(
        const PsxRuntimeEnvironment& runtime,
        const PcTextureRuntimeBuildOptions& options = {});

    // Fill only materials not already resolved by an external source. This
    // lets a level combine hash-named PC images with inline PSX variants.
    void add_inline_missing(
        const PsxRuntimeEnvironment& runtime,
        const PcTextureRuntimeBuildOptions& options = {});

    [[nodiscard]] const std::vector<PcTextureRuntimeRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] std::size_t material_count() const noexcept {
        return material_record_indices_.size();
    }
    [[nodiscard]] std::size_t resolved_count() const noexcept {
        return records_.size();
    }
    [[nodiscard]] std::size_t unresolved_count() const noexcept {
        return material_count() - resolved_count();
    }
    [[nodiscard]] const PcTextureRuntimeRecord* record_for_material(
        std::size_t material_index,
        std::uint32_t material_checksum) const noexcept;
    [[nodiscard]] std::optional<std::array<std::uint32_t, 2>>
    dimensions_for_material(
        std::size_t material_index,
        std::uint32_t material_checksum) const noexcept;

private:
    std::vector<PcTextureRuntimeRecord> records_;
    std::vector<std::size_t> material_record_indices_;

    explicit PcTextureRuntime(std::size_t material_count)
        : material_record_indices_(
            material_count, std::numeric_limits<std::size_t>::max()) {}

    void add_image(
        std::size_t material_index,
        std::uint32_t material_checksum,
        PcTextureSourceKind source_kind,
        std::string source_path,
        PcTextureImage image,
        std::array<std::uint32_t, 2> declared_dimensions,
        std::uint32_t flags,
        const PcTextureRuntimeBuildOptions& options);
};

} // namespace opentony::assets
