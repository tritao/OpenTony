#pragma once

#include "input_state.hpp"
#include "../trg/level_runtime.hpp"

#include <cstdint>

namespace opentony::runtime {

class LevelFrameObserver {
public:
    virtual ~LevelFrameObserver() = default;

    // Called after device polling/action-mask construction and before the
    // level-side tick. This is where a future player/input consumer belongs.
    virtual void on_input_frame(
        std::uint64_t,
        const InputState&) {}

    // Called after trigger timers/state and scene synchronization complete.
    // Renderer, camera, collision response, and player code can subscribe
    // without entering the TRG decoder.
    virtual void on_level_tick(
        std::uint64_t,
        std::uint32_t,
        const InputState&, 
        const opentony::trg::LevelRuntime&) {}
};

// Recovered high-level ordering for one level iteration:
// poll/action mask -> input history -> level/script tick -> downstream
// object/player/camera/render observers.
class LevelFrameScheduler final {
public:
    explicit LevelFrameScheduler(opentony::trg::LevelRuntime& level) noexcept
        : level_(level) {}

    void step(
        std::uint32_t elapsed_ms,
        std::uint16_t action_mask,
        LevelFrameObserver* observer = nullptr);

    void step(
        std::uint32_t elapsed_ms,
        std::uint16_t action_mask,
        std::int8_t horizontal_axis,
        std::int8_t vertical_axis,
        LevelFrameObserver* observer = nullptr);

    [[nodiscard]] std::uint64_t frame_index() const noexcept { return frame_index_; }
    [[nodiscard]] const InputState& input() const noexcept { return input_; }
    [[nodiscard]] const opentony::trg::LevelRuntime& level() const noexcept {
        return level_;
    }

private:
    opentony::trg::LevelRuntime& level_;
    InputState input_;
    std::uint64_t frame_index_{};
};

} // namespace opentony::runtime
