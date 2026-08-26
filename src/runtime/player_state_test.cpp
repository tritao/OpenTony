#include "player_state.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    opentony::runtime::PlayerState player({100, 200, 300});
    player.set_collision_response({1, 2, 3});
    player.set_physics_state(3);
    player.set_ground_update_state(1);
    player.begin_physics_frame();
    assert(player.previous_position() == player.position());

    const opentony::runtime::PositionCommitResult result = player.commit_position(
        {200, 400, 600},
        [](const opentony::runtime::FixedPosition& candidate) {
            return candidate[0] > 150;
        });
    assert(result.position == opentony::runtime::FixedPosition({100, 400, 600}));
    assert(player.position() == result.position);
    assert(player.previous_position() == opentony::runtime::FixedPosition({100, 200, 300}));
    assert(player.collision_response() == opentony::runtime::FixedPosition({1, 2, 3}));

    player.set_motion_correction({4, 5, 6});
    player.set_air_motion({7, 8, 9});
    player.apply_restart({100, 200, 300}, 0x44556677, 0x8899);
    assert(player.position() == opentony::runtime::FixedPosition({100, 200, 300}));
    assert(player.previous_position() == player.position());
    assert(player.collision_response() == opentony::runtime::FixedPosition({0, 0, 0}));
    assert(player.motion_correction() == opentony::runtime::FixedPosition({0, 0, 0}));
    assert(player.air_motion() == opentony::runtime::FixedPosition({0, 0, 0}));
    assert(player.restart_auxiliary() == 0x44556677);
    assert(player.restart_auxiliary_word() == 0x8899);
    assert(player.physics_state() == 3);
    assert(player.ground_update_state() == 1);
    opentony::runtime::InputState input;
    input.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left));
    const auto turn = player.update_ground_turn(input);
    assert(turn.accumulator == -0x3c00);
    assert(player.turn_mirror() == -0x3c00);
    // The -4 table-unit yaw is applied to the live Q12 orientation and its
    // three retail basis handoff vectors are refreshed in the same update.
    assert(player.orientation().at(1, 1) == 0x1000);
    assert(player.retail_basis().at_310c[1] == 0x1000);
    player.set_collision_response({0x1000, 0x2000, 0});
    assert(player.remove_collision_normal_component({0x1000, 0, 0}) == 0x1000);
    assert(player.collision_response() == opentony::runtime::FixedPosition({0, 0x2000, 0}));
    player.set_collision_response({-0x1000, 0, 0});
    const auto response = player.apply_collision_response({0x1000, 0, 0});
    assert(response.dot == -0x1000);
    assert(response.adjusted);
    assert(player.collision_response() == opentony::runtime::FixedPosition({0xcd, 0, 0}));

    opentony::runtime::PlayerState collision_orientation;
    collision_orientation.set_air_motion({0, 0x1000, 0});
    const auto orientation_result =
        collision_orientation.apply_collision_orientation({0, 0, 0x1000}, 0x200);
    assert(orientation_result.adjusted);
    assert(orientation_result.forward_dot == -0x1000);
    assert(orientation_result.lateral_dot == 0);
    assert(collision_orientation.air_motion()
        == opentony::runtime::FixedPosition({0, 0x1000, 0}));
    assert(collision_orientation.orientation()
        != opentony::runtime::q12_identity_matrix());

    player.apply_bouncy_platform_response(2, {100, 0, -200}, 0);
    assert(player.collision_response()
        == opentony::runtime::FixedPosition({150, -0x50000, -300}));
    player.apply_bouncy_platform_response(1, {100, 0, -200}, 0x20);
    assert(player.collision_response()
        == opentony::runtime::FixedPosition({100, -0x20000, -200}));
    player.apply_bouncy_platform_response(1, {100, 0, -200}, 0x28);
    assert(player.collision_response()
        == opentony::runtime::FixedPosition({100, -0x40000, -200}));

    opentony::runtime::PlayerState ollie_player;
    ollie_player.set_physics_state(0);
    opentony::runtime::InputState ollie_input;
    ollie_input.begin_frame(opentony::runtime::kKickActionBit);
    const auto charging = ollie_player.run_ollie_prephysics(ollie_input);
    assert(charging.event == opentony::runtime::OlliePrePhysicsEvent::Charging);
    assert(charging.latch_set);
    assert(charging.pending_set);
    assert(ollie_player.ollie().charge == 1);

    ollie_input.begin_frame(0);
    const auto launched = ollie_player.run_ollie_prephysics(ollie_input);
    assert(launched.event == opentony::runtime::OlliePrePhysicsEvent::Launched);
    assert(launched.state_requested);
    assert(launched.requested_state == 3);
    assert(launched.request_reason == opentony::runtime::kAlternateLaunchReason);
    assert(ollie_player.physics_state() == 3);
    assert(ollie_player.last_state_request().from == 0);
    assert(ollie_player.last_state_request().to == 3);
    assert(ollie_player.ollie().launch_count == 1);

    opentony::runtime::PlayerState air_hold;
    air_hold.set_physics_state(1);
    air_hold.set_collision_response({10, 20, 30});
    air_hold.set_motion_correction({40, 50, 60});
    opentony::runtime::InputState jump_input;
    jump_input.begin_frame(opentony::runtime::kJumpActionBit);
    jump_input.begin_frame(opentony::runtime::kJumpActionBit);
    jump_input.begin_frame(opentony::runtime::kJumpActionBit);
    assert(air_hold.apply_in_air_jump_hold_effect(jump_input));
    assert(air_hold.collision_response()[1] == 0);
    assert(air_hold.motion_correction()[1] == 0);

    opentony::runtime::PlayerState ground_player;
    ground_player.set_collision_response({0x1000, 0, 0});
    ground_player.clear_motion_correction();
    ground_player.prepare_ground_basis_correction(false);
    assert(ground_player.motion_correction()
        == opentony::runtime::FixedPosition({-0x1000, 0, 0}));
    ground_player.integrate_motion_correction();
    assert(ground_player.collision_response()
        == opentony::runtime::FixedPosition({0, 0, 0}));

    ground_player.set_collision_response({0, 0, 0x1000});
    ground_player.clear_motion_correction();
    ground_player.prepare_ground_basis_correction(true);
    assert(ground_player.motion_correction()[2] == -8);

    opentony::runtime::PlayerState integrated;
    integrated.set_collision_response({100, 200, 300});
    integrated.set_motion_correction({0x1000, 0, 0});
    integrated.integrate_position();
    // dt=1: velocity contributes directly; correction contributes
    // (0x1000 >> 8) / 2 == 8 after the retail helper sequence.
    assert(integrated.position()
        == opentony::runtime::FixedPosition({108, 200, 300}));
    std::cout << "Player state tests passed\n";
}
