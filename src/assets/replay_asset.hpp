#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class ReplayFormatError final : public std::runtime_error {
public:
    explicit ReplayFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kReplayHeaderSize = 0x38U;
inline constexpr std::size_t kReplayHighlightCount = 5U;
inline constexpr std::size_t kReplayCardBufferSize = 0x80000U;
inline constexpr std::size_t kReplayCardTransferSize = 0x7fe00U;
inline constexpr std::size_t kReplayStreamOffset = 0x200U;
inline constexpr std::size_t kReplayStreamBoundary = 0x4800U;

struct ReplayHighlightRange {
    std::int32_t start{};
    std::int32_t end{};
};

struct ReplayHeader {
    std::uint32_t num_frames{};
    std::array<ReplayHighlightRange, kReplayHighlightCount> highlights{};
    std::uint16_t num_skaters{};
    std::uint8_t skater{};
    std::uint8_t skater2{};
    std::array<std::byte, 2> reserved_30{};
    std::uint16_t level{};
    std::uint16_t game{};
};

struct ReplayFrame {
    // The replay decoder writes the first eight signed 20-bit channels after
    // shifting each value left by twelve into the live fixed-point object.
    std::array<std::int32_t, 8> fixed_channels{};
    std::array<std::int16_t, 8> narrow_channels{};
};

class ReplayBitReader final {
public:
    explicit ReplayBitReader(std::span<const std::byte> bytes, std::size_t bit_offset = 0)
        : bytes_(bytes), bit_offset_(bit_offset) {}

    [[nodiscard]] std::uint32_t read_bits(unsigned count);
    [[nodiscard]] std::int32_t read_signed(unsigned count);
    [[nodiscard]] bool read_flag() { return read_bits(1U) != 0; }
    [[nodiscard]] ReplayFrame read_frame();
    [[nodiscard]] std::size_t bit_offset() const noexcept { return bit_offset_; }
    [[nodiscard]] std::size_t remaining_bits() const noexcept {
        return bit_offset_ >= bytes_.size() * 8U ? 0U : bytes_.size() * 8U - bit_offset_;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t bit_offset_{};
};

// Native reader for the replay header used by DEMO*.REC and the serialized
// prefix copied into each retail card stream. The variable frame stream is
// intentionally exposed separately because optional post-frame fields are
// selected by the active replay mode.
class ReplayAsset final {
public:
    static ReplayAsset load(const std::string& path);
    static ReplayAsset parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] const ReplayHeader& header() const noexcept { return header_; }
    [[nodiscard]] std::span<const std::byte> serialized_header() const noexcept {
        return std::span<const std::byte>(bytes_).first(kReplayHeaderSize);
    }
    [[nodiscard]] std::span<const std::byte> payload() const noexcept {
        return std::span<const std::byte>(bytes_).subspan(kReplayHeaderSize);
    }
    [[nodiscard]] std::span<const std::byte> card_transfer(
        std::size_t player = 0) const;
    [[nodiscard]] ReplayBitReader frame_reader(
        std::size_t bit_offset = kReplayStreamOffset * 8U) const {
        return ReplayBitReader(std::span<const std::byte>(bytes_), bit_offset);
    }
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    ReplayHeader header_{};
};

} // namespace opentony::assets
