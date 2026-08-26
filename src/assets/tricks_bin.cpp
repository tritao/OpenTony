#include "tricks_bin.hpp"

#include <fstream>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::int16_t read_i16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept {
    const std::uint16_t raw = static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
    return static_cast<std::int16_t>(raw);
}

[[nodiscard]] std::optional<std::span<const std::uint8_t>> bounded_sequence_table(
    std::span<const std::uint8_t> bytes,
    std::int16_t offset) noexcept {
    if (offset < 0 || static_cast<std::size_t>(offset) >= bytes.size()) {
        return std::nullopt;
    }

    const std::size_t start = static_cast<std::size_t>(offset);
    std::size_t cursor = start;
    // The observed PC table has 596 records. Keep the scan bounded while
    // leaving room for alternate builds with a larger source table.
    for (std::size_t record = 0; record < 4096; ++record) {
        if (bytes.size() - cursor < sizeof(std::int16_t)) {
            return std::nullopt;
        }
        const std::int16_t length = read_i16(bytes, cursor);
        if (length == 0) {
            return bytes.subspan(start, cursor - start + sizeof(std::int16_t));
        }
        if (length < 0 || length > 12) {
            return std::nullopt;
        }
        const std::size_t record_bytes =
            (static_cast<std::size_t>(length) + 3U) * sizeof(std::int16_t);
        if (bytes.size() - cursor < record_bytes) {
            return std::nullopt;
        }
        cursor += record_bytes;
    }
    return std::nullopt;
}

} // namespace

TricksBinView parse_tricks_bin(
    std::span<const std::uint8_t> bytes) noexcept {
    TricksBinView result{};
    result.bytes = bytes;
    if (bytes.size() < sizeof(std::int16_t) * 8) {
        return result;
    }

    for (std::size_t index = 0; index < result.header.section_offsets.size(); ++index) {
        result.header.section_offsets[index] = read_i16(bytes, index * 2);
        const std::int32_t offset = result.header.section_offsets[index];
        if (offset < 0 || static_cast<std::size_t>(offset) >= bytes.size()) {
            return result;
        }
    }
    result.valid = true;
    return result;
}

TricksBinArchive TricksBinArchive::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw TricksBinFormatError("cannot open tricks.bin: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw TricksBinFormatError("cannot determine tricks.bin size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw TricksBinFormatError("cannot read tricks.bin: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

TricksBinArchive TricksBinArchive::parse(
    std::vector<std::uint8_t> bytes,
    std::string source) {
    const TricksBinView view = parse_tricks_bin(bytes);
    if (!view.valid) {
        throw TricksBinFormatError(
            source.empty() ? "invalid tricks.bin image" : "invalid tricks.bin: " + source);
    }
    TricksBinArchive archive{};
    archive.bytes_ = std::move(bytes);
    archive.source_ = std::move(source);
    return archive;
}

std::optional<std::int16_t> TricksBinView::action_lookup(
    std::size_t index) const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    const std::size_t table_offset = static_cast<std::size_t>(
        static_cast<std::uint16_t>(header.action_offset_table_offset()));
    if (index > (bytes.size() - table_offset) / sizeof(std::int16_t) ||
        table_offset + index * sizeof(std::int16_t) + sizeof(std::int16_t) > bytes.size()) {
        return std::nullopt;
    }
    return read_i16(bytes, table_offset + index * sizeof(std::int16_t));
}

std::optional<std::span<const std::uint8_t>>
TricksBinView::player_input_sequence_table(std::size_t index) const noexcept {
    const std::optional<std::int16_t> relative = action_lookup(index);
    if (!relative.has_value()) {
        return std::nullopt;
    }
    return bounded_sequence_table(bytes, *relative);
}

std::optional<std::span<const std::uint8_t>> TricksBinView::action_stream(
    std::int16_t relative_offset) const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    const std::int32_t absolute = static_cast<std::int32_t>(relative_offset);
    if (absolute < 0 || static_cast<std::size_t>(absolute) >= bytes.size()) {
        return std::nullopt;
    }
    return bytes.subspan(static_cast<std::size_t>(absolute));
}

std::optional<std::span<const std::uint8_t>> TricksBinView::action_stream_for_lookup(
    std::size_t index) const noexcept {
    const std::optional<std::int16_t> relative = action_lookup(index);
    if (!relative.has_value()) {
        return std::nullopt;
    }
    return action_stream(*relative);
}

std::optional<std::span<const std::uint8_t>> TricksBinView::source_sequence_table()
    const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    return bounded_sequence_table(bytes, header.section_offsets[5]);
}

std::optional<std::span<const std::uint8_t>> TricksBinView::alternate_sequence_table()
    const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    return bounded_sequence_table(bytes, header.section_offsets[6]);
}

std::optional<std::span<const std::uint8_t>>
TricksBinView::special_other_resource_table() const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    const std::size_t offset = static_cast<std::size_t>(
        static_cast<std::uint16_t>(header.section_offsets[7]));
    if (offset >= bytes.size()) {
        return std::nullopt;
    }
    return bytes.subspan(offset);
}

std::optional<std::span<const std::uint8_t>> TricksBinView::player_sequence_table()
    const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    const std::size_t offset = static_cast<std::size_t>(
        static_cast<std::uint16_t>(header.section_offsets[3]));
    return bytes.subspan(offset);
}

std::optional<std::span<const std::uint8_t>> TricksBinView::other_player_sequence_table()
    const noexcept {
    if (!valid) {
        return std::nullopt;
    }
    const std::size_t offset = static_cast<std::size_t>(
        static_cast<std::uint16_t>(header.section_offsets[4]));
    return bytes.subspan(offset);
}

} // namespace opentony::assets
