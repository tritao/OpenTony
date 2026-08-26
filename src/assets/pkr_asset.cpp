#include "pkr_asset.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <zlib.h>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint32_t u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw PkrFormatError("PKR u32 is truncated: " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::string fixed_name(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    std::size_t length,
    const std::string& source,
    bool allow_path_separators = false) {
    if (offset > bytes.size() || length > bytes.size() - offset) {
        throw PkrFormatError("PKR name is truncated: " + source);
    }
    std::size_t end = 0;
    while (end < length && bytes[offset + end] != std::byte{0}) {
        ++end;
    }
    if (end == 0 || end == length) {
        throw PkrFormatError("PKR name is empty or unterminated: " + source);
    }
    std::string result;
    result.reserve(end);
    for (std::size_t index = 0; index < end; ++index) {
        const unsigned char character =
            std::to_integer<unsigned char>(bytes[offset + index]);
        if (character == '\\' || (!allow_path_separators && character == '/')) {
            throw PkrFormatError("PKR name contains a path separator: " + source);
        }
        if (allow_path_separators && character == '/' && index + 1 < end
            && std::to_integer<unsigned char>(bytes[offset + index + 1]) == '/') {
            throw PkrFormatError("PKR directory contains an empty component: " + source);
        }
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] std::size_t checked_end(
    std::size_t offset,
    std::uint32_t size,
    std::size_t file_size,
    const std::string& source) {
    if (offset > file_size || static_cast<std::size_t>(size) > file_size - offset) {
        throw PkrFormatError("PKR payload is outside the archive: " + source);
    }
    return offset + static_cast<std::size_t>(size);
}

[[nodiscard]] std::vector<std::byte> decode_rle8(
    std::span<const std::byte> stored,
    std::size_t decoded_size,
    const std::string& source) {
    std::vector<std::byte> decoded;
    decoded.reserve(decoded_size);
    std::size_t cursor = 0;
    while (cursor < stored.size()) {
        if (stored.size() - cursor < 2U) {
            throw PkrFormatError("PKR BIBD stream is truncated: " + source);
        }
        const std::size_t count = std::to_integer<std::uint8_t>(stored[cursor++]);
        if (count == 0 || decoded.size() > decoded_size
            || count > decoded_size - decoded.size()) {
            throw PkrFormatError("PKR BIBD run exceeds decoded size: " + source);
        }
        const std::byte value = stored[cursor++];
        decoded.insert(decoded.end(), count, value);
    }
    if (decoded.size() != decoded_size) {
        throw PkrFormatError("PKR BIBD stream ends before decoded size: " + source);
    }
    return decoded;
}

[[nodiscard]] std::vector<std::byte> decode_rle16(
    std::span<const std::byte> stored,
    std::size_t decoded_size,
    const std::string& source) {
    std::vector<std::byte> decoded;
    decoded.reserve(decoded_size);
    std::size_t cursor = 0;
    while (cursor < stored.size()) {
        if (stored.size() - cursor < 3U) {
            throw PkrFormatError("PKR WIBD stream is truncated: " + source);
        }
        const std::size_t count = static_cast<std::size_t>(
            std::to_integer<std::uint8_t>(stored[cursor])
            | (static_cast<std::uint16_t>(
                std::to_integer<std::uint8_t>(stored[cursor + 1])) << 8U));
        cursor += 2U;
        if (count == 0 || decoded.size() > decoded_size
            || count > decoded_size - decoded.size()) {
            throw PkrFormatError("PKR WIBD run exceeds decoded size: " + source);
        }
        const std::byte value = stored[cursor++];
        decoded.insert(decoded.end(), count, value);
    }
    if (decoded.size() != decoded_size) {
        throw PkrFormatError("PKR WIBD stream ends before decoded size: " + source);
    }
    return decoded;
}

[[nodiscard]] std::vector<std::byte> decode_zlib(
    std::span<const std::byte> stored,
    std::size_t decoded_size,
    const std::string& source) {
    if (decoded_size > std::numeric_limits<uLong>::max()
        || stored.size() > std::numeric_limits<uLong>::max()) {
        throw PkrFormatError("PKR ZLIB buffer is too large: " + source);
    }
    std::vector<std::byte> decoded(decoded_size);
    uLongf output_size = static_cast<uLongf>(decoded_size);
    const int result = ::uncompress(
        reinterpret_cast<Bytef*>(decoded.data()),
        &output_size,
        reinterpret_cast<const Bytef*>(stored.data()),
        static_cast<uLong>(stored.size()));
    if (result != Z_OK || output_size != decoded_size) {
        throw PkrFormatError("PKR ZLIB decode failed: " + source);
    }
    return decoded;
}

} // namespace

