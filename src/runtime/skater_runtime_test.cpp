#include "skater_runtime.hpp"

#include <cassert>

int main() {
    opentony::runtime::SkaterRuntimeObject first(0, 3, 17, {1, 2, 3});
    opentony::runtime::SkaterRuntimeObject second(1, 4, 18, {4, 5, 6});
    first.set_peer(&second);
    second.set_peer(&first);
    assert(first.allocation_size() == 0x3538);
    assert(first.psx_region_slot() == 3);
    assert(first.model_index() == 17);
    assert(first.player_index() == 0);
    const opentony::runtime::FixedPosition expected_position{1, 2, 3};
    assert(first.player().position() == expected_position);
    assert(first.camera().allocation_size() == 0x674);
    assert(first.camera().parent_player_index() == 0);
    assert(first.camera().mode() == 1);
    assert(first.peer() == &second);
    first.camera().set_mode(23);
    first.camera().set_update_tick(4);
    assert(first.camera().mode() == 23);
    assert(first.camera().update_tick() == 4);
    assert(second.peer() == &first);
    return 0;
}
