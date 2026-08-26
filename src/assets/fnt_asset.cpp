#include "fnt_asset.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint32_t u32(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw FntFormatError("FNT u32 is truncated: " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

} // namespace

FntRuntimeFont FntRuntimeFont::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw FntFormatError("cannot open FNT file: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw FntFormatError("cannot determine FNT file size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw FntFormatError("cannot read FNT file: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

FntRuntimeFont FntRuntimeFont::parse(
    std::vector<std::byte> bytes,
    std::string source,
    std::uint32_t atlas_format_word) {
    if (bytes.size() <= 0x28U) {
        throw FntFormatError("FNT file is too short: " + source);
    }
    const std::uint32_t count = u32(bytes, 0, source);
    if (count > (bytes.size() - 4U - 0x20U) / 0x10U) {
        throw FntFormatError("FNT record count is unreasonably large: " + source);
    }
    const std::size_t records_offset = 4U;
    const std::size_t palette_offset = records_offset
        + static_cast<std::size_t>(count) * 0x10U;
    if (palette_offset > bytes.size() || bytes.size() - palette_offset < 0x20U) {
        throw FntFormatError("FNT palette table is truncated: " + source);
    }

    FntRuntimeFont result{};
    result.source_ = std::move(source);
    result.atlas_format_word_ = atlas_format_word;
    result.records_.reserve(static_cast<std::size_t>(count));
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t offset = records_offset + static_cast<std::size_t>(index) * 0x10U;
        FntDiskGlyphRecord record{};
        for (std::size_t word = 0; word < record.words.size(); ++word) {
            record.words[word] = u32(bytes, offset + word * 4U, result.source_);
        }
        result.records_.push_back(record);
    }
    std::copy_n(
        bytes.begin() + static_cast<std::ptrdiff_t>(palette_offset),
        result.palette_bytes_.size(),
        result.palette_bytes_.begin());
    const std::size_t glyph_data_offset = palette_offset + result.palette_bytes_.size();
    result.packed_glyph_data_.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(glyph_data_offset),
        bytes.end());

    result.entries_.reserve(static_cast<std::size_t>(count) + 1U);
    for (std::size_t index = 0; index < result.records_.size(); ++index) {
        const auto& record = result.records_[index].words;
        result.entries_.push_back(RuntimeFontGlyphEntry{
            static_cast<std::uint8_t>(record[3]),
            static_cast<std::uint8_t>(record[1]),
            static_cast<std::uint8_t>(record[2]),
            index,
            false,
        });
    }
    // Retail allocates one extra blank/sentinel entry. Its graphics object is
    // constructed from trailing constructor defaults rather than a new disk
    // record, so leave the copied source bytes zero.
    result.entries_.push_back(RuntimeFontGlyphEntry{
        0, 0, 0, result.records_.size(), true});
    return result;
}

} // namespace opentony::assets
