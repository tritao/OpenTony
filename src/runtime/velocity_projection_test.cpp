#include "velocity_projection.hpp"
#include "player_state.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::FixedPosition;
    using opentony::runtime::PlayerState;
    using opentony::runtime::project_velocity_preserving_magnitude;

    const auto projected = project_velocity_preserving_magnitude(
        FixedPosition{0x1000, 0x1000, 0},
        FixedPosition{0x1000, 0, 0});
    assert(projected.original_magnitude_q12 == 5792);
    assert(projected.projected_magnitude_q12 == 4096);
    assert(projected.rescaled);
    assert(projected.velocity == FixedPosition({0, 5792, 0}));

    const auto parallel = project_velocity_preserving_magnitude(
        FixedPosition{0x1000, 0, 0},
        FixedPosition{0x1000, 0, 0});
    assert(parallel.original_magnitude_q12 == 0x1000);
    assert(parallel.projected_magnitude_q12 == 0);
    assert(!parallel.rescaled);
    assert(parallel.velocity == FixedPosition({0, 0, 0}));

    PlayerState player;
    player.set_collision_response({0x1000, 0x1000, 0});
    const auto player_result = player.project_collision_velocity({0x1000, 0, 0});
    assert(player_result == projected);
    assert(player.collision_response() == projected.velocity);

    std::cout << "Velocity projection tests passed\n";
}
