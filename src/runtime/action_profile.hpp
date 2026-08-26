#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace opentony::runtime {

// The retail action poller at 0x00489a10 materializes this block at
// skater+0x2ccc. The slot names remain offsets: the raw bit mapping is
// proven, but several configured actions still need their downstream trace.
struct ActionProfileState final {
    static constexpr std::size_t kSlotCount = 16;

    std::array<bool, kSlotCount> slots{};
    std::int8_t horizontal_axis{}; // retail +0x149
    std::int8_t vertical_axis{};   // retail +0x148

    [[nodiscard]] bool slot_at_offset(std::uint16_t offset) const noexcept {
        if ((offset & 0x0fU) != 0 || offset >= 0x100U) {
            return false;
        }
        return slots[offset / 0x10U];
    }
};

// Exact raw-mask/analog-threshold portion of FUN_00489a10. This is the
// native representation consumed by the later 0x00492120 action-table
// reducer and by callers that supply the recovered 0x0049b010 gates.
[[nodiscard]] ActionProfileState map_action_profile(
    std::uint16_t action_mask,
    std::int8_t horizontal_axis = 0,
    std::int8_t vertical_axis = 0) noexcept;

} // namespace opentony::runtime
