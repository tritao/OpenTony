#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class FntFormatError final : public std::runtime_error {
public:
    explicit FntFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kRuntimeFontAllocationSize = 0x14c;
inline constexpr std::size_t kRuntimeFontGlyphEntrySize = 0x08;
inline constexpr std::size_t kRuntimeFontGlyphImageSize = 0x5c;

struct FntDiskGlyphRecord {
    std::array<std::uint32_t, 4> words{};
};

struct RuntimeFontGlyphEntry {
    std::uint8_t source_byte_0{};
    std::uint8_t source_byte_1{};
    std::uint8_t source_byte_2{};
    std::size_t source_record{};
    bool sentinel{};
};

// Bounded native representation of the runtime font constructor. It keeps
// source records/palette/glyph bytes value-owned and exposes the proven
// runtime table sizes and copied entry bytes without inventing the graphics
// library's glyph decoding or texture pointers.
class FntRuntimeFont final {
public:
    static FntRuntimeFont load(const std::string& path);
    static FntRuntimeFont parse(
        std::vector<std::byte> bytes,
        std::string source = {},
        std::uint32_t atlas_format_word = 0);

    [[nodiscard]] std::uint32_t glyph_count() const noexcept {
        return static_cast<std::uint32_t>(records_.size());
    }
    [[nodiscard]] const std::vector<FntDiskGlyphRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] const std::array<std::byte, 0x20>& palette_bytes() const noexcept {
        return palette_bytes_;
    }
    [[nodiscard]] std::span<const std::byte> packed_glyph_data() const noexcept {
        return packed_glyph_data_;
    }
    [[nodiscard]] const std::vector<RuntimeFontGlyphEntry>& entries() const noexcept {
        return entries_;
    }
    [[nodiscard]] std::uint32_t atlas_format_word() const noexcept {
        return atlas_format_word_;
    }
    [[nodiscard]] std::size_t font_allocation_size() const noexcept {
        return kRuntimeFontAllocationSize;
    }
    [[nodiscard]] std::size_t entry_table_allocation_size() const noexcept {
        return entries_.size() * kRuntimeFontGlyphEntrySize;
    }
    [[nodiscard]] std::size_t glyph_image_allocation_size() const noexcept {
        return entries_.size() * kRuntimeFontGlyphImageSize;
    }

private:
    std::string source_;
    std::uint32_t atlas_format_word_{};
    std::vector<FntDiskGlyphRecord> records_;
    std::array<std::byte, 0x20> palette_bytes_{};
    std::vector<std::byte> packed_glyph_data_;
    std::vector<RuntimeFontGlyphEntry> entries_;
};

} // namespace opentony::assets
