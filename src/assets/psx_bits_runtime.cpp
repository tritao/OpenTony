#include "psx_bits_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint32_t read_u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw PsxFormatError("BITS tag has a truncated u32");
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::string read_name(
    const std::vector<std::byte>& bytes,
    std::size_t offset) {
    std::string name;
    name.reserve(kPsxBitsGroupNameSize);
    for (std::size_t index = 0; index < kPsxBitsGroupNameSize; ++index) {
        const char value = static_cast<char>(
            std::to_integer<std::uint8_t>(bytes[offset + index]));
        if (value == '\0') {
            break;
        }
        name.push_back(value);
    }
    return name;
}

[[nodiscard]] bool equal_folded(
    const std::string& left,
    const std::string& right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    return std::equal(
        left.begin(), left.end(), right.begin(),
        [](char a, char b) {
            return std::tolower(static_cast<unsigned char>(a))
                == std::tolower(static_cast<unsigned char>(b));
        });
}

} // namespace

void PsxBitsRuntime::build(const PsxArchive& archive) {
    archive_ = &archive;
    groups_.clear();
    const auto tag = std::find_if(
        archive.tags().begin(),
        archive.tags().end(),
        [](const PsxTag& value) { return value.type == 0x45U; });
    if (tag == archive.tags().end()) {
        throw PsxFormatError("PSX archive has no BITS type-0x45 tag");
    }
    const std::size_t payload = tag->offset + 8U;
    if (payload > archive.bytes().size() || tag->size > archive.bytes().size() - payload) {
        throw PsxFormatError("BITS tag payload is outside the PSX archive");
    }
    const std::size_t end = payload + tag->size;
    std::size_t cursor = payload;
    const std::uint32_t group_count = read_u32(archive.bytes(), cursor);
    cursor += 4U;
    if (group_count > (end - cursor) / 12U) {
        throw PsxFormatError("BITS group count is unreasonably large");
    }
    groups_.reserve(static_cast<std::size_t>(group_count));
    for (std::uint32_t group_index = 0; group_index < group_count; ++group_index) {
        if (cursor > end || end - cursor < kPsxBitsGroupNameSize + 4U) {
            throw PsxFormatError("BITS group header is truncated");
        }
        PsxBitsGroup group{};
        group.name = read_name(archive.bytes(), cursor);
        cursor += kPsxBitsGroupNameSize;
        const std::uint32_t entry_count = read_u32(archive.bytes(), cursor);
        cursor += 4U;
        if (entry_count > (end - cursor) / kPsxBitsEntrySize) {
            throw PsxFormatError("BITS group entries are unreasonably large");
        }
        group.entries.reserve(static_cast<std::size_t>(entry_count));
        for (std::uint32_t entry_index = 0; entry_index < entry_count; ++entry_index) {
            std::array<std::byte, kPsxBitsEntrySize> entry{};
            std::copy_n(
                archive.bytes().begin() + static_cast<std::ptrdiff_t>(cursor),
                kPsxBitsEntrySize,
                entry.begin());
            cursor += kPsxBitsEntrySize;
            group.entries.push_back(entry);
        }
        groups_.push_back(std::move(group));
    }
    if (cursor != end) {
        throw PsxFormatError("BITS tag has trailing payload bytes");
    }
}

const PsxBitsGroup* PsxBitsRuntime::find(const std::string& name) const noexcept {
    const auto found = std::find_if(
        groups_.begin(), groups_.end(),
        [&name](const PsxBitsGroup& group) {
            return equal_folded(group.name, name);
        });
    return found == groups_.end() ? nullptr : &*found;
}

} // namespace opentony::assets
