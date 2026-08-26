#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace opentony::runtime {

// The four movement bits are confirmed by the retail DirectInput/action-mask
// traces. Other action bits are intentionally left as raw mask values until
// their bindings are recovered.
enum class MovementAction : std::uint8_t {
    Left,
    Right,
    Up,
    Down,
};

[[nodiscard]] constexpr std::uint16_t movement_bit(MovementAction action) noexcept {
    switch (action) {
    case MovementAction::Left:
        return 0x8000;
    case MovementAction::Right:
        return 0x2000;
    case MovementAction::Up:
        return 0x1000;
    case MovementAction::Down:
        return 0x4000;
    }
    return 0;
}

struct ActionTransition {
    bool held{};
    bool pressed{};
    bool released{};
    bool press_latched{};
    // Retail action-record byte +1: once this action has transitioned from
    // inactive to active, the byte remains set until a higher-level consumer
    // clears or replaces the record.
    bool activation_seen{};
    std::uint32_t held_frames{};
    std::uint32_t inactive_frames{};
    std::uint32_t frames_since_press{};
    std::uint64_t updates{};
};

struct InputFrameSnapshot {
    std::uint16_t action_mask{};
    std::uint16_t effective_movement_mask{};
    std::int8_t horizontal_axis{};
    std::int8_t vertical_axis{};
};

// Renderer/input-device independent action history. Its edge/counter model
// mirrors the observed 16-byte retail movement records while retaining the
// source action mask for later bindings.
class InputState final {
public:
    static constexpr std::size_t kHistoryDepth = 4;

    // horizontal_axis and vertical_axis are the signed controller values used
    // by retail when the corresponding digital bit is absent. The observed
    // action poller treats <= -0x29 as left/up and >= 0x29 as right/down.
    void begin_frame(
        std::uint16_t action_mask,
        std::int8_t horizontal_axis = 0,
        std::int8_t vertical_axis = 0) noexcept;

    [[nodiscard]] std::uint16_t action_mask() const noexcept { return action_mask_; }
    [[nodiscard]] std::int8_t horizontal_axis() const noexcept {
        return horizontal_axis_;
    }
    [[nodiscard]] std::int8_t vertical_axis() const noexcept {
        return vertical_axis_;
    }
    [[nodiscard]] std::uint16_t effective_movement_mask() const noexcept {
        return effective_movement_mask_;
    }
    [[nodiscard]] std::uint16_t previous_action_mask() const noexcept {
        return previous_action_mask_;
    }
    // Semantic equivalent of the four-frame records copied by retail
    // FUN_00489cd0. Entry zero is the current completed frame; larger values
    // address older completed frames. The native representation deliberately
    // stores masks, not the retail pointer destinations.
    [[nodiscard]] const InputFrameSnapshot& history(
        std::size_t frames_ago) const noexcept;
    [[nodiscard]] bool held(std::uint16_t bit) const noexcept {
        return bit != 0 && (effective_mask(bit) & bit) != 0;
    }
    [[nodiscard]] bool pressed(std::uint16_t bit) const noexcept {
        const std::uint16_t previous = (bit & 0xf000U) != 0
            ? previous_effective_movement_mask_
            : previous_action_mask_;
        return bit != 0 && (effective_mask(bit) & bit) != 0
            && (previous & bit) == 0;
    }
    [[nodiscard]] bool released(std::uint16_t bit) const noexcept {
        const std::uint16_t previous = (bit & 0xf000U) != 0
            ? previous_effective_movement_mask_
            : previous_action_mask_;
        return bit != 0 && (effective_mask(bit) & bit) == 0
            && (previous & bit) != 0;
    }
    [[nodiscard]] const ActionTransition& movement(MovementAction action) const noexcept;
    // Retail's non-directional action records use the same active/edge/
    // counter layout as the movement records. `bit` must contain one of the
    // 16-bit action-mask bits; unknown bits return an empty record.
    [[nodiscard]] const ActionTransition& action(std::uint16_t bit) const noexcept;
    void clear_press_latch(MovementAction action) noexcept;
    void clear_action_latch(std::uint16_t bit) noexcept;

private:
    std::uint16_t action_mask_{};
    std::uint16_t previous_action_mask_{};
    std::uint16_t effective_movement_mask_{};
    std::uint16_t previous_effective_movement_mask_{};
    std::int8_t horizontal_axis_{};
    std::int8_t vertical_axis_{};
    ActionTransition movement_[4]{};
    ActionTransition actions_[16]{};
    std::array<InputFrameSnapshot, kHistoryDepth> history_{};

    [[nodiscard]] std::uint16_t effective_mask(std::uint16_t bit) const noexcept;
};

} // namespace opentony::runtime
