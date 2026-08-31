#include "collision_recovery.hpp"

#include "player_state.hpp"
#include "tests/test_check.hpp"

#include <iostream>

namespace {

using opentony::runtime::FixedPosition;
using opentony::runtime::PlayerState;
using opentony::runtime::PositionCollisionHit;

PositionCollisionHit make_hit(
    FixedPosition position,
    std::uint32_t raw_collision_word = 0) {
    PositionCollisionHit hit{};
    hit.position = position;
    hit.normal = {0, 0x1000, 0};
    hit.raw_collision_word = raw_collision_word;
    return hit;
}

} // namespace

int main() {
    using opentony::runtime::apply_outer_floor_recovery;

    PlayerState upward_player({100, 200, 300});
    upward_player.set_outer_floor_reference_position({10, 20, 30});
    bool service_called = false;
    const auto upward_result = apply_outer_floor_recovery(
        upward_player,
        upward_player.position(),
        [](const FixedPosition&, const FixedPosition&)
            -> std::optional<PositionCollisionHit> {
            return make_hit({100, 1000, 300});
        },
        false,
        [&service_called](const PositionCollisionHit& hit) {
            service_called = hit.position[1] == 1000;
        });
    CHECK(upward_result.upward_hit);
    CHECK(upward_result.external_service_requested);
    CHECK(service_called);
    CHECK(upward_player.outer_floor_distance() == 800 - 0x1e000);
    CHECK(upward_player.position() == FixedPosition({100, 200, 300}));

    PlayerState restart_player({100, 200, 300});
    restart_player.set_outer_floor_reference_position({10, 20, 30});
    int restart_calls = 0;
    const auto restart_result = apply_outer_floor_recovery(
        restart_player,
        restart_player.position(),
        [&restart_calls](const FixedPosition&, const FixedPosition& end)
            -> std::optional<PositionCollisionHit> {
            ++restart_calls;
            if (restart_calls == 1) {
                return make_hit({100, 1000, 300});
            }
            CHECK(end[1] == 200);
            return make_hit({100, 150, 300});
        },
        true);
    CHECK(restart_result.restart_probe_hit);
    CHECK(restart_result.response_yaw_applied);
    CHECK(restart_player.position() == FixedPosition({10, 20, 30}));
    CHECK(restart_player.previous_position() == FixedPosition({10, 20, 30}));

    PlayerState short_player({100, 200, 300});
    int short_calls = 0;
    const auto short_result = apply_outer_floor_recovery(
        short_player,
        short_player.position(),
        [&short_calls](const FixedPosition&, const FixedPosition& end)
            -> std::optional<PositionCollisionHit> {
            ++short_calls;
            if (short_calls == 2) {
                CHECK(end[1] == 200 + 0x64000);
                return make_hit({700, 500, 900});
            }
            return std::nullopt;
        },
        false);
    CHECK(!short_result.upward_hit);
    CHECK(short_result.short_recovery_hit);
    CHECK(short_player.position() == FixedPosition({700, 500 - 0x1e000, 900}));
    CHECK(short_player.previous_position() == short_player.position());

    PlayerState fallback_player({100, 200, 300});
    fallback_player.set_outer_floor_reference_position({10, 20, 30});
    fallback_player.set_collision_response({1, 2, 100});
    const auto fallback_result = apply_outer_floor_recovery(
        fallback_player,
        fallback_player.position(),
        [](const FixedPosition&, const FixedPosition&)
            -> std::optional<PositionCollisionHit> {
            return std::nullopt;
        },
        false);
    CHECK(fallback_result.z_recovery_hit == false);
    CHECK(fallback_player.position() == FixedPosition({10, 200, 30}));
    CHECK(fallback_player.previous_position() == fallback_player.position());
    CHECK(fallback_player.collision_response() == FixedPosition({-50, 2, -25}));

    std::cout << "Collision recovery tests passed\n";
}
