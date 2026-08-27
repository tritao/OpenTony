#include "position_commit.hpp"

#include "tests/test_check.hpp"
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
    CHECK(opentony::runtime::accepts_retail_ground_contact(floor_hit));
    CHECK(!opentony::runtime::accepts_retail_ground_contact(slope_limit_hit));
    const auto slide = opentony::runtime::PositionCommitter::commit(
        current,
        desired,
        [](const opentony::runtime::FixedPosition& position) {
            return position[0] > 100;
        });
    const opentony::runtime::FixedPosition expected_slide{0, 300, 400};
    CHECK(slide.position == expected_slide);
    CHECK(slide.collided);
    CHECK(!slide.blocked);
    CHECK(slide.probes == 2);
    CHECK(slide.selected_candidate == static_cast<std::uint8_t>(
        opentony::runtime::PositionCommitCandidate::OldX));

    const auto blocked = opentony::runtime::PositionCommitter::commit(
        current,
        desired,
        [](const opentony::runtime::FixedPosition&) { return true; });
    CHECK(blocked.position == current);
    CHECK(blocked.collided);
    CHECK(blocked.blocked);
    CHECK(blocked.probes == 7);
    CHECK(blocked.selected_candidate == static_cast<std::uint8_t>(
        opentony::runtime::PositionCommitCandidate::CurrentFallback));

    const auto bypassed = opentony::runtime::PositionCommitter::commit(
        current,
        desired,
        [](const opentony::runtime::FixedPosition&) { return true; },
        true);
    CHECK(bypassed.position == desired);
    CHECK(!bypassed.collided);
    CHECK(bypassed.probes == 0);
    CHECK(bypassed.selected_candidate == static_cast<std::uint8_t>(
        opentony::runtime::PositionCommitCandidate::Direct));
    std::cout << "Position commit tests passed\n";
}
