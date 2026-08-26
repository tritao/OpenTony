#include "gameplay_frame.hpp"

namespace opentony::runtime {
namespace {

class GameplayObserver final : public LevelFrameObserver {
public:
    GameplayObserver(
        LevelFrameObserver* downstream,
        PlayerState& player,
        const PlayerPhysicsFrameHooks& physics_hooks,
        PlayerPhysicsFrameResult& physics_result,
        std::int32_t frame_scale_q8) noexcept
        : downstream_(downstream),
          player_(player),
          physics_hooks_(physics_hooks),
          physics_result_(physics_result),
          frame_scale_q8_(frame_scale_q8) {}

    void on_input_frame(
        std::uint64_t frame,
        const InputState& input) override {
        if (downstream_ != nullptr) {
            downstream_->on_input_frame(frame, input);
        }
    }

    void on_level_tick(
        std::uint64_t frame,
        std::uint32_t milliseconds,
        const InputState& input,
        const opentony::trg::LevelRuntime& level) override {
        if (downstream_ != nullptr) {
            downstream_->on_level_tick(frame, milliseconds, input, level);
        }
        physics_result_ = PlayerPhysicsFrame::step(
            player_,
            input,
            physics_hooks_,
            frame_scale_q8_);
    }

private:
    LevelFrameObserver* downstream_{};
    PlayerState& player_;
    const PlayerPhysicsFrameHooks& physics_hooks_;
    PlayerPhysicsFrameResult& physics_result_;
    std::int32_t frame_scale_q8_;
};

} // namespace

GameplayFrameResult GameplayFrame::step(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    const PlayerPhysicsFrameHooks& physics_hooks,
    LevelFrameObserver* observer,
    std::int32_t frame_scale_q8) {
    return step(
        elapsed_ms,
        action_mask,
        0,
        0,
        physics_hooks,
        observer,
        frame_scale_q8);
}

GameplayFrameResult GameplayFrame::step(
    std::uint32_t elapsed_ms,
    const DirectInputKeyboardState& keyboard,
    const InputBindings& bindings,
    const PlayerPhysicsFrameHooks& physics_hooks,
    LevelFrameObserver* observer,
    std::int32_t frame_scale_q8) {
    return step(
        elapsed_ms,
        bindings.action_mask(keyboard),
        physics_hooks,
        observer,
        frame_scale_q8);
}

GameplayFrameResult GameplayFrame::step(
    std::uint32_t elapsed_ms,
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis,
    const PlayerPhysicsFrameHooks& physics_hooks,
    LevelFrameObserver* observer,
    std::int32_t frame_scale_q8) {
    GameplayFrameResult result{};
    result.elapsed_ms = elapsed_ms;
    result.frame_scale_q8 = frame_scale_q8;
    result.trigger_event_count_before =
        scheduler_.level().state().events().size();
    GameplayObserver gameplay_observer(
        observer,
        player_,
        physics_hooks,
        result.physics,
        frame_scale_q8);
    scheduler_.step(
        elapsed_ms,
        action_mask,
        horizontal_axis,
        vertical_axis,
        &gameplay_observer);
    result.frame_index = scheduler_.frame_index();
    result.input = scheduler_.input();
    const auto& events = scheduler_.level().state().events();
    result.trigger_event_count_after = events.size();
    if (events.size() > result.trigger_event_count_before) {
        result.trigger_events.assign(
            events.begin() + static_cast<std::ptrdiff_t>(
                result.trigger_event_count_before),
            events.end());
    }
    return result;
}

} // namespace opentony::runtime
