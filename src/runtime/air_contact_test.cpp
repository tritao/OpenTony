#include "air_contact.hpp"
#include "physics_frame.hpp"

#include "tests/test_check.hpp"
#include <iostream>
#include <optional>

int main() {
    const opentony::runtime::PositionCollisionHit ordinary{
        1, 2, 3, 4, 0x2000, {0, 0, 0}, {0, 0x1000, 0}, 0, 0, 0};
    CHECK(opentony::runtime::accepts_standard_air_contact(
        ordinary, 1, false, 0, 1, {false, 0}));

    const opentony::runtime::PositionCollisionHit rejected{
        1, 2, 3, 4, 0x2000, {0, 0, 0}, {0, 0x1000, 0}, 0, 0,
        0x01c00000U};
    CHECK(!opentony::runtime::accepts_standard_air_contact(
        rejected, 1, true, 0, 100, {false, 0}));
    CHECK(opentony::runtime::accepts_standard_air_contact(
        rejected, 3, true, 0, 100, {false, 0}));

    const opentony::runtime::PositionCollisionHit wall_like{
        1, 2, 3, 4, 0x2000, {0, 0, 0}, {0, 0x1000, 0}, 0x80, 0,
        0x00400000U};
    CHECK(!opentony::runtime::accepts_standard_air_contact(
        wall_like, 1, true, 0, 100, {false, 0}));
    CHECK(opentony::runtime::accepts_standard_air_contact(
        wall_like, 1, true, 0, 100, {true, 0}));

    opentony::runtime::PlayerState player({0, 100, 0});
    player.set_physics_state(3);
    player.set_collision_response({0, -200, 0});
    opentony::runtime::InputState input;
    input.begin_frame(0);
    opentony::runtime::PlayerPhysicsFrameHooks hooks{};
    hooks.apply_ground_turn = false;
    hooks.integrate_motion_correction = false;
    hooks.collision_query = [](
        const opentony::runtime::FixedPosition& start,
        const opentony::runtime::FixedPosition& end)
        -> std::optional<opentony::runtime::PositionCollisionHit> {
        if (start[1] > 0 && end[1] <= 0) {
            return opentony::runtime::PositionCollisionHit{
                1, 2, 3, 4, 0x2000, {0, 0, 0}, {0, 0x1000, 0}, 0, 0, 0};
        }
        return std::nullopt;
    };
    hooks.standard_air_contact_input = [](
        const opentony::runtime::PlayerState&,
        const opentony::runtime::InputState&,
        const opentony::runtime::PositionCollisionHit&)
        -> std::optional<opentony::runtime::StandardAirContactInput> {
        return opentony::runtime::StandardAirContactInput{false, 0};
    };
    const auto frame = opentony::runtime::PlayerPhysicsFrame::step(
        player, input, hooks);
    CHECK(frame.landed);
    CHECK(player.physics_state() == 0);

    std::cout << "Air contact tests passed\n";
}
