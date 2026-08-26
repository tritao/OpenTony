#pragma once

#include "psx_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opentony::assets {

struct PsxAnimationRecord final {
    std::uint32_t relative_data_offset{};
    std::uint32_t frame_count_and_flags{};

    [[nodiscard]] std::uint16_t frame_count() const noexcept {
        return static_cast<std::uint16_t>(frame_count_and_flags);
    }

    [[nodiscard]] std::uint16_t flags() const noexcept {
        return static_cast<std::uint16_t>(frame_count_and_flags >> 16U);
    }
};

// Renderer-independent view of the type-0x2c animation table in SK2ANIM.PSX.
// The compressed stream remains opaque here; this boundary is sufficient for
// binding a retail animation ID to its frame count and source byte range.
class PsxAnimationTable final {
public:
    static PsxAnimationTable load(const std::string& path);
    static PsxAnimationTable parse(const PsxArchive& archive);

    [[nodiscard]] std::size_t animation_count() const noexcept {
        return records_.size();
    }
    [[nodiscard]] std::size_t part_count() const noexcept {
        return part_count_;
    }
    [[nodiscard]] const PsxAnimationRecord& record(
        std::uint16_t animation) const;
    [[nodiscard]] std::span<const std::uint8_t> frame_counts() const noexcept {
        return frame_counts_;
    }
    [[nodiscard]] std::span<const std::uint16_t> hierarchy_words() const noexcept {
        return hierarchy_words_;
    }
    [[nodiscard]] std::span<const std::byte> stream(
        std::uint16_t animation) const;
    [[nodiscard]] std::size_t payload_size() const noexcept {
        return payload_.size();
    }

private:
    std::vector<std::byte> payload_;
    std::vector<PsxAnimationRecord> records_;
    std::vector<std::uint8_t> frame_counts_;
    std::vector<std::uint16_t> hierarchy_words_;
    std::size_t part_count_{};
};

} // namespace opentony::assets
