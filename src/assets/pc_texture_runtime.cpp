#include "pc_texture_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::string upper_ascii(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return value;
}

[[nodiscard]] std::string hash_name(std::uint32_t checksum) {
    std::ostringstream stream;
    stream << std::uppercase << std::hex << std::setw(8)
           << std::setfill('0') << checksum << ".BMP";
    return stream.str();
}

[[nodiscard]] std::uint16_t checked_u16(
    std::uint32_t value,
    const char* field) {
    if (value > std::numeric_limits<std::uint16_t>::max()) {
        throw PcTextureRuntimeError(
            std::string("PC texture ") + field + " exceeds the runtime u16 field");
    }
    return static_cast<std::uint16_t>(value);
}

[[nodiscard]] std::array<std::uint32_t, 2> normalize_dimensions(
    std::uint32_t width,
    std::uint32_t height,
    const PcTextureRuntimeBuildOptions& options) {
    const std::array<std::uint32_t, 2> result = options.normalize_dimensions
        ? options.normalize_dimensions(width, height)
        : std::array<std::uint32_t, 2>{width, height};
    if (result[0] == 0 || result[1] == 0) {
        throw PcTextureRuntimeError("PC texture dimensions cannot be zero");
    }
    (void)checked_u16(result[0], "normalized width");
    (void)checked_u16(result[1], "normalized height");
    return result;
}

[[nodiscard]] std::optional<std::filesystem::path> find_case_insensitive_file(
    const std::vector<std::filesystem::path>& directories,
    std::string_view filename) {
    const std::string wanted = upper_ascii(std::string(filename));
    for (const std::filesystem::path& directory : directories) {
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error)) {
            continue;
        }
        const std::filesystem::path exact = directory / std::string(filename);
        if (std::filesystem::is_regular_file(exact, error)) {
            return exact;
        }
        error.clear();
        for (const std::filesystem::directory_entry& entry
             : std::filesystem::directory_iterator(directory, error)) {
            if (error) {
                break;
            }
            std::error_code regular_error;
            if (std::filesystem::is_regular_file(entry, regular_error)
                && upper_ascii(entry.path().filename().string()) == wanted) {
                return entry.path();
            }
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> find_package_bitmap(
    const PkrArchive& package,
    std::string_view filename) {
    const std::string wanted = upper_ascii(std::string(filename));
    for (std::size_t index = 0; index < package.entries().size(); ++index) {
        const PkrFileEntry& entry = package.entries()[index];
        if (upper_ascii(entry.name) != wanted) {
            continue;
        }
        const std::string directory = upper_ascii(entry.directory);
        if (directory == "NEWTEX" || directory == "NEWBMP"
            || directory.empty()) {
            return index;
        }
    }
    return std::nullopt;
}

} // namespace

std::uint16_t PcTextureRuntimeRecord::u16(std::size_t offset) const noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint16_t>(
            std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U));
}

std::uint32_t PcTextureRuntimeRecord::u32(std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(raw_[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(raw_[offset + 3])) << 24U));
}

