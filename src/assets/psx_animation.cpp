#include "psx_animation.hpp"

#include <limits>
#include <stdexcept>
#include <string>

namespace opentony::assets {
namespace {

constexpr std::uint32_t kAnimationTag = 0x0000002cU;
constexpr std::uint32_t kHierarchyTag = 0x52454948U; // "HIER" little-endian

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::byte> bytes,
    std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw PsxFormatError("animation packet reads outside its tag");
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(
               std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::uint32_t read_u32(
    std::span<const std::byte> bytes,
    std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw PsxFormatError("animation packet reads outside its tag");
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(
               std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(
               std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(
               std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::span<const std::byte> tag_payload(
    const PsxArchive& archive,
    const PsxTag& tag) {
    const auto& bytes = archive.bytes();
    if (tag.offset > bytes.size() || bytes.size() - tag.offset < 8
        || tag.size > bytes.size() - tag.offset - 8) {
        throw PsxFormatError("animation tag exceeds the PSX resource");
    }
    return std::span<const std::byte>(bytes).subspan(tag.offset + 8, tag.size);
}

[[nodiscard]] const PsxTag& find_tag(
    const PsxArchive& archive,
    std::uint32_t type,
    const char* label) {
    for (const PsxTag& tag : archive.tags()) {
        if (tag.type == type) {
            return tag;
        }
    }
    throw PsxFormatError(std::string("PSX animation resource has no ") + label);
}

} // namespace

PsxAnimationTable PsxAnimationTable::load(const std::string& path) {
    return parse(PsxArchive::load(path));
}

PsxAnimationTable PsxAnimationTable::parse(const PsxArchive& archive) {
    const PsxTag& animation_tag = find_tag(
        archive,
        kAnimationTag,
        "type-0x2c animation tag");
    const std::span<const std::byte> source = tag_payload(archive, animation_tag);
    if (source.size() < 4) {
        throw PsxFormatError("animation tag is shorter than its count");
    }

    PsxAnimationTable result{};
    result.payload_.assign(source.begin(), source.end());
    const std::span<const std::byte> payload(result.payload_);
    const std::uint32_t count = read_u32(payload, 0);
    if (count > std::numeric_limits<std::size_t>::max() / 8U) {
        throw PsxFormatError("animation table count exceeds its tag");
    }
    const std::size_t record_bytes = static_cast<std::size_t>(count) * 8U;
    if (record_bytes > payload.size() - 4U) {
        throw PsxFormatError("animation table count exceeds its tag");
    }
    result.records_.reserve(static_cast<std::size_t>(count));
    result.frame_counts_.reserve(static_cast<std::size_t>(count));
    std::uint32_t previous_offset = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t offset = 4U + static_cast<std::size_t>(index) * 8U;
        const PsxAnimationRecord record{
            read_u32(payload, offset),
            read_u32(payload, offset + 4U),
        };
        if (record.frame_count() == 0 || record.frame_count() > 255) {
            throw PsxFormatError("animation record has an invalid frame count");
        }
        if (record.relative_data_offset >= payload.size()
            || (index != 0 && record.relative_data_offset <= previous_offset)) {
            throw PsxFormatError("animation record offsets are not increasing");
        }
        result.records_.push_back(record);
        result.frame_counts_.push_back(
            static_cast<std::uint8_t>(record.frame_count()));
        previous_offset = record.relative_data_offset;
    }
    if (count != 0
        && result.records_.front().relative_data_offset
            < 4U + record_bytes) {
        throw PsxFormatError("animation streams overlap the record table");
    }

    const PsxTag& hierarchy_tag = find_tag(
        archive,
        kHierarchyTag,
        "HIER hierarchy tag");
    const std::span<const std::byte> hierarchy = tag_payload(
        archive,
        hierarchy_tag);
    if ((hierarchy.size() & 1U) != 0) {
        throw PsxFormatError("animation hierarchy has an odd byte count");
    }
    result.hierarchy_words_.reserve(hierarchy.size() / 2U);
    for (std::size_t offset = 0; offset < hierarchy.size(); offset += 2U) {
        result.hierarchy_words_.push_back(read_u16(hierarchy, offset));
    }
    result.part_count_ = archive.models().size();
    if (result.part_count_ != 0
        && result.hierarchy_words_.size() < result.part_count_) {
        throw PsxFormatError("animation hierarchy has too few part words");
    }
    return result;
}

const PsxAnimationRecord& PsxAnimationTable::record(
    std::uint16_t animation) const {
    if (static_cast<std::size_t>(animation) >= records_.size()) {
        throw std::out_of_range("animation ID is outside the animation table");
    }
    return records_[animation];
}

std::span<const std::byte> PsxAnimationTable::stream(
    std::uint16_t animation) const {
    const std::size_t index = static_cast<std::size_t>(animation);
    const PsxAnimationRecord& selected = record(animation);
    const std::size_t begin = selected.relative_data_offset;
    const std::size_t end = index + 1U < records_.size()
        ? records_[index + 1U].relative_data_offset
        : payload_.size();
    if (begin >= end || end > payload_.size()) {
        throw PsxFormatError("animation stream has an invalid byte range");
    }
    return std::span<const std::byte>(payload_).subspan(begin, end - begin);
}

} // namespace opentony::assets
