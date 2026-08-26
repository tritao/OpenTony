#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::assets {

class SaveFormatError final : public std::runtime_error {
public:
    explicit SaveFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kSaveHeaderSize = 0x200U;
inline constexpr std::size_t kSaveHeaderNameOffset = 0x60U;
inline constexpr std::size_t kSaveHeaderNameSize = 0x20U;
inline constexpr std::size_t kSaveHeaderBlockOffset = 0x80U;
inline constexpr std::size_t kSaveHeaderBlockSize = 0x80U;
inline constexpr std::size_t kSaveHeaderBlockCount = 3U;
inline constexpr std::size_t kSaveBlockAlignment = 0x80U;
inline constexpr std::size_t kSaveCardBlockSize = 0x2000U;
inline constexpr std::size_t kCareerRegisteredSize = 0x1e00U;
inline constexpr std::size_t kCareerImageSize = 0x1dc4U;
inline constexpr std::size_t kCareerRecordCount = 20U;
inline constexpr std::size_t kCareerRecordOffset = 0x04U;
inline constexpr std::size_t kCareerRecordStride = 0x104U;
inline constexpr std::uint32_t kCareerFormatMagic = 0x101500U;

enum class SaveActionType : std::uint8_t {
    career = 1,
    replay = 2,
    custom_park = 3,
};

struct SaveHeader {
    SaveActionType action_type{SaveActionType::career};
    std::array<char, kSaveHeaderNameSize> name{};
    std::array<std::array<std::byte, kSaveHeaderBlockSize>, kSaveHeaderBlockCount> blocks{};
};

// Native representation of the common SC header and its 128-byte-aligned
// registered payload buffers. The payload remains type-specific; consumers
// such as CareerSaveImage decode only the buffer they own.
class SaveGameFile final {
public:
    static SaveGameFile parse(std::vector<std::byte> bytes, std::string source = {});
    static SaveGameFile build(
        SaveActionType action_type,
        std::string_view name,
        const std::array<std::array<std::byte, kSaveHeaderBlockSize>, kSaveHeaderBlockCount>& blocks,
        std::vector<std::byte> payload,
        std::string source = {});

    [[nodiscard]] const SaveHeader& header() const noexcept { return header_; }
    [[nodiscard]] std::span<const std::byte> payload() const noexcept {
        return std::span<const std::byte>(bytes_).subspan(kSaveHeaderSize);
    }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
    [[nodiscard]] std::size_t card_block_count() const noexcept {
        return bytes_.size() / kSaveCardBlockSize;
    }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    SaveHeader header_{};
};

struct CareerSkaterRecord {
    std::array<std::byte, kCareerRecordStride> raw{};

    [[nodiscard]] std::uint32_t trick_unlock_bits() const noexcept;
    [[nodiscard]] std::array<std::byte, 10> raw_trick_stat_seeds() const noexcept;
    [[nodiscard]] std::array<std::byte, 0x35> custom_appearance() const noexcept;
};

// The first 0x1dc4 bytes of the registered career buffer are the serialized
// career image. Unknown bytes remain owned raw data so later field evidence
// can be added without changing offsets or save compatibility.
class CareerSaveImage final {
public:
    static CareerSaveImage parse(std::span<const std::byte> bytes, std::string source = {});

    [[nodiscard]] std::uint32_t format_magic() const noexcept { return format_magic_; }
    [[nodiscard]] const std::array<std::uint8_t, 2>& active_skater_selectors() const noexcept {
        return active_skater_selectors_;
    }
    [[nodiscard]] const std::array<std::uint8_t, 2>& active_costume_selectors() const noexcept {
        return active_costume_selectors_;
    }
    [[nodiscard]] const std::vector<CareerSkaterRecord>& skater_records() const noexcept {
        return skater_records_;
    }
    [[nodiscard]] std::span<const std::byte> image_bytes() const noexcept { return image_bytes_; }

private:
    std::vector<std::byte> image_bytes_;
    std::string source_;
    std::uint32_t format_magic_{};
    std::array<std::uint8_t, 2> active_skater_selectors_{};
    std::array<std::uint8_t, 2> active_costume_selectors_{};
    std::vector<CareerSkaterRecord> skater_records_;
};

} // namespace opentony::assets
