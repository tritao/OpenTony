#include "action_profile.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::map_action_profile;

    const auto raw = map_action_profile(0x0fffU);
    assert(raw.slot_at_offset(0x00));
    assert(raw.slot_at_offset(0x10));
    assert(raw.slot_at_offset(0x20));
    assert(raw.slot_at_offset(0x30));
    assert(raw.slot_at_offset(0x40));
    assert(raw.slot_at_offset(0x50));
    assert(raw.slot_at_offset(0x60));
    assert(raw.slot_at_offset(0x70));
    assert(!raw.slot_at_offset(0x80));
    assert(!raw.slot_at_offset(0x90));
    assert(!raw.slot_at_offset(0xa0));
    assert(!raw.slot_at_offset(0xb0));
    assert(raw.slot_at_offset(0xc0));
    assert(raw.slot_at_offset(0xd0));
    assert(raw.slot_at_offset(0xe0));
    assert(raw.slot_at_offset(0xf0));

    const auto analog = map_action_profile(0, -0x29, 0x29);
    assert(analog.slot_at_offset(0x80));
    assert(!analog.slot_at_offset(0x90));
    assert(!analog.slot_at_offset(0xa0));
    assert(analog.slot_at_offset(0xb0));
    assert(analog.horizontal_axis == -0x29);
    assert(analog.vertical_axis == 0x29);

    const auto just_inside = map_action_profile(0, -0x28, 0x28);
    assert(!just_inside.slot_at_offset(0x80));
    assert(!just_inside.slot_at_offset(0xb0));
    assert(!just_inside.slot_at_offset(0x90));
    assert(!just_inside.slot_at_offset(0xa0));
    assert(!just_inside.slot_at_offset(0x100));
    assert(!just_inside.slot_at_offset(0x81));

    std::cout << "Action profile tests passed\n";
}
