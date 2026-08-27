#pragma once

#include <cstdint>

namespace opentony::runtime {

struct GroundTurnConfig {
    // Retail DAT_0056865c is an 8-bit fractional frame scale in this path.
    // 0x100 is the normal one-frame value.
    std::int32_t frame_scale_q8{0x100};
    // player+0x29b7 selects the 0x3c, 0x78, or (for other nonzero profiles)
    // 0xb4 turn base.
    std::int32_t turn_profile{};
    // The ordinary state-0/7 branch uses +/-0x2d000. A high-speed
    // surface/board branch can select +/-0x5a000; other dispatcher states
    // have a separate +/-0xa0000 limit and must opt into it explicitly.
    std::int32_t limit{0x2d000};
    // FUN_00493370 uses +3144 >> 2 ordinarily and >> 1 in its alternate
    // surface branch. This selector is separate from the turn base.
    std::int32_t release_decay_shift{2};
    // Signed player +0x31a1 byte. The ordinary digital path leaves this at
    // zero; when its magnitude reaches 0x1a, retail moves the accumulator
    // toward the sign-corrected (limit * lean) >> 7 target.
    std::int8_t lean{};
};

struct GroundTurnResult {
    std::int32_t accumulator{};
    std::int32_t mirror{};
    std::int32_t delta{};
    // Retail +0x2e78: the wide-limit branch selected by the caller's
    // surface/profile policy.
    bool wide_profile{};
    // Retail +0x2e7c: this frame selected an active turn/target branch. It
    // remains true when an already-capped accumulator is pressed again.
    bool policy_changed{};
    // Wide branch applied the response-normalization write to +0x58.
    bool response_normalized{};
};

// The bounded part of retail FUN_00493370 that is established by the
// grounded action trace: Left/Right changes a signed turn accumulator and the
// result is mirrored to the adjacent steering field. The optional lean target
// is the confirmed non-digital branch of the same routine. Basis rotation and
// collision projection consume this result in later routines.
class GroundTurn final {
public:
    [[nodiscard]] static GroundTurnResult update(
        std::int32_t current,
        bool left,
        bool right,
        GroundTurnConfig config = {}) noexcept;

    // Exact scalar preparation performed by the ordinary grounded heading
    // path before FUN_0049b500's angle-table/matrix writer.
    [[nodiscard]] static std::int32_t angle12(
        std::int32_t turn_accumulator,
        std::int32_t frame_scale_q8) noexcept;
};

} // namespace opentony::runtime
