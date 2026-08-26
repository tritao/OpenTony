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
    // FUN_00492120's DAT_005369c8 lookup result for the directional portion
    // of this profile. Lean bits from skater +0x31a1/+0x31a2 are intentionally
    // not folded in here; callers with those raw fields can use the explicit
    // selector below.
    std::uint8_t selected_action{};

    [[nodiscard]] bool slot_at_offset(std::uint16_t offset) const noexcept {
        if ((offset & 0x0fU) != 0 || offset >= 0x100U) {
            return false;
        }
        return slots[offset / 0x10U];
    }
};

// Exact DAT_005369c8 reducer used by FUN_00492120. The optional lean values
// are signed skater bytes at +0x31a2 (vertical) and +0x31a1 (horizontal).
[[nodiscard]] std::uint8_t select_action_table_entry(
    const ActionProfileState& profile,
    std::int8_t vertical_lean = 0,
    std::int8_t horizontal_lean = 0) noexcept;

// Exact raw-mask/analog-threshold portion of FUN_00489a10. This is the
// native representation consumed by the later 0x00492120 action-table
// reducer and by callers that supply the recovered 0x0049b010 gates.
[[nodiscard]] ActionProfileState map_action_profile(
    std::uint16_t action_mask,
    std::int8_t horizontal_axis = 0,
    std::int8_t vertical_axis = 0) noexcept;

} // namespace opentony::runtime
