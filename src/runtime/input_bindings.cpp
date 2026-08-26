#include "input_bindings.hpp"

#include "input_state.hpp"

namespace opentony::runtime {

InputBindings::InputBindings() noexcept = default;

void InputBindings::bind(InputBinding binding) noexcept {
    bits_[binding.scan_code] = static_cast<std::uint16_t>(
        bits_[binding.scan_code] | binding.action_bit);
}

std::uint16_t InputBindings::action_mask(
    const DirectInputKeyboardState& keyboard) const noexcept {
    std::uint16_t result = 0;
    for (std::size_t scan_code = 0; scan_code < keyboard.size(); ++scan_code) {
        if ((keyboard[scan_code] & 0x80U) != 0U) {
            result = static_cast<std::uint16_t>(
                result | bits_[scan_code]);
        }
    }
    return result;
}

InputBindings InputBindings::movement_defaults() noexcept {
    InputBindings result;
    result.bind({kDikLeft, movement_bit(MovementAction::Left)});
    result.bind({kDikRight, movement_bit(MovementAction::Right)});
    result.bind({kDikUp, movement_bit(MovementAction::Up)});
    result.bind({kDikDown, movement_bit(MovementAction::Down)});
    return result;
}

} // namespace opentony::runtime
