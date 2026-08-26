#include "custom_park_asset.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint16_t u16(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw CustomParkFormatError("custom park u16 is truncated: " + source);
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::uint32_t u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw CustomParkFormatError("custom park u32 is truncated: " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

void require_range(
    std::size_t offset,
    std::size_t length,
    std::size_t size,
    const std::string& source) {
    if (offset > size || length > size - offset) {
        throw CustomParkFormatError("custom park range is outside the file: " + source);
    }
}

[[nodiscard]] CustomParkDimensions dimensions_for(std::uint32_t version, const std::string& source) {
    switch (version) {
    case 0:
        return {16, 16};
    case 1:
        return {24, 24};
    case 2:
        return {30, 30};
    case 3:
        return {30, 18};
    case 4:
        return {60, 6};
    default:
        throw CustomParkFormatError("unsupported custom park version: " + source);
    }
}

[[nodiscard]] std::size_t checked_product(
    std::size_t left,
    std::size_t right,
    const std::string& source) {
    if (right != 0 && left > std::numeric_limits<std::size_t>::max() / right) {
        throw CustomParkFormatError("custom park count overflows: " + source);
    }
    return left * right;
}

[[nodiscard]] std::string bounded_name(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    std::size_t length,
    const std::string& source) {
    require_range(offset, length, bytes.size(), source);
    std::size_t end = 0;
    while (end < length && bytes[offset + end] != std::byte{0}) {
        ++end;
    }
    if (end == length) {
        throw CustomParkFormatError("custom park item name is unterminated: " + source);
    }
    return std::string(
        reinterpret_cast<const char*>(bytes.data() + offset), end);
}

} // namespace

