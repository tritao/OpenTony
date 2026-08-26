#include "action_profile.hpp"

namespace opentony::runtime {
namespace {

constexpr std::uint16_t kSlotBits[8] = {
    0x0010, // +0x00
    0x0080, // +0x10
    0x0020, // +0x20
    0x0040, // +0x30
    0x0004, // +0x40
    0x0001, // +0x50
    0x0008, // +0x60
    0x0002, // +0x70
};

} // namespace

ActionProfileState map_action_profile(
    std::uint16_t action_mask,
    std::int8_t horizontal_axis,
    std::int8_t vertical_axis) noexcept {
    ActionProfileState result{};
    result.horizontal_axis = horizontal_axis;
    result.vertical_axis = vertical_axis;

    for (std::size_t index = 0; index < 8; ++index) {
        result.slots[index] = (action_mask & kSlotBits[index]) != 0;
    }

    // FUN_00489a10 falls back to normalized controller axes for the four
    // directional slots. This OR form is equivalent for the materialized
    // boolean values.
    result.slots[8] = (action_mask & 0x8000U) != 0
        || horizontal_axis <= -0x29;
    result.slots[9] = (action_mask & 0x2000U) != 0
        || horizontal_axis >= 0x29;
    result.slots[10] = (action_mask & 0x1000U) != 0
        || vertical_axis <= -0x29;
    result.slots[11] = (action_mask & 0x4000U) != 0
        || vertical_axis >= 0x29;

    result.slots[12] = (action_mask & 0x0200U) != 0;
    result.slots[13] = (action_mask & 0x0400U) != 0;
    result.slots[14] = (action_mask & 0x0800U) != 0;
    result.slots[15] = (action_mask & 0x0100U) != 0;
    return result;
}

} // namespace opentony::runtime
