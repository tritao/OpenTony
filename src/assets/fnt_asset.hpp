#pragma once

#include "pre_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
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

struct FntTextRuntimeView {
    std::uint32_t glyph_count{};
    std::uint32_t atlas_format_word{};
    std::span<const RuntimeFontGlyphEntry> entries{};
};

// Bounded native representation of the runtime font constructor. It keeps
// source records/palette/glyph bytes value-owned and exposes the proven
// runtime table sizes and copied entry bytes without inventing the graphics
// library's glyph decoding or texture pointers.
class FntRuntimeFont final {
public:
    static FntRuntimeFont load(
        const std::string& path,
        std::uint32_t atlas_format_word = 0);
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

inline constexpr std::size_t kRuntimeFontSlotCount = 8;
inline constexpr std::size_t kRuntimeFontNameSize = 0x10;

// Native counterpart of the retail eight-slot font table. A font owns its
// parsed records and glyph data; loading an embedded PRE entry copies only the
// payload needed to construct that runtime object, matching the retail
// temporary-buffer lifetime.
class FntRuntimeManager final {
public:
    FntRuntimeManager() = default;

    std::size_t load(
        std::string name,
        std::vector<std::byte> bytes,
        std::uint32_t atlas_format_word = 0);
    std::size_t load_file(
        std::string name,
        const std::string& path,
        std::uint32_t atlas_format_word = 0);
    std::size_t load_embedded(
        const PreRuntimeManager& pre,
        std::string name,
        std::string_view resource_name,
        std::uint32_t atlas_format_word = 0);

    [[nodiscard]] const FntRuntimeFont* find(std::string_view name) const noexcept;
    // The entry span borrows the selected font and is invalid after unload of
    // that font or destruction of this manager.
    [[nodiscard]] FntTextRuntimeView text_view(std::string_view name) const;
    [[nodiscard]] const FntRuntimeFont& font(std::size_t slot) const;
    [[nodiscard]] const std::string& font_name(std::size_t slot) const;
    [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_count_; }

    void unload(std::string_view name);

private:
    struct Slot {
        std::string name;
        FntRuntimeFont font;
    };

    std::array<std::optional<Slot>, kRuntimeFontSlotCount> slots_{};
    std::size_t loaded_count_{};

    static void validate_font_name(std::string_view name);
};

} // namespace opentony::assets
