#pragma once

#include "psx_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opentony::assets {

inline constexpr std::uint32_t kPsxAnimationTag = 0x0000002aU;
inline constexpr std::uint32_t kPsxCompressedAnimationTag = 0x0000002cU;
inline constexpr std::uint32_t kPsxHierarchyTag = 0x52454948U;

struct PsxAnimationProduct {
    std::uint32_t tag_type{};
    std::size_t tag_offset{};
    std::vector<std::array<std::byte, 8>> records;
    std::vector<std::byte> source_stream;
};

struct PsxAnimationChannelDecode {
    std::uint8_t interpolation_count{};
    std::uint8_t encoding{};
    std::vector<std::int16_t> samples;
    std::size_t consumed_bytes{};
};

// Decode one channel using the exact 0x004305f0 stream contract. The count is
// the number of source values/key targets supplied to the decoder; nibble
// interpolation can therefore produce more output samples than that count.
[[nodiscard]] PsxAnimationChannelDecode decode_psx_animation_channel(
    std::span<const std::byte> stream,
    std::size_t source_value_count);

// Bounded native representation of the common PSX animation post-model
// products. Both retail animation tag variants share the count/8-byte-record
// and image-relative source-stream boundary; record fields and compressed
// pose semantics remain raw until their consumers are fully recovered.
class PsxAnimationRuntime final {
public:
    void build(const PsxArchive& archive);

    [[nodiscard]] const std::vector<PsxAnimationProduct>& products() const noexcept {
        return products_;
    }
    [[nodiscard]] std::span<const std::byte> hierarchy_payload() const noexcept {
        return hierarchy_payload_;
    }
    [[nodiscard]] const PsxArchive& source_archive() const noexcept { return *archive_; }

private:
    const PsxArchive* archive_{};
    std::vector<PsxAnimationProduct> products_;
    std::vector<std::byte> hierarchy_payload_;
};

inline constexpr std::size_t kPsxAnimationPlaybackStateSize = 0x118U;

// Offset-preserving native counterpart of the animation fields consumed by
// ob.cpp's 0x00480730/0x00480950 pair. The surrounding object contains more
// state; this value-owned record covers the recovered playback contract and
// accepts the retail fixed-point clock as an explicit input.
class PsxAnimationPlaybackState final {
public:
    void start(
        std::uint16_t animation_index,
        std::uint8_t frame_count,
        std::int32_t start_frame = -1,
        std::int32_t end_frame = -1,
        std::uint8_t alternate_frame = 0) noexcept;
    void start_special(
        std::uint16_t animation_index,
        std::uint8_t frame_count,
        std::int8_t direction) noexcept;

    // time_scale_q8 is the global 0x56865c scale (0x100 is one normal tick);
    // animation_clock is the global clock used by mode 3. Keeping both
    // caller-supplied makes replay and retail comparison deterministic.
    void advance(
        std::int32_t time_scale_q8,
        std::int32_t animation_clock = 0);

    [[nodiscard]] std::uint16_t animation_index() const noexcept;
    [[nodiscard]] std::uint8_t frame_count() const noexcept;
    [[nodiscard]] std::int16_t current_frame() const noexcept;
    [[nodiscard]] std::uint8_t mode() const noexcept;
    [[nodiscard]] std::int8_t direction() const noexcept;
    [[nodiscard]] std::uint8_t end_frame() const noexcept;
    [[nodiscard]] std::uint8_t alternate_frame() const noexcept;
    [[nodiscard]] std::int16_t original_start_frame() const noexcept;
    [[nodiscard]] std::int32_t playback_rate_fixed() const noexcept;
    [[nodiscard]] bool finished() const noexcept;

    void set_mode(std::uint8_t value) noexcept;
    void set_playback_rate_fixed(std::int32_t value) noexcept;
    void set_pingpong_range(
        std::int16_t start,
        std::int16_t end,
        std::int16_t time_origin) noexcept;

    [[nodiscard]] std::span<const std::byte> raw() const noexcept { return raw_; }

private:
    std::array<std::byte, kPsxAnimationPlaybackStateSize> raw_{};

    [[nodiscard]] std::uint8_t u8(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept;
    [[nodiscard]] std::int16_t s16(std::size_t offset) const noexcept;
    [[nodiscard]] std::int32_t s32(std::size_t offset) const noexcept;
    void put8(std::size_t offset, std::uint8_t value) noexcept;
    void put16(std::size_t offset, std::uint16_t value) noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
    void set_accumulator(std::int32_t value) noexcept;
};

} // namespace opentony::assets