void PcTextureRuntimeRecord::put16(
    std::size_t offset,
    std::uint16_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value & 0xffU);
    raw_[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void PcTextureRuntimeRecord::put32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    put16(offset, static_cast<std::uint16_t>(value));
    put16(offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

std::uint32_t PcTextureRuntimeRecord::flags() const noexcept {
    return u32(0x10);
}

std::array<std::uint32_t, 2>
PcTextureRuntimeRecord::declared_dimensions() const noexcept {
    return {u16(0x14), u16(0x16)};
}

std::array<std::uint32_t, 2>
PcTextureRuntimeRecord::normalized_dimensions() const noexcept {
    return {u16(0x18), u16(0x1a)};
}

std::array<std::uint32_t, 2>
PcTextureRuntimeRecord::source_dimensions() const noexcept {
    return {u16(0x1c), u16(0x1e)};
}

bool PcTextureRuntimeRecord::ready() const noexcept {
    return u32(0x20) == 1U;
}

PcTextureRuntime PcTextureRuntime::build_external(
    const PsxRuntimeEnvironment& runtime,
    const std::filesystem::path& asset_root,
    const PcTextureRuntimeBuildOptions& options) {
    PcTextureRuntime result(runtime.materials().records().size());
    const std::vector<std::filesystem::path> directories{
        asset_root,
        asset_root / "newtex",
        asset_root / ".." / "newtex",
        asset_root.parent_path() / "newtex",
    };
    for (std::size_t material_index = 0;
         material_index < runtime.materials().records().size();
         ++material_index) {
        const std::uint32_t checksum =
            runtime.materials().record(material_index).checksum();
        const std::string filename = hash_name(checksum);
        const auto path = find_case_insensitive_file(directories, filename);
        if (!path.has_value()) {
            continue;
        }
        const BmpAsset bitmap = BmpAsset::load(path->string());
        const BmpImage& image = bitmap.image();
        result.add_image(
            material_index,
            checksum,
            PcTextureSourceKind::ExternalBitmap,
            path->string(),
            PcTextureImage{image.width, image.height, image.rgb},
            {0, 0},
            options.external_flags,
            options);
    }
    return result;
}

PcTextureRuntime PcTextureRuntime::build_external(
    const PsxRuntimeEnvironment& runtime,
    const PkrArchive& package,
    const PcTextureRuntimeBuildOptions& options) {
    PcTextureRuntime result(runtime.materials().records().size());
    for (std::size_t material_index = 0;
         material_index < runtime.materials().records().size();
         ++material_index) {
        const std::uint32_t checksum =
            runtime.materials().record(material_index).checksum();
        const std::string filename = hash_name(checksum);
        const auto entry_index = find_package_bitmap(package, filename);
        if (!entry_index.has_value()) {
            continue;
        }
        const PkrFileEntry& entry = package.entry(*entry_index);
        const BmpAsset bitmap = BmpAsset::parse(
            package.decode(*entry_index), entry.archive_path());
        const BmpImage& image = bitmap.image();
        result.add_image(
            material_index,
            checksum,
            PcTextureSourceKind::ExternalBitmap,
            entry.archive_path(),
            PcTextureImage{image.width, image.height, image.rgb},
            {0, 0},
            options.external_flags,
            options);
    }
    return result;
}

PcTextureRuntime PcTextureRuntime::build_inline(
    const PsxRuntimeEnvironment& runtime,
    const PcTextureRuntimeBuildOptions& options) {
    PcTextureRuntime result(runtime.materials().records().size());
    result.add_inline_missing(runtime, options);
    return result;
}

void PcTextureRuntime::add_inline_missing(
    const PsxRuntimeEnvironment& runtime,
    const PcTextureRuntimeBuildOptions& options) {
    if (material_record_indices_.size() != runtime.materials().records().size()) {
        throw PcTextureRuntimeError(
            "inline texture source uses a different material table");
    }
    const PsxArchive& archive = runtime.source_archive();
    for (std::size_t texture_index = 0;
         texture_index < archive.textures().size();
         ++texture_index) {
        const PsxTexture& texture = archive.textures()[texture_index];
        const auto material_index = runtime.materials().material_index_for_texture(
            texture.name_index);
        if (!material_index.has_value()
            || material_record_indices_[*material_index]
                != std::numeric_limits<std::size_t>::max()) {
            continue;
        }
        const std::uint32_t checksum =
            runtime.materials().record(*material_index).checksum();
        const PsxDecodedTexture decoded = archive.decode_texture(texture_index);
        add_image(
            *material_index,
            checksum,
            PcTextureSourceKind::InlinePsx,
            {},
            PcTextureImage{
                decoded.width,
                decoded.height,
                decoded.rgb,
            },
            {texture.width, texture.height},
            options.inline_flags,
            options);
    }
}

void PcTextureRuntime::add_image(
    std::size_t material_index,
    std::uint32_t material_checksum,
    PcTextureSourceKind source_kind,
    std::string source_path,
    PcTextureImage image,
    std::array<std::uint32_t, 2> declared_dimensions,
    std::uint32_t flags,
    const PcTextureRuntimeBuildOptions& options) {
    if (material_index >= material_record_indices_.size()) {
        throw PcTextureRuntimeError("PC texture material index is outside the table");
    }
    if (material_record_indices_[material_index]
        != std::numeric_limits<std::size_t>::max()) {
        return;
    }
    if (image.width == 0 || image.height == 0) {
        throw PcTextureRuntimeError("PC texture image has zero dimensions");
    }
    const std::size_t width = static_cast<std::size_t>(image.width);
    const std::size_t height = static_cast<std::size_t>(image.height);
    if (height > std::numeric_limits<std::size_t>::max() / width
        || width * height > std::numeric_limits<std::size_t>::max() / 3U) {
        throw PcTextureRuntimeError("PC texture image size overflows the host");
    }
    const std::size_t expected_pixels = width * height * 3U;
    if (image.rgb.size() != expected_pixels) {
        throw PcTextureRuntimeError("PC texture image has an inconsistent RGB size");
    }
    const auto realized_dimensions =
        normalize_dimensions(image.width, image.height, options);
    (void)checked_u16(declared_dimensions[0], "declared width");
    (void)checked_u16(declared_dimensions[1], "declared height");
    (void)checked_u16(image.width, "source width");
    (void)checked_u16(image.height, "source height");

    PcTextureRuntimeRecord record{};
    record.material_index_ = material_index;
    record.material_checksum_ = material_checksum;
    record.source_kind_ = source_kind;
    record.source_path_ = std::move(source_path);
    record.image_ = std::move(image);
    record.put32(0x0c, material_checksum);
    record.put32(0x10, flags);
    record.put16(0x14, static_cast<std::uint16_t>(declared_dimensions[0]));
    record.put16(0x16, static_cast<std::uint16_t>(declared_dimensions[1]));
    record.put16(0x18, static_cast<std::uint16_t>(realized_dimensions[0]));
    record.put16(0x1a, static_cast<std::uint16_t>(realized_dimensions[1]));
    record.put16(0x1c, static_cast<std::uint16_t>(record.image_.width));
    record.put16(0x1e, static_cast<std::uint16_t>(record.image_.height));
    record.put32(0x20, 1U);
    material_record_indices_[material_index] = records_.size();
    records_.push_back(std::move(record));
}

const PcTextureRuntimeRecord* PcTextureRuntime::record_for_material(
    std::size_t material_index,
    std::uint32_t material_checksum) const noexcept {
    if (material_index >= material_record_indices_.size()) {
        return nullptr;
    }
    const std::size_t record_index = material_record_indices_[material_index];
    if (record_index == std::numeric_limits<std::size_t>::max()
        || record_index >= records_.size()) {
        return nullptr;
    }
    const PcTextureRuntimeRecord& record = records_[record_index];
    return record.material_checksum() == material_checksum ? &record : nullptr;
}

std::optional<std::array<std::uint32_t, 2>>
PcTextureRuntime::dimensions_for_material(
    std::size_t material_index,
    std::uint32_t material_checksum) const noexcept {
    const PcTextureRuntimeRecord* record =
        record_for_material(material_index, material_checksum);
    if (record == nullptr) {
        return std::nullopt;
    }
    return std::array<std::uint32_t, 2>{
        record->image().width,
        record->image().height,
    };
}

} // namespace opentony::assets
