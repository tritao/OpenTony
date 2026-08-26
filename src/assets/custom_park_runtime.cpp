#include "custom_park_runtime.hpp"

#include <algorithm>
#include <limits>
#include <string>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint16_t read16(
    std::span<const std::byte> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::uint32_t read32(
    std::span<const std::byte> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

void write16(std::span<std::byte> bytes, std::size_t offset, std::uint16_t value) noexcept {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void write32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept {
    write16(bytes, offset, static_cast<std::uint16_t>(value));
    write16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

[[nodiscard]] std::int32_t scaled_coordinate(std::int16_t value) {
    const auto widened = static_cast<std::int64_t>(value) << 12;
    if (widened < std::numeric_limits<std::int32_t>::min()
        || widened > std::numeric_limits<std::int32_t>::max()) {
        throw CustomParkRuntimeError("custom park generated coordinate overflows Q12");
    }
    return static_cast<std::int32_t>(widened);
}

} // namespace

std::uint16_t CustomParkRuntimeCell::translated_reference(std::size_t index) const {
    if (index >= 5U) {
        throw CustomParkRuntimeError("custom park runtime cell reference is out of range");
    }
    return read16(raw, index * 2U);
}

std::uint8_t CustomParkRuntimeItem::item_byte() const noexcept {
    return std::to_integer<std::uint8_t>(raw[0x28]);
}

std::string CustomParkRuntimeItem::item_name() const {
    const auto bytes = std::span<const std::byte>(raw).subspan(0x0d, 0x1b);
    std::size_t length = 0;
    while (length < bytes.size() && bytes[length] != std::byte{0}) {
        ++length;
    }
    return std::string(reinterpret_cast<const char*>(bytes.data()), length);
}

std::array<std::int32_t, 3> CustomParkPublishedObject::position() const noexcept {
    return {
        static_cast<std::int32_t>(read32(raw, 0x08)),
        static_cast<std::int32_t>(read32(raw, 0x0c)),
        static_cast<std::int32_t>(read32(raw, 0x10)),
    };
}

std::uint16_t CustomParkPublishedObject::model_index() const noexcept {
    return read16(raw, 0x1a);
}

std::uint8_t CustomParkPublishedObject::slot() const noexcept {
    return std::to_integer<std::uint8_t>(raw[0x1f]);
}

std::uint32_t CustomParkGapMember::source_item() const noexcept {
    return read32(raw, 0x1c);
}

std::uint32_t CustomParkGapMember::partner_index() const noexcept {
    return read32(raw, 0x08);
}

std::uint32_t CustomParkGapMember::next_index() const noexcept {
    return read32(raw, 0x0c);
}

std::uint32_t CustomParkGapMember::published_object_index() const noexcept {
    return read32(raw, 0x14);
}

bool CustomParkGapMember::active() const noexcept {
    return std::to_integer<std::uint8_t>(raw[0x38]) != 0;
}

CustomParkRuntimeRegion CustomParkRuntimeRegion::build(const CustomParkArchive& archive) {
    CustomParkRuntimeRegion result{};
    result.source_archive_ = &archive;
    result.cells_.reserve(archive.cells().size());
    for (const CustomParkCell& source : archive.cells()) {
        CustomParkRuntimeCell cell{};
        for (std::size_t index = 0; index < source.translated_references.size(); ++index) {
            write16(cell.raw, index * 2U, source.translated_references[index]);
        }
        for (std::size_t index = 0; index < source.unpacked_cell_values.size(); ++index) {
            cell.raw[0x0aU + index] = static_cast<std::byte>(source.unpacked_cell_values[index]);
        }
        result.cells_.push_back(cell);
    }
    result.items_.reserve(archive.items().size());
    for (const CustomParkItemRecord& source : archive.items()) {
        CustomParkRuntimeItem item{};
        item.raw[0] = static_cast<std::byte>(source.endpoint_axis_index_0[0]);
        item.raw[1] = static_cast<std::byte>(source.endpoint_axis_index_0[1]);
        item.raw[2] = static_cast<std::byte>(source.endpoint_axis_index_1[0]);
        item.raw[3] = static_cast<std::byte>(source.endpoint_axis_index_1[1]);
        item.raw[4] = static_cast<std::byte>(source.endpoint_axis_index_2[0]);
        item.raw[5] = static_cast<std::byte>(source.endpoint_axis_index_2[1]);
        item.raw[6] = static_cast<std::byte>(source.endpoint_two_bit_values[0]);
        item.raw[7] = static_cast<std::byte>(source.endpoint_two_bit_values[1]);
        for (std::size_t index = 0; index < source.expanded_nibble_values.size(); ++index) {
            item.raw[0x08U + index] = static_cast<std::byte>(source.expanded_nibble_values[index]);
        }
        item.raw[0x0c] = static_cast<std::byte>(source.packed_item_flags);
        const std::size_t copy_size = std::min(source.item_name.size(), std::size_t{0x1a});
        std::copy_n(source.item_name.begin(), copy_size,
            reinterpret_cast<char*>(item.raw.data() + 0x0d));
        item.raw[0x28] = static_cast<std::byte>(source.item_byte);
        result.items_.push_back(item);
    }
    return result;
}

std::size_t CustomParkRuntimeRegion::append_generated_piece(
    const CustomParkGeneratedPieceInput& input) {
    std::array<std::byte, kCustomParkGeneratedPieceSize> piece{};
    write16(piece, 0x10, input.source_dimensions[0]);
    write16(piece, 0x12, input.source_dimensions[1]);
    write16(piece, 0x14, input.source_dimensions[2]);
    piece[0x0b] = static_cast<std::byte>(input.source_height_cells);
    write16(piece, 0x21c, static_cast<std::uint16_t>(input.grid_position[0]));
    write16(piece, 0x21e, static_cast<std::uint16_t>(input.grid_position[1]));
    write16(piece, 0x220, static_cast<std::uint16_t>(input.grid_position[2]));
    generated_pieces_.push_back(piece);

    CustomParkPublishedObject object{};
    write32(object.raw, 0x08, static_cast<std::uint32_t>(scaled_coordinate(input.grid_position[0])));
    write32(object.raw, 0x0c, static_cast<std::uint32_t>(scaled_coordinate(input.grid_position[1])));
    write32(object.raw, 0x10, static_cast<std::uint32_t>(scaled_coordinate(input.grid_position[2])));
    write16(object.raw, 0x1a, input.model_index);
    object.raw[0x1f] = static_cast<std::byte>(input.slot);
    published_objects_.push_back(object);
    return published_objects_.size() - 1U;
}

void CustomParkRuntimeRegion::append_gap_pair(const CustomParkGapPairInput& input) {
    if (input.first_object >= published_objects_.size()
        || input.second_object >= published_objects_.size()) {
        throw CustomParkRuntimeError("custom park gap object index is out of range");
    }
    if (input.source_item >= items_.size()) {
        throw CustomParkRuntimeError("custom park gap item index is out of range");
    }
    const std::size_t first_index = gap_members_.size();
    const std::size_t second_index = first_index + 1U;
    CustomParkGapMember first{};
    CustomParkGapMember second{};
    write32(first.raw, 0x00, 1U);
    write32(second.raw, 0x00, 0U);
    write32(first.raw, 0x04, input.first_model_index);
    write32(second.raw, 0x04, input.second_model_index);
    write32(first.raw, 0x08, static_cast<std::uint32_t>(second_index));
    write32(second.raw, 0x08, static_cast<std::uint32_t>(first_index));
    write32(first.raw, 0x1c, input.source_item);
    write32(second.raw, 0x1c, input.source_item);
    write32(first.raw, 0x14, static_cast<std::uint32_t>(input.first_object));
    write32(second.raw, 0x14, static_cast<std::uint32_t>(input.second_object));
    first.raw[0x38] = static_cast<std::byte>(input.active ? 1 : 0);
    second.raw[0x38] = static_cast<std::byte>(input.active ? 1 : 0);
    first.raw[0x39] = std::byte{0};
    second.raw[0x39] = std::byte{0};
    gap_members_.push_back(first);
    gap_members_.push_back(second);
}

} // namespace opentony::assets
