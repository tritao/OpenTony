#include "velocity_projection.hpp"
#include "player_state.hpp"

#include "tests/test_check.hpp"
#include <iostream>

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::PlayerState;
    using opentony::runtime::project_velocity_preserving_magnitude;

    const auto projected = project_velocity_preserving_magnitude(
        FixedPosition{0x1000, 0x1000, 0},
        FixedPosition{0x1000, 0, 0});
    CHECK(projected.original_magnitude_q12 == 5792);
    CHECK(projected.projected_magnitude_q12 == 4096);
    CHECK(projected.rescaled);
    CHECK(projected.velocity == FixedPosition({0, 5792, 0}));

    const auto parallel = project_velocity_preserving_magnitude(
        FixedPosition{0x1000, 0, 0},
        FixedPosition{0x1000, 0, 0});
    CHECK(parallel.original_magnitude_q12 == 0x1000);
    CHECK(parallel.projected_magnitude_q12 == 0);
    CHECK(!parallel.rescaled);
    CHECK(parallel.velocity == FixedPosition({0, 0, 0}));

    PlayerState player;
    player.set_collision_response({0x1000, 0x1000, 0});
    const auto player_result = player.project_collision_velocity({0x1000, 0, 0});
    CHECK(player_result == projected);
    CHECK(player.collision_response() == projected.velocity);

    std::cout << "Velocity projection tests passed\n";
}
