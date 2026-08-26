#pragma once

#include <array>
#include <cstdint>

namespace opentony::runtime {

using DirectInputKeyboardState = std::array<std::uint8_t, 256>;

// The scan codes are the DirectInput DIK values observed at the retail
// keyboard-state boundary. Action bits remain data-driven so configured
// trick/menu controls can be loaded without hard-coding an unverified layout.
inline constexpr std::uint8_t kDikLeft = 0xcb;
inline constexpr std::uint8_t kDikRight = 0xcd;
inline constexpr std::uint8_t kDikUp = 0xc8;
inline constexpr std::uint8_t kDikDown = 0xd0;

struct InputBinding {
    std::uint8_t scan_code{};
    std::uint16_t action_bit{};
};

// Converts the polled DirectInput keyboard buffer into the 16-bit action mask
// consumed by InputState. Multiple bindings may target one action and a
// configured binding may target any raw action bit.
class InputBindings final {
public:
    InputBindings() noexcept;

    void bind(InputBinding binding) noexcept;
    void clear(std::uint8_t scan_code) noexcept { bits_[scan_code] = 0; }

    [[nodiscard]] std::uint16_t action_mask(
        const DirectInputKeyboardState& keyboard) const noexcept;

    [[nodiscard]] static InputBindings movement_defaults() noexcept;

private:
    std::array<std::uint16_t, 256> bits_{};
};

} // namespace opentony::runtime
