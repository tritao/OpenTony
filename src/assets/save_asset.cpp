#include "save_asset.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint32_t u32(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

void require_range(
    std::span<const std::byte> bytes,
    std::size_t offset,
    std::size_t size,
    const std::string& source) {
    if (offset > bytes.size() || size > bytes.size() - offset) {
        throw SaveFormatError("save range is outside the file: " + source);
    }
}

[[nodiscard]] SaveActionType checked_action(std::uint8_t value, const std::string& source) {
    switch (value) {
    case 1:
        return SaveActionType::career;
    case 2:
        return SaveActionType::replay;
    case 3:
        return SaveActionType::custom_park;
    default:
        throw SaveFormatError("save action type is unsupported: " + source);
    }
}

[[nodiscard]] std::size_t aligned_size(std::size_t value, const std::string& source) {
    if (value > std::numeric_limits<std::size_t>::max() - (kSaveBlockAlignment - 1U)) {
        throw SaveFormatError("save size overflows alignment: " + source);
    }
    return (value + (kSaveBlockAlignment - 1U)) & ~(kSaveBlockAlignment - 1U);
}

} // namespace

SaveGameFile SaveGameFile::parse(std::vector<std::byte> bytes, std::string source) {
    const std::span<const std::byte> view(bytes);
    if (view.size() < kSaveHeaderSize || view[0] != std::byte{'S'}
        || view[1] != std::byte{'C'}) {
        throw SaveFormatError("save header does not begin with SC: " + source);
    }
    if (view[2] != static_cast<std::byte>(std::to_integer<std::uint8_t>(view[3]) + 0x10U)) {
        throw SaveFormatError("save header action marker is invalid: " + source);
    }
    if (bytes.size() % kSaveBlockAlignment != 0
        || bytes.size() % kSaveCardBlockSize != 0
        || bytes.size() < kSaveHeaderSize) {
        throw SaveFormatError("save size is not card-block aligned: " + source);
    }
    SaveGameFile result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    const std::span<const std::byte> saved(result.bytes_);
    result.header_.action_type = checked_action(
        std::to_integer<std::uint8_t>(saved[3]), result.source_);
    std::copy_n(
        reinterpret_cast<const char*>(saved.data() + kSaveHeaderNameOffset),
        kSaveHeaderNameSize,
        result.header_.name.begin());
    for (std::size_t block = 0; block < kSaveHeaderBlockCount; ++block) {
        std::copy_n(
            saved.begin() + static_cast<std::ptrdiff_t>(kSaveHeaderBlockOffset
                + block * kSaveHeaderBlockSize),
            kSaveHeaderBlockSize,
            result.header_.blocks[block].begin());
    }
    return result;
}

SaveGameFile SaveGameFile::build(
    SaveActionType action_type,
    std::string_view name,
    const std::array<std::array<std::byte, kSaveHeaderBlockSize>, kSaveHeaderBlockCount>& blocks,
    std::vector<std::byte> payload,
    std::string source) {
    if (name.size() >= kSaveHeaderNameSize) {
        throw SaveFormatError("save name exceeds the 32-byte header field: " + source);
    }
    const std::size_t payload_size = aligned_size(payload.size(), source);
    payload.resize(payload_size, std::byte{0});
    if ((kSaveHeaderSize + payload.size()) % kSaveCardBlockSize != 0) {
        throw SaveFormatError("save payload does not fill whole card blocks: " + source);
    }
    SaveGameFile result{};
    result.source_ = std::move(source);
    result.bytes_.assign(kSaveHeaderSize, std::byte{0});
    result.bytes_.resize(kSaveHeaderSize + payload.size(), std::byte{0});
    result.bytes_[0] = std::byte{'S'};
    result.bytes_[1] = std::byte{'C'};
    const auto action = static_cast<std::uint8_t>(action_type);
    if (action < 1U || action > 3U) {
        throw SaveFormatError("save action type is unsupported: " + result.source_);
    }
    result.bytes_[2] = static_cast<std::byte>(0x10U + action);
    result.bytes_[3] = static_cast<std::byte>(action);
    std::copy(name.begin(), name.end(), reinterpret_cast<char*>(
        result.bytes_.data() + kSaveHeaderNameOffset));
    for (std::size_t block = 0; block < kSaveHeaderBlockCount; ++block) {
        std::copy(
            blocks[block].begin(), blocks[block].end(),
            result.bytes_.begin() + static_cast<std::ptrdiff_t>(
                kSaveHeaderBlockOffset + block * kSaveHeaderBlockSize));
    }
    std::copy(
        payload.begin(), payload.end(),
        result.bytes_.begin() + static_cast<std::ptrdiff_t>(kSaveHeaderSize));
    return parse(std::move(result.bytes_), result.source_);
}

std::uint32_t CareerSkaterRecord::trick_unlock_bits() const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw[0x34])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw[0x35])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw[0x36])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw[0x37])) << 24U));
}

std::array<std::byte, 10> CareerSkaterRecord::raw_trick_stat_seeds() const noexcept {
    std::array<std::byte, 10> result{};
    std::copy_n(raw.begin() + 0x38, result.size(), result.begin());
    return result;
}

std::array<std::byte, 0x35> CareerSkaterRecord::custom_appearance() const noexcept {
    std::array<std::byte, 0x35> result{};
    std::copy_n(raw.begin() + 0xcc, result.size(), result.begin());
    return result;
}

CareerSaveImage CareerSaveImage::parse(
    std::span<const std::byte> bytes,
    std::string source) {
    if (bytes.size() < kCareerImageSize) {
        throw SaveFormatError("career image is shorter than 0x1dc4 bytes: " + source);
    }
    CareerSaveImage result{};
    result.source_ = std::move(source);
    result.image_bytes_.assign(bytes.begin(), bytes.begin() + kCareerImageSize);
    const std::span<const std::byte> image(result.image_bytes_);
    result.format_magic_ = u32(image, 0);
    result.active_skater_selectors_ = {
        std::to_integer<std::uint8_t>(image[0x1870]),
        std::to_integer<std::uint8_t>(image[0x1871])};
    result.active_costume_selectors_ = {
        std::to_integer<std::uint8_t>(image[0x1872]),
        std::to_integer<std::uint8_t>(image[0x1873])};
    result.skater_records_.reserve(kCareerRecordCount);
    for (std::size_t index = 0; index < kCareerRecordCount; ++index) {
        const std::size_t offset = kCareerRecordOffset + index * kCareerRecordStride;
        require_range(image, offset, kCareerRecordStride, result.source_);
        CareerSkaterRecord record{};
        std::copy_n(
            image.begin() + static_cast<std::ptrdiff_t>(offset),
            kCareerRecordStride,
            record.raw.begin());
        result.skater_records_.push_back(record);
    }
    return result;
}

} // namespace opentony::assets
