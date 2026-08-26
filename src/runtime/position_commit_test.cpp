#include "position_commit.hpp"

#include <cassert>
#include <iostream>

int main() {
    const opentony::runtime::FixedPosition current{0, 0, 0};
    const opentony::runtime::FixedPosition desired{200, 300, 400};
    const opentony::runtime::PositionCollisionHit floor_hit{
        0, 0, 0, 0, 0, {}, {0, 0x1000, 0}, 0, 0};
    const opentony::runtime::PositionCollisionHit slope_limit_hit{
        0,
        0,
        0,
        0,
        0,
        {},
        {0, opentony::runtime::kRetailGroundContactNormalYQ12, 0},
        0,
        0,
    };
    assert(opentony::runtime::accepts_retail_ground_contact(floor_hit));
    assert(!opentony::runtime::accepts_retail_ground_contact(slope_limit_hit));
    const auto slide = opentony::runtime::PositionCommitter::commit(
        current,
        desired,
        [](const opentony::runtime::FixedPosition& position) {
            return position[0] > 100;
        });
    const opentony::runtime::FixedPosition expected_slide{0, 300, 400};
    assert(slide.position == expected_slide);
    assert(slide.collided);
    assert(!slide.blocked);
    assert(slide.probes == 2);

    const auto blocked = opentony::runtime::PositionCommitter::commit(
        current,
        desired,
        [](const opentony::runtime::FixedPosition&) { return true; });
    assert(blocked.position == current);
    assert(blocked.collided);
    assert(blocked.blocked);
    assert(blocked.probes == 7);

    const auto bypassed = opentony::runtime::PositionCommitter::commit(
        current,
        desired,
        [](const opentony::runtime::FixedPosition&) { return true; },
        true);
    assert(bypassed.position == desired);
    assert(!bypassed.collided);
    assert(bypassed.probes == 0);
    std::cout << "Position commit tests passed\n";
}
