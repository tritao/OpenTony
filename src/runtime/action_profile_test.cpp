#include "action_profile.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    using opentony::runtime::map_action_profile;

    const auto raw = map_action_profile(0x0fffU);
    CHECK(raw.slot_at_offset(0x00));
    CHECK(raw.slot_at_offset(0x10));
    CHECK(raw.slot_at_offset(0x20));
    CHECK(raw.slot_at_offset(0x30));
    CHECK(raw.slot_at_offset(0x40));
    CHECK(raw.slot_at_offset(0x50));
    CHECK(raw.slot_at_offset(0x60));
    CHECK(raw.slot_at_offset(0x70));
    CHECK(!raw.slot_at_offset(0x80));
    CHECK(!raw.slot_at_offset(0x90));
    CHECK(!raw.slot_at_offset(0xa0));
    CHECK(!raw.slot_at_offset(0xb0));
    CHECK(raw.slot_at_offset(0xc0));
    CHECK(raw.slot_at_offset(0xd0));
    CHECK(raw.slot_at_offset(0xe0));
    CHECK(raw.slot_at_offset(0xf0));

    const auto analog = map_action_profile(0, -0x29, 0x29);
    CHECK(analog.slot_at_offset(0x80));
    CHECK(!analog.slot_at_offset(0x90));
    CHECK(!analog.slot_at_offset(0xa0));
    CHECK(analog.slot_at_offset(0xb0));
    CHECK(analog.horizontal_axis == -0x29);
    CHECK(analog.vertical_axis == 0x29);

    const auto just_inside = map_action_profile(0, -0x28, 0x28);
    CHECK(!just_inside.slot_at_offset(0x80));
    CHECK(!just_inside.slot_at_offset(0xb0));
    CHECK(!just_inside.slot_at_offset(0x90));
    CHECK(!just_inside.slot_at_offset(0xa0));
    CHECK(!just_inside.slot_at_offset(0x100));
    CHECK(!just_inside.slot_at_offset(0x81));

    std::cout << "Action profile tests passed\n";
}
