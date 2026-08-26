#include "skater_runtime.hpp"

#include "tests/test_check.hpp"

int main() {
    opentony::runtime::SkaterRuntimeObject first(0, 3, 17, {1, 2, 3});
    opentony::runtime::SkaterRuntimeObject second(1, 4, 18, {4, 5, 6});
    first.set_peer(&second);
    second.set_peer(&first);
    CHECK(first.allocation_size() == 0x3538);
    CHECK(first.psx_region_slot() == 3);
    CHECK(first.model_index() == 17);
    CHECK(first.player_index() == 0);
    const opentony::runtime::FixedPosition expected_position{1, 2, 3};
    CHECK(first.player().position() == expected_position);
    CHECK(first.camera().allocation_size() == 0x674);
    CHECK(first.camera().parent_player_index() == 0);
    CHECK(first.camera().mode() == 1);
    CHECK(first.peer() == &second);
    first.camera().set_mode(23);
    first.camera().set_update_tick(4);
    CHECK(first.camera().mode() == 23);
    CHECK(first.camera().update_tick() == 4);
    CHECK(second.peer() == &first);
    return 0;
}
