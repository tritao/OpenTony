#include "replay_asset.hpp"

#include <fstream>
#include <limits>

namespace opentony::assets {
namespace {

[[nodiscard]] std::uint16_t u16(
    const std::vector<std::byte>& bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 2U) {
        throw ReplayFormatError("replay u16 is truncated: " + source);
    }
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::uint32_t u32(
    std::span<const std::byte> bytes,
    std::size_t offset,
    const std::string& source) {
    if (offset > bytes.size() || bytes.size() - offset < 4U) {
        throw ReplayFormatError("replay u32 is truncated: " + source);
    }
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

[[nodiscard]] std::int32_t signed_value(std::uint32_t value, unsigned count) {
    if (count == 0U || count > 32U) {
        throw ReplayFormatError("replay signed field width is invalid");
    }
    if (count == 32U) {
        return static_cast<std::int32_t>(value);
    }
    const std::uint32_t sign = std::uint32_t{1} << (count - 1U);
    if ((value & sign) == 0) {
        return static_cast<std::int32_t>(value);
    }
    return static_cast<std::int32_t>(value | (~std::uint32_t{0} << count));
}

} // namespace

std::uint32_t ReplayBitReader::read_bits(unsigned count) {
    if (count == 0U || count > 32U || remaining_bits() < count) {
        throw ReplayFormatError("replay bit stream is truncated");
    }
    std::uint32_t value = 0;
    for (unsigned bit = 0; bit < count; ++bit) {
        const std::size_t position = bit_offset_ + bit;
        const std::uint8_t byte = std::to_integer<std::uint8_t>(bytes_[position / 8U]);
        value |= static_cast<std::uint32_t>((byte >> (position % 8U)) & 1U) << bit;
    }
    bit_offset_ += count;
    return value;
}

std::int32_t ReplayBitReader::read_signed(unsigned count) {
    return signed_value(read_bits(count), count);
}

ReplayFrame ReplayBitReader::read_frame() {
    ReplayFrame frame{};
    for (std::int32_t& value : frame.fixed_channels) {
        value = read_signed(20U) << 12;
    }
    for (std::int16_t& value : frame.narrow_channels) {
        value = static_cast<std::int16_t>(read_signed(16U));
    }
    return frame;
}

ReplayAsset ReplayAsset::load(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw ReplayFormatError("cannot open replay: " + path);
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw ReplayFormatError("cannot determine replay size: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), length);
        if (!input) {
            throw ReplayFormatError("cannot read replay: " + path);
        }
    }
    return parse(std::move(bytes), path);
}

ReplayAsset ReplayAsset::parse(std::vector<std::byte> bytes, std::string source) {
    if (bytes.size() < kReplayHeaderSize) {
        throw ReplayFormatError("replay header is truncated: " + source);
    }
    ReplayAsset result{};
    result.bytes_ = std::move(bytes);
    result.source_ = std::move(source);
    const std::span<const std::byte> view(result.bytes_);
    result.header_.num_frames = u32(view, 0x00, result.source_);
    for (std::size_t index = 0; index < kReplayHighlightCount; ++index) {
        const std::size_t offset = 0x04U + index * 8U;
        result.header_.highlights[index].start = static_cast<std::int32_t>(
            u32(view, offset, result.source_));
        result.header_.highlights[index].end = static_cast<std::int32_t>(
            u32(view, offset + 4U, result.source_));
    }
    result.header_.num_skaters = u16(result.bytes_, 0x2c, result.source_);
    if (result.header_.num_skaters == 0 || result.header_.num_skaters > 2) {
        throw ReplayFormatError("replay skater count is outside the retail range: " + result.source_);
    }
    result.header_.skater = std::to_integer<std::uint8_t>(result.bytes_[0x2e]);
    result.header_.skater2 = std::to_integer<std::uint8_t>(result.bytes_[0x2f]);
    result.header_.reserved_30 = {
        result.bytes_[0x30], result.bytes_[0x31]};
    result.header_.level = u16(result.bytes_, 0x32, result.source_);
    result.header_.game = u16(result.bytes_, 0x34, result.source_);
    return result;
}

std::span<const std::byte> ReplayAsset::card_transfer(std::size_t player) const {
    if (player > 1U) {
        throw ReplayFormatError("replay player index is outside the two-player range: " + source_);
    }
    const std::size_t offset = player * kReplayCardTransferSize;
    if (offset > bytes_.size() || kReplayCardTransferSize > bytes_.size() - offset) {
        throw ReplayFormatError("replay card transfer is truncated: " + source_);
    }
    return std::span<const std::byte>(bytes_).subspan(offset, kReplayCardTransferSize);
}

} // namespace opentony::assets
