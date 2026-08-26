#include "queued_motion.hpp"

#include <cstdlib>

namespace opentony::runtime {

QueuedMotionDrainResult drain_queued_motion(
    QueuedMotionState& state,
    std::int32_t frame_scale_q8) noexcept {
    QueuedMotionDrainResult result{};
    for (std::size_t axis = 0; axis < state.pending.size(); ++axis) {
        const std::int32_t pending = state.pending[axis];
        if (pending == 0) {
            continue;
        }

        const std::int32_t rate = state.rate[axis];
        const std::int32_t magnitude = static_cast<std::int32_t>(
            (static_cast<std::int64_t>(std::abs(rate)) * frame_scale_q8) >> 8);
        if (pending < magnitude) {
            // The retail sign expression is +1 for non-negative rates and
            // -1 for negative rates. The action command normally supplies a
            // positive amount and chooses direction through the rate word.
            const std::int32_t sign = rate < 0 ? -1 : 1;
            result.local_delta[axis] = pending * sign;
            state.pending[axis] = 0;
        } else {
            state.pending[axis] = pending - magnitude;
            result.local_delta[axis] = magnitude;
            state.accumulated[axis] += magnitude;
        }
        result.moved = result.moved || result.local_delta[axis] != 0;
    }
    return result;
}

bool set_queued_motion_command(
    QueuedMotionState& state,
    std::int32_t axis,
    std::int16_t amount,
    std::int16_t rate) noexcept {
    if (axis < 0 || axis >= static_cast<std::int32_t>(state.pending.size())) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(axis);
    state.pending[index] = amount;
    state.rate[index] = rate;
    if (rate == 0) {
        state.pending[index] = 0;
        state.accumulated[index] = 0;
    }
    return true;
}

} // namespace opentony::runtime
