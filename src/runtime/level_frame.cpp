#include "level_frame.hpp"

namespace opentony::runtime {

void LevelFrameScheduler::step(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    LevelFrameObserver* observer) {
    step(elapsed_ms, action_mask, 0, 0, observer);
}

void LevelFrameScheduler::step(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis,
    LevelFrameObserver* observer) {
    input_.begin_frame(action_mask, horizontal_axis, vertical_axis);
    ++frame_index_;
    if (observer != nullptr) {
        observer->on_input_frame(frame_index_, input_);
    }
    level_.tick(elapsed_ms);
    if (observer != nullptr) {
        observer->on_level_tick(frame_index_, elapsed_ms, input_, level_);
    }
}

} // namespace opentony::runtime
