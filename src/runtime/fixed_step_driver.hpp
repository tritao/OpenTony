#pragma once

#include "gameplay_frame.hpp"

#include <cstdint>

namespace opentony::runtime {

struct FixedStepConfig {
    std::uint32_t simulation_step_ms{16};
    std::uint32_t max_catch_up_steps{8};
    std::int32_t frame_scale_q8{0x100};
};

struct FixedStepAdvanceResult {
    std::uint32_t steps{};
    std::uint32_t consumed_ms{};
    std::uint32_t dropped_ms{};
    bool stepped{};
    GameplayFrameResult last{};
};

// Deterministic application-layer accumulator. The retail timing source and
// exact catch-up policy remain evidence seams; callers supply this policy
// explicitly instead of hiding a guessed timer constant in gameplay code.
class FixedStepDriver final {
public:
    FixedStepDriver(
        GameplayFrame& gameplay,
        FixedStepConfig config = {});

    void reset() noexcept { accumulated_ms_ = 0; }

    [[nodiscard]] FixedStepAdvanceResult advance(
        std::uint32_t elapsed_ms,
        const DirectInputKeyboardState& keyboard,
        const InputBindings& bindings,
        const PlayerPhysicsFrameHooks& physics_hooks = {},
        LevelFrameObserver* observer = nullptr);

    // Direct action/device path for gamepads and replay fixtures. The raw
    // axes are preserved through GameplayFrame so the retail analog-to-action
    // thresholds remain at the InputState boundary instead of being folded
    // into a frontend-specific binding.
    [[nodiscard]] FixedStepAdvanceResult advance(
        std::uint32_t elapsed_ms,
        std::uint16_t action_mask,
        std::int8_t horizontal_axis,
        std::int8_t vertical_axis,
        const PlayerPhysicsFrameHooks& physics_hooks = {},
        LevelFrameObserver* observer = nullptr);

    [[nodiscard]] std::uint32_t accumulated_ms() const noexcept {
        return accumulated_ms_;
    }
    [[nodiscard]] const FixedStepConfig& config() const noexcept {
        return config_;
    }

private:
    GameplayFrame& gameplay_;
    FixedStepConfig config_;
    std::uint32_t accumulated_ms_{};
};

} // namespace opentony::runtime
