#include "gameplay_frame.hpp"

#include "../trg/level_runtime.hpp"

#include "tests/test_check.hpp"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

[[nodiscard]] std::string asset_path(const char* relative) {
    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    return (root / relative).string();
}

class Observer final : public opentony::runtime::LevelFrameObserver {
public:
    void on_input_frame(
        std::uint64_t,
        const opentony::runtime::InputState& input) override {
        order.push_back(1);
        CHECK(input.held(opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left)));
    }

    void on_level_tick(
        std::uint64_t,
        std::uint32_t,
        const opentony::runtime::InputState& input,
        const opentony::trg::LevelRuntime& level) override {
        order.push_back(2);
        CHECK(input.held(opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left)));
        CHECK(level.state().time_ms() == 16);
    }

    std::vector<int> order;
};

} // namespace

int main() {
    using opentony::runtime::DirectInputKeyboardState;
    using opentony::runtime::InputBindings;
    using opentony::runtime::MovementAction;
    using opentony::runtime::kDikLeft;
    using opentony::runtime::movement_bit;

    const std::string trg = asset_path("SKWARE_T.TRG");
    const std::string psx = asset_path("SKWARE.PSX");
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::trg::LevelRuntime level(trg, psx, asset_path(""));
    level.initialize();

    opentony::runtime::PlayerState player({0, 0, 0});
    opentony::runtime::PlayerPhysicsFrameHooks physics_hooks{};
    physics_hooks.apply_ground_turn = false;
    physics_hooks.integrate_position = false;
    physics_hooks.on_stage = [](
        opentony::runtime::PhysicsDispatchStage,
        opentony::runtime::PlayerState&,
        const opentony::runtime::InputState& input) {
        CHECK(input.held(opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left)));
    };

    opentony::runtime::GameplayFrame frame(level, player);
    Observer observer;
    const auto result = frame.step(
        16,
        opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left),
        physics_hooks,
        &observer,
        0x80);

    CHECK(observer.order == std::vector<int>({1, 2}));
    CHECK(result.frame_index == 1);
    CHECK(result.elapsed_ms == 16);
    CHECK(result.frame_scale_q8 == 0x80);
    CHECK(result.input.held(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left)));
    CHECK(result.trigger_event_count_after
        >= result.trigger_event_count_before);
    CHECK(result.trigger_events.size()
        == result.trigger_event_count_after
            - result.trigger_event_count_before);
    CHECK(result.physics.dispatch.state == 0);
    CHECK(result.physics.dispatch.handled);
    CHECK(player.frame_counter() == 1);

    DirectInputKeyboardState keyboard{};
    keyboard[kDikLeft] = 0x80;
    const auto keyboard_result = frame.step(
        16,
        keyboard,
        InputBindings::movement_defaults(),
        physics_hooks,
        nullptr,
        0x100);
    CHECK(keyboard_result.frame_index == 2);
    CHECK(keyboard_result.input.action_mask()
        == movement_bit(MovementAction::Left));

    std::cout << "gameplay frame ok\n";
}