PkrArchive PkrArchive::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw PkrFormatError("cannot open PKR file: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw PkrFormatError("cannot determine PKR file size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw PkrFormatError("cannot read PKR file: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

PkrArchive PkrArchive::parse(
    std::vector<std::byte> bytes,
    std::string source) {
    if (bytes.size() < 16U
        || std::string(reinterpret_cast<const char*>(bytes.data()), 4) != "PKR2") {
        throw PkrFormatError("PKR header is not PKR2: " + source);
    }
    PkrArchive result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    result.version_ = u32(result.bytes_, 4, result.source_);
    const std::uint32_t directory_count = u32(result.bytes_, 8, result.source_);
    const std::uint32_t file_count = u32(result.bytes_, 12, result.source_);
    const std::size_t directory_table_end = 16U
        + static_cast<std::size_t>(directory_count) * 40U;
    if (directory_table_end < 16U || directory_table_end > result.bytes_.size()) {
        throw PkrFormatError("PKR directory table is outside the archive: " + result.source_);
    }
    result.directories_.reserve(static_cast<std::size_t>(directory_count));
    for (std::uint32_t index = 0; index < directory_count; ++index) {
        const std::size_t offset = 16U + static_cast<std::size_t>(index) * 40U;
        PkrDirectory directory{};
        directory.name = fixed_name(
            result.bytes_, offset, 32U, result.source_, true);
        directory.entries_offset = u32(result.bytes_, offset + 32U, result.source_);
        directory.file_count = u32(result.bytes_, offset + 36U, result.source_);
        const std::size_t entries_size = static_cast<std::size_t>(directory.file_count) * 48U;
        if (entries_size / 48U != directory.file_count
            || directory.entries_offset < directory_table_end
            || directory.entries_offset > result.bytes_.size()
            || entries_size > result.bytes_.size() - directory.entries_offset) {
            throw PkrFormatError("PKR directory entries are outside the archive: " + result.source_);
        }
        result.directories_.push_back(std::move(directory));
    }

    result.entries_.reserve(static_cast<std::size_t>(file_count));
    for (const PkrDirectory& directory : result.directories_) {
        for (std::uint32_t index = 0; index < directory.file_count; ++index) {
            const std::size_t offset = directory.entries_offset
                + static_cast<std::size_t>(index) * 48U;
            PkrFileEntry entry{};
            entry.directory = directory.name;
            entry.name = fixed_name(result.bytes_, offset, 32U, result.source_);
            entry.marker = u32(result.bytes_, offset + 32U, result.source_);
            if (entry.marker != kPkrRawMarker && entry.marker > 2U) {
                throw PkrFormatError("PKR entry has an unsupported marker: " + result.source_);
            }
            entry.payload_offset = u32(result.bytes_, offset + 36U, result.source_);
            entry.stored_size = u32(result.bytes_, offset + 40U, result.source_);
            entry.decoded_size = u32(result.bytes_, offset + 44U, result.source_);
            if (entry.marker == kPkrRawMarker
                && entry.stored_size != entry.decoded_size) {
                throw PkrFormatError("PKR raw entry has mismatched sizes: " + result.source_);
            }
            (void)checked_end(
                entry.payload_offset, entry.stored_size,
                result.bytes_.size(), result.source_);
            const auto duplicate = std::find_if(
                result.entries_.begin(), result.entries_.end(),
                [&entry](const PkrFileEntry& existing) {
                    return existing.archive_path() == entry.archive_path();
                });
            if (duplicate != result.entries_.end()) {
                throw PkrFormatError("PKR has a duplicate archive path: " + entry.archive_path());
            }
            result.entries_.push_back(std::move(entry));
        }
    }
    if (result.entries_.size() != file_count) {
        throw PkrFormatError("PKR file count does not match directory entries: " + result.source_);
    }
    return result;
}

const PkrFileEntry& PkrArchive::entry(std::size_t index) const {
    if (index >= entries_.size()) {
        throw PkrFormatError("PKR entry index is outside the archive: " + source_);
    }
    return entries_[index];
}

const PkrFileEntry* PkrArchive::find(std::string_view archive_path) const noexcept {
    const auto found = std::find_if(
        entries_.begin(), entries_.end(),
        [archive_path](const PkrFileEntry& entry) {
            return entry.archive_path() == archive_path;
        });
    return found == entries_.end() ? nullptr : &*found;
}

std::span<const std::byte> PkrArchive::stored_payload(std::size_t index) const {
    const PkrFileEntry& current = entry(index);
    return std::span<const std::byte>(bytes_).subspan(
        current.payload_offset, current.stored_size);
}

std::vector<std::byte> PkrArchive::decode(std::size_t index) const {
    const PkrFileEntry& current = entry(index);
    const std::span<const std::byte> stored = stored_payload(index);
    switch (current.marker) {
    case kPkrRawMarker:
        return std::vector<std::byte>(stored.begin(), stored.end());
    case 0:
        return decode_rle8(stored, current.decoded_size, source_);
    case 1:
        return decode_rle16(stored, current.decoded_size, source_);
    case 2:
        return decode_zlib(stored, current.decoded_size, source_);
    default:
        throw PkrFormatError("PKR entry marker is unsupported: " + source_);
    }
}

std::vector<std::byte> PkrArchive::decode(std::string_view archive_path) const {
    const PkrFileEntry* current = find(archive_path);
    if (current == nullptr) {
        throw PkrFormatError("PKR entry was not found: " + std::string(archive_path));
    }
    const std::size_t index = static_cast<std::size_t>(
        std::distance(entries_.data(), current));
    return decode(index);
}

} // namespace opentony::assets
