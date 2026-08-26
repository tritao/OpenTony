#pragma once

#include <array>
#include <cstdint>

namespace opentony::runtime {

// Retail player offsets:
//   +0x2ca0/+0x2ca4/+0x2ca8  pending local amounts
//   +0x2c94/+0x2c98/+0x2c9c  signed per-frame rates
//   +0x2cac/+0x2cb0/+0x2cb4  accumulated local movement
// The arrays retain the three independent scalar words without pretending
// that the surrounding retail object is a C++-compatible packed struct.
struct QueuedMotionState final {
    std::array<std::int32_t, 3> pending{};
    std::array<std::int32_t, 3> rate{};
    std::array<std::int32_t, 3> accumulated{};

    friend bool operator==(
        const QueuedMotionState&,
        const QueuedMotionState&) = default;
};

struct QueuedMotionDrainResult final {
    std::array<std::int32_t, 3> local_delta{};
    bool moved{};

    friend bool operator==(
        const QueuedMotionDrainResult&,
        const QueuedMotionDrainResult&) = default;
};

// Implements the first branch of retail FUN_00493370. The result is still in
// the player's local basis; FUN_004e85a0's orientation transform remains a
// separate boundary until its exact coordinate convention is fully checked.
[[nodiscard]] QueuedMotionDrainResult drain_queued_motion(
    QueuedMotionState& state,
    std::int32_t frame_scale_q8 = 0x100) noexcept;

// Implements action-stream opcode 0x2b's field writes from FUN_004be450.
// Invalid axes are rejected rather than reproducing the retail out-of-bounds
// write; valid values retain the sign-extended 16-bit command fields.
[[nodiscard]] bool set_queued_motion_command(
    QueuedMotionState& state,
    std::int32_t axis,
    std::int16_t amount,
    std::int16_t rate) noexcept;

} // namespace opentony::runtime
