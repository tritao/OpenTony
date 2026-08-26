#include "input_bindings.hpp"
#include "input_state.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::DirectInputKeyboardState;
    using opentony::runtime::InputBinding;
    using opentony::runtime::InputBindings;
    using opentony::runtime::MovementAction;
    using opentony::runtime::movement_bit;

    DirectInputKeyboardState keyboard{};
    const InputBindings defaults = InputBindings::movement_defaults();
    keyboard[opentony::runtime::kDikLeft] = 0x80;
    keyboard[opentony::runtime::kDikUp] = 0x80;
    assert(defaults.action_mask(keyboard)
        == static_cast<std::uint16_t>(
            movement_bit(MovementAction::Left)
            | movement_bit(MovementAction::Up)));

    keyboard[opentony::runtime::kDikLeft] = 0;
    assert(defaults.action_mask(keyboard)
        == movement_bit(MovementAction::Up));

    InputBindings configured;
    configured.bind(InputBinding{0x1f, 0x0010});
    configured.bind(InputBinding{0x20, 0x0010});
    keyboard = {};
    keyboard[0x1f] = 0x80;
    keyboard[0x20] = 0x01;
    assert(configured.action_mask(keyboard) == 0x0010);
    configured.clear(0x1f);
    assert(configured.action_mask(keyboard) == 0);

    std::cout << "input binding tests passed\n";
}
