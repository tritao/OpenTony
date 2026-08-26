#include "fixed_step_driver.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

[[nodiscard]] std::string asset_path(const char* relative) {
    const std::filesystem::path root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    return (root / relative).string();
}

} // namespace

int main() {
    const std::string trg = asset_path("SKWARE_T.TRG");
    const std::string psx = asset_path("SKWARE.PSX");
    if (!std::filesystem::is_regular_file(trg)
        || !std::filesystem::is_regular_file(psx)) {
        std::cout << "fixture unavailable\n";
        return 0;
    }

    opentony::trg::LevelRuntime level(trg, psx, asset_path(""));
    level.initialize();
    opentony::runtime::PlayerState player;
    opentony::runtime::GameplayFrame gameplay(level, player);
    opentony::runtime::FixedStepDriver driver(
        gameplay,
        opentony::runtime::FixedStepConfig{16, 2, 0x80});
    const auto bindings = opentony::runtime::InputBindings::movement_defaults();
    opentony::runtime::DirectInputKeyboardState keyboard{};
    keyboard[opentony::runtime::kDikLeft] = 0x80;

    const auto partial = driver.advance(15, keyboard, bindings);
    assert(!partial.stepped);
    assert(partial.steps == 0);
    assert(driver.accumulated_ms() == 15);

    const auto first = driver.advance(1, keyboard, bindings);
    assert(first.stepped);
    assert(first.steps == 1);
    assert(first.consumed_ms == 16);
    assert(first.dropped_ms == 0);
    assert(first.last.frame_index == 1);
    assert(first.last.frame_scale_q8 == 0x80);
    assert(first.last.input.action_mask()
        == opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Left));

    const auto analog = driver.advance(
        16,
        0,
        static_cast<std::int8_t>(0x29),
        0);
    assert(analog.stepped);
    assert(analog.steps == 1);
    assert(analog.last.input.action_mask() == 0);
    assert(analog.last.input.horizontal_axis() == 0x29);
    assert(analog.last.input.effective_movement_mask()
        == opentony::runtime::movement_bit(
            opentony::runtime::MovementAction::Right));
    assert(analog.last.input.movement(
        opentony::runtime::MovementAction::Right).pressed);

    const auto capped = driver.advance(80, keyboard, bindings);
    assert(capped.steps == 2);
    assert(capped.consumed_ms == 32);
    assert(capped.dropped_ms == 48);
    assert(capped.last.frame_index == 4);
    assert(driver.accumulated_ms() == 0);

    std::cout << "Fixed-step driver tests passed\n";
}
