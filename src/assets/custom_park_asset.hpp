#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class CustomParkFormatError final : public std::runtime_error {
public:
    explicit CustomParkFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::uint32_t kCustomParkMagic = 0x4e25U;
inline constexpr std::uint32_t kCustomParkLegacyMagic = 0x4e24U;
inline constexpr std::size_t kCustomParkCellDiskSize = 0x08U;
inline constexpr std::size_t kCustomParkCellRuntimeSize = 0x10U;
inline constexpr std::size_t kCustomParkItemCount = 10U;
inline constexpr std::size_t kCustomParkItemDiskSize = 0x24U;
inline constexpr std::size_t kCustomParkItemRuntimeSize = 0x2cU;
inline constexpr std::size_t kCustomParkTrailingTableSize = 0x40U;

struct CustomParkDimensions {
    std::uint32_t width{};
    std::uint32_t depth{};
};

struct CustomParkDiskCell {
    std::array<std::uint8_t, 5> compact_references{};
    std::uint8_t packed_two_bit_values{};
    std::uint16_t packed_value_word{};
};

struct CustomParkCell {
    std::array<std::uint16_t, 5> translated_references{};
    std::array<std::uint8_t, 6> unpacked_cell_values{};
    std::uint8_t packed_two_bit_values{};
    std::uint16_t packed_value_word{};
};

struct CustomParkDiskItemRecord {
    std::array<std::uint8_t, 6> endpoint_axis_indices{};
    std::uint16_t packed_nibble_word{};
    std::uint16_t packed_two_bit_word{};
    std::uint8_t legacy_load_item_byte{};
    std::uint8_t current_item_byte{};
    std::array<char, 25> current_name_bytes{};
};

struct CustomParkItemRecord {
    std::array<std::uint8_t, 2> endpoint_axis_index_0{};
    std::array<std::uint8_t, 2> endpoint_axis_index_1{};
    std::array<std::uint8_t, 2> endpoint_axis_index_2{};
    std::array<std::uint8_t, 2> endpoint_two_bit_values{};
    std::array<std::uint8_t, 4> expanded_nibble_values{};
    std::uint8_t packed_item_flags{};
    std::string item_name;
    std::uint8_t item_byte{};
};

// The caller may provide the 256-entry table used by the retail
// LevelGen_TranslatePieceReference routine. With no table, ordinary compact
// references retain their byte value and 0xff maps to the observed empty
// value 0xffff; this keeps the unresolved game-data table explicit.
class CustomParkArchive final {
public:
    static CustomParkArchive load(const std::string& path);
    static CustomParkArchive parse(
        std::vector<std::byte> bytes,
        std::string source = {},
        std::span<const std::uint16_t> reference_translation = {});

    [[nodiscard]] std::uint32_t magic() const noexcept { return magic_; }
    [[nodiscard]] std::uint32_t version() const noexcept { return version_; }
    [[nodiscard]] std::uint32_t map_variant() const noexcept { return map_variant_; }
    [[nodiscard]] CustomParkDimensions dimensions() const noexcept { return dimensions_; }
    [[nodiscard]] std::size_t cell_count() const noexcept { return cells_.size(); }
    [[nodiscard]] const std::vector<CustomParkDiskCell>& disk_cells() const noexcept {
        return disk_cells_;
    }
    [[nodiscard]] const std::vector<CustomParkCell>& cells() const noexcept { return cells_; }
    [[nodiscard]] const std::vector<CustomParkDiskItemRecord>& disk_items() const noexcept {
        return disk_items_;
    }
    [[nodiscard]] const std::vector<CustomParkItemRecord>& items() const noexcept {
        return items_;
    }
    [[nodiscard]] std::span<const std::byte> trailing_table() const noexcept {
        return trailing_table_;
    }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

    [[nodiscard]] std::size_t serialized_content_size() const noexcept {
        return 0x0cU + cells_.size() * kCustomParkCellDiskSize
            + kCustomParkItemCount * kCustomParkItemDiskSize
            + kCustomParkTrailingTableSize;
    }
    [[nodiscard]] std::size_t aligned_allocation_size() const noexcept;

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::uint32_t magic_{};
    std::uint32_t version_{};
    std::uint32_t map_variant_{};
    CustomParkDimensions dimensions_{};
    std::vector<CustomParkDiskCell> disk_cells_;
    std::vector<CustomParkCell> cells_;
    std::vector<CustomParkDiskItemRecord> disk_items_;
    std::vector<CustomParkItemRecord> items_;
    std::array<std::byte, kCustomParkTrailingTableSize> trailing_table_{};
};

} // namespace opentony::assets
