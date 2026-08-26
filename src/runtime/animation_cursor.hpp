#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace opentony::runtime {

// The retail object stores the five numeric modes directly. Keep the values
// stable at this boundary; public names for the less common modes are still
// provisional in the reverse-engineering evidence.
enum class AnimationPlaybackMode : std::uint8_t {
    Stop = 0,
    Loop = 1,
    Hold = 2,
    ClockPingPong = 3,
    Reverse = 4,
};

// The steering selector's exact integer easing primitive. It is shared by
// the gameplay-facing ground selector and the cursor differential tests.
[[nodiscard]] std::int16_t approach_animation_frame(
    std::int16_t current,
    std::int16_t target) noexcept;

struct AnimationTableView final {
    std::span<const std::uint8_t> frame_counts{};

    [[nodiscard]] bool contains(std::uint16_t animation) const noexcept {
        return static_cast<std::size_t>(animation) < frame_counts.size()
            && frame_counts[animation] != 0;
    }

    [[nodiscard]] std::uint8_t frame_count(
        std::uint16_t animation) const noexcept {
        return static_cast<std::size_t>(animation) < frame_counts.size()
            ? frame_counts[animation]
            : 0;
    }
};

struct AnimationRequestResult final {
    bool applied = false;
    bool invalid_id = false;
    std::uint16_t requested_id = 0;
    std::uint16_t effective_id = 0;
    std::uint8_t frame_count = 0;
};

// Compatibility representation of the animation fields recovered from the
// retail CSuper object. The cursor is intentionally independent of the pose
// cache and of gameplay-specific selector policy.
struct AnimationCursor final {
    std::uint16_t id = 0;                 // +0xf6
    std::int16_t frame = 0;               // +0xf4
    std::uint16_t fraction = 0;           // +0x104
    std::uint32_t rate = 0x10000;          // +0x108
    std::uint8_t mode = 0;                // +0xf8
    std::int8_t direction = 0;             // +0x100
    std::int8_t endpoint = 0;              // +0x101
    std::int8_t alternate_endpoint = 0;   // +0x102
    std::int16_t target_frame = 0;         // +0xfa
    std::int16_t target_frame2 = 0;        // +0xfc
    std::int16_t mode3_clock = 0;          // +0xfe
    std::uint8_t frame_count = 1;          // +0x106
    bool finished = true;                  // +0x107
    std::int16_t request_start = 0;        // +0x114

    // Transition history consumed by FrameReached. These are explicit rather
    // than inferred from the current cursor because a request may replace the
    // current animation between the two stores.
    std::int16_t old_frame = 0;            // +0x10c
    std::int16_t new_frame = 0;            // +0x10e
    std::uint16_t old_anim = 0;            // +0x110
    std::int8_t old_anim_dir = 0;          // +0x112

    [[nodiscard]] AnimationRequestResult request(
        AnimationTableView table,
        std::uint16_t animation,
        std::int32_t start = -1,
        std::int32_t end = -1,
        std::int32_t alternate = -1) noexcept;

    [[nodiscard]] AnimationRequestResult cycle(
        AnimationTableView table,
        std::uint16_t animation,
        std::int32_t playback_direction) noexcept;

    // `global_scale_q8` is DAT_0056865c. The optional clock is
    // DAT_005685f4, used only by mode 3. The return value preserves the
    // retail packed-word return where it is observable, while callers should
    // read the fields above for the authoritative state.
    [[nodiscard]] std::int32_t advance(
        std::int32_t global_scale_q8,
        std::int32_t animation_clock = 0) noexcept;

    [[nodiscard]] bool frame_reached(
        std::uint16_t animation,
        std::int16_t queried_frame) const noexcept;
};

} // namespace opentony::runtime
