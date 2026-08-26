#include "fixed_step_driver.hpp"

#include <stdexcept>

namespace opentony::runtime {

FixedStepDriver::FixedStepDriver(
    GameplayFrame& gameplay,
    FixedStepConfig config)
    : gameplay_(gameplay), config_(config) {
    if (config_.simulation_step_ms == 0) {
        throw std::invalid_argument("fixed-step simulation interval must be non-zero");
    }
}

FixedStepAdvanceResult FixedStepDriver::advance(
    std::uint32_t elapsed_ms,
    const DirectInputKeyboardState& keyboard,
    const InputBindings& bindings,
    const PlayerPhysicsFrameHooks& physics_hooks,
    LevelFrameObserver* observer) {
    return advance(
        elapsed_ms,
        bindings.action_mask(keyboard),
        0,
        0,
        physics_hooks,
        observer);
}

FixedStepAdvanceResult FixedStepDriver::advance(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis,
    const PlayerPhysicsFrameHooks& physics_hooks,
    LevelFrameObserver* observer) {
    FixedStepAdvanceResult result{};
    const std::uint64_t accumulated =
        static_cast<std::uint64_t>(accumulated_ms_) + elapsed_ms;
    accumulated_ms_ = accumulated > 0xffffffffU
        ? 0xffffffffU
        : static_cast<std::uint32_t>(accumulated);

    const std::uint32_t maximum_steps = config_.max_catch_up_steps;
    while (accumulated_ms_ >= config_.simulation_step_ms
        && result.steps < maximum_steps) {
        result.last = gameplay_.step(
            config_.simulation_step_ms,
            action_mask,
            horizontal_axis,
            vertical_axis,
            physics_hooks,
            observer,
            config_.frame_scale_q8);
        accumulated_ms_ -= config_.simulation_step_ms;
        ++result.steps;
        result.consumed_ms += config_.simulation_step_ms;
        result.stepped = true;
    }

    if (result.steps == maximum_steps
        && accumulated_ms_ >= config_.simulation_step_ms) {
        const std::uint32_t retained =
            accumulated_ms_ % config_.simulation_step_ms;
        result.dropped_ms = accumulated_ms_ - retained;
        accumulated_ms_ = retained;
    }
    return result;
}

} // namespace opentony::runtime