CustomParkArchive CustomParkArchive::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw CustomParkFormatError("cannot open custom park: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw CustomParkFormatError("cannot determine custom park size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw CustomParkFormatError("cannot read custom park: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

CustomParkArchive CustomParkArchive::parse(
    std::vector<std::byte> bytes,
    std::string source,
    std::span<const std::uint16_t> reference_translation) {
    if (bytes.size() < 0x0cU) {
        throw CustomParkFormatError("custom park header is truncated: " + source);
    }
    CustomParkArchive result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    result.magic_ = u32(result.bytes_, 0, result.source_);
    if (result.magic_ != kCustomParkMagic && result.magic_ != kCustomParkLegacyMagic) {
        throw CustomParkFormatError("custom park magic is unsupported: " + result.source_);
    }
    result.version_ = u32(result.bytes_, 4, result.source_);
    result.map_variant_ = u32(result.bytes_, 8, result.source_);
    result.dimensions_ = dimensions_for(result.version_, result.source_);
    const std::size_t cell_count = checked_product(
        result.dimensions_.width, result.dimensions_.depth, result.source_);
    const std::size_t cell_bytes = checked_product(
        cell_count, kCustomParkCellDiskSize, result.source_);
    const std::size_t cells_end = 0x0cU + cell_bytes;
    if (cells_end < 0x0cU) {
        throw CustomParkFormatError("custom park cell range overflows: " + result.source_);
    }
    const std::size_t items_bytes = kCustomParkItemCount * kCustomParkItemDiskSize;
    const std::size_t items_end = cells_end + items_bytes;
    if (items_end < cells_end) {
        throw CustomParkFormatError("custom park item range overflows: " + result.source_);
    }
    const std::size_t content_end = items_end + kCustomParkTrailingTableSize;
    if (content_end < items_end || content_end > result.bytes_.size()) {
        throw CustomParkFormatError("custom park payload is truncated: " + result.source_);
    }

    result.disk_cells_.reserve(cell_count);
    result.cells_.reserve(cell_count);
    for (std::size_t index = 0; index < cell_count; ++index) {
        const std::size_t offset = 0x0cU + index * kCustomParkCellDiskSize;
        CustomParkDiskCell disk{};
        for (std::size_t reference = 0; reference < disk.compact_references.size(); ++reference) {
            disk.compact_references[reference] = std::to_integer<std::uint8_t>(
                result.bytes_[offset + reference]);
        }
        disk.packed_two_bit_values = std::to_integer<std::uint8_t>(result.bytes_[offset + 5]);
        disk.packed_value_word = u16(result.bytes_, offset + 6, result.source_);
        CustomParkCell cell{};
        for (std::size_t reference = 0; reference < disk.compact_references.size(); ++reference) {
            const std::uint8_t compact = disk.compact_references[reference];
            if (!reference_translation.empty()) {
                if (reference_translation.size() != 256U) {
                    throw CustomParkFormatError(
                        "custom park reference table must contain 256 entries: " + result.source_);
                }
                cell.translated_references[reference] = reference_translation[compact];
            } else {
                cell.translated_references[reference] =
                    compact == 0xffU ? 0xffffU : compact;
            }
        }
        const std::uint32_t packed = static_cast<std::uint32_t>(disk.packed_two_bit_values)
            | (static_cast<std::uint32_t>(disk.packed_value_word) << 8U);
        for (std::size_t value = 0; value < 5U; ++value) {
            cell.unpacked_cell_values[value] = static_cast<std::uint8_t>(
                (packed >> (value * 2U)) & 0x03U);
        }
        cell.unpacked_cell_values[5] = static_cast<std::uint8_t>((packed >> 10U) & 0xffU);
        cell.packed_two_bit_values = disk.packed_two_bit_values;
        cell.packed_value_word = disk.packed_value_word;
        result.disk_cells_.push_back(disk);
        result.cells_.push_back(cell);
    }

    result.disk_items_.reserve(kCustomParkItemCount);
    result.items_.reserve(kCustomParkItemCount);
    const bool current_format = result.magic_ == kCustomParkMagic;
    const std::size_t name_offset = current_format ? 0x0bU : 0x0aU;
    const std::size_t name_length = kCustomParkItemDiskSize - name_offset;
    for (std::size_t index = 0; index < kCustomParkItemCount; ++index) {
        const std::size_t offset = cells_end + index * kCustomParkItemDiskSize;
        CustomParkDiskItemRecord disk{};
        for (std::size_t value = 0; value < disk.endpoint_axis_indices.size(); ++value) {
            disk.endpoint_axis_indices[value] = std::to_integer<std::uint8_t>(
                result.bytes_[offset + value]);
        }
        disk.packed_nibble_word = u16(result.bytes_, offset + 6, result.source_);
        disk.packed_two_bit_word = u16(result.bytes_, offset + 8, result.source_);
        disk.legacy_load_item_byte = std::to_integer<std::uint8_t>(
            result.bytes_[offset + 9]);
        disk.current_item_byte = std::to_integer<std::uint8_t>(
            result.bytes_[offset + 10]);
        const std::string name = bounded_name(
            result.bytes_, offset + name_offset, name_length, result.source_);
        std::copy_n(
            name.begin(), std::min(name.size(), disk.current_name_bytes.size() - 1U),
            disk.current_name_bytes.begin());

        CustomParkItemRecord item{};
        item.endpoint_axis_index_0 = {
            disk.endpoint_axis_indices[0], disk.endpoint_axis_indices[1]};
        item.endpoint_axis_index_1 = {
            disk.endpoint_axis_indices[2], disk.endpoint_axis_indices[3]};
        item.endpoint_axis_index_2 = {
            disk.endpoint_axis_indices[4], disk.endpoint_axis_indices[5]};
        item.endpoint_two_bit_values = {
            static_cast<std::uint8_t>(disk.packed_two_bit_word & 0x03U),
            static_cast<std::uint8_t>((disk.packed_two_bit_word >> 2U) & 0x03U)};
        for (std::size_t value = 0; value < item.expanded_nibble_values.size(); ++value) {
            item.expanded_nibble_values[value] = static_cast<std::uint8_t>(
                (disk.packed_nibble_word >> (value * 4U)) & 0x0fU);
        }
        item.packed_item_flags = static_cast<std::uint8_t>(disk.packed_two_bit_word >> 4U);
        item.item_name = name;
        item.item_byte = current_format
            ? disk.current_item_byte : disk.legacy_load_item_byte;
        result.disk_items_.push_back(disk);
        result.items_.push_back(std::move(item));
    }
    std::copy_n(
        result.bytes_.begin() + static_cast<std::ptrdiff_t>(items_end),
        kCustomParkTrailingTableSize,
        result.trailing_table_.begin());
    return result;
}

std::size_t CustomParkArchive::aligned_allocation_size() const noexcept {
    const std::size_t content = serialized_content_size();
    return (content + 0x7fU) & ~static_cast<std::size_t>(0x7fU);
}

} // namespace opentony::assets
