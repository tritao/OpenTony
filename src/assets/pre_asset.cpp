#include "pre_asset.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint8_t byte_at(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset >= bytes.size()) {
        throw PreFormatError("PRE byte read outside " + source);
    }
    return std::to_integer<std::uint8_t>(bytes[offset]);
}

[[nodiscard]] std::uint32_t u32_at(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        throw PreFormatError("PRE u32 read outside " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::size_t align4(std::size_t value) {
    if (value > std::numeric_limits<std::size_t>::max() - 3U) {
        throw PreFormatError("PRE alignment overflows the host size");
    }
    return (value + 3U) & ~static_cast<std::size_t>(3U);
}

[[nodiscard]] bool unsafe_name(std::string_view name) noexcept {
    if (name.empty() || name.front() == '/' || name.front() == '\\') {
        return true;
    }
    std::size_t component_start = 0;
    while (component_start <= name.size()) {
        const std::size_t separator = name.find_first_of("/\\", component_start);
        const std::size_t end = separator == std::string_view::npos
            ? name.size()
            : separator;
        if (name.substr(component_start, end - component_start) == "..") {
            return true;
        }
        if (separator == std::string_view::npos) {
            break;
        }
        component_start = separator + 1;
    }
    return false;
}

} // namespace

PreArchive PreArchive::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw PreFormatError("cannot open PRE file: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw PreFormatError("cannot determine PRE file size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw PreFormatError("cannot read PRE file: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

PreArchive PreArchive::parse(std::vector<std::byte> bytes, std::string source) {
    if (bytes.size() < 4) {
        throw PreFormatError("PRE header is truncated");
    }
    PreArchive archive{};
    archive.bytes_ = std::move(bytes);
    archive.source_ = std::move(source);
    const std::uint32_t count = u32_at(archive.bytes_, 0, archive.source_);
    if (count > archive.bytes_.size() / 5U) {
        throw PreFormatError("PRE record count is unreasonably large");
    }
    archive.entries_.reserve(static_cast<std::size_t>(count));
    std::size_t cursor = 4;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t name_start = cursor;
        while (cursor < archive.bytes_.size()
               && byte_at(archive.bytes_, cursor, archive.source_) != 0) {
            ++cursor;
        }
        if (cursor == name_start) {
            throw PreFormatError("PRE record has an empty name");
        }
        if (cursor == archive.bytes_.size()) {
            throw PreFormatError("PRE record name is unterminated");
        }
        if (cursor - name_start > 4096U) {
            throw PreFormatError("PRE record name exceeds 4096 bytes");
        }
        const std::string name(
            reinterpret_cast<const char*>(archive.bytes_.data() + name_start),
            cursor - name_start);
        if (unsafe_name(name)) {
            throw PreFormatError("PRE record has an unsafe path: " + name);
        }
        cursor = align4(cursor + 1U);
        if (cursor > archive.bytes_.size() || archive.bytes_.size() - cursor < 4U) {
            throw PreFormatError("PRE record size is truncated: " + name);
        }
        const std::uint32_t size = u32_at(archive.bytes_, cursor, archive.source_);
        cursor += 4U;
        const std::size_t data_offset = cursor;
        if (size > archive.bytes_.size() - data_offset) {
            throw PreFormatError("PRE payload exceeds the file: " + name);
        }
        const auto duplicate = std::find_if(
            archive.entries_.begin(),
            archive.entries_.end(),
            [&name](const PreEntry& entry) { return entry.name == name; });
        if (duplicate != archive.entries_.end()) {
            throw PreFormatError("PRE contains a duplicate path: " + name);
        }
        archive.entries_.push_back(PreEntry{name, data_offset, size});
        cursor = align4(data_offset + static_cast<std::size_t>(size));
        if (cursor > archive.bytes_.size()) {
            throw PreFormatError("PRE payload alignment exceeds the file: " + name);
        }
    }
    return archive;
}

const PreEntry& PreArchive::entry(std::size_t index) const {
    if (index >= entries_.size()) {
        throw PreFormatError("PRE entry index is outside the archive");
    }
    return entries_[index];
}

const PreEntry* PreArchive::find(std::string_view name) const noexcept {
    const auto found = std::find_if(
        entries_.begin(),
        entries_.end(),
        [name](const PreEntry& entry) { return entry.name == name; });
    return found == entries_.end() ? nullptr : &*found;
}

std::span<const std::byte> PreArchive::payload(std::size_t index) const {
    const PreEntry& current = entry(index);
    return std::span<const std::byte>(bytes_).subspan(current.data_offset, current.size);
}

std::span<const std::byte> PreArchive::payload(std::string_view name) const {
    const PreEntry* current = find(name);
    if (current == nullptr) {
        throw PreFormatError("PRE entry was not found: " + std::string(name));
    }
    return std::span<const std::byte>(bytes_).subspan(current->data_offset, current->size);
}

} // namespace opentony::assets
