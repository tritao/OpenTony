#include "player_state.hpp"

#include "tests/test_check.hpp"
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    opentony::runtime::PlayerState player({100, 200, 300});
    player.set_collision_response({1, 2, 3});
    player.set_physics_state(3);
    player.set_ground_update_state(1);
    player.begin_physics_frame();
    CHECK(player.previous_position() == player.position());

    const opentony::runtime::PositionCommitResult result = player.commit_position(
        {200, 400, 600},
        [](const opentony::runtime::FixedPosition& candidate) {
            return candidate[0] > 150;
        });
    CHECK(result.position == opentony::runtime::FixedPosition({100, 400, 600}));
    CHECK(player.position() == result.position);
    CHECK(player.previous_position() == opentony::runtime::FixedPosition({100, 200, 300}));
    CHECK(player.collision_response() == opentony::runtime::FixedPosition({1, 2, 3}));

    player.set_motion_correction({4, 5, 6});
    player.set_air_motion({7, 8, 9});
    player.apply_restart({100, 200, 300}, 0x44556677, 0x8899);
    CHECK(player.position() == opentony::runtime::FixedPosition({100, 200, 300}));
    CHECK(player.previous_position() == player.position());
    CHECK(player.collision_response() == opentony::runtime::FixedPosition({0, 0, 0}));
    CHECK(player.motion_correction() == opentony::runtime::FixedPosition({0, 0, 0}));
    CHECK(player.air_motion() == opentony::runtime::FixedPosition({0, 0, 0}));
    CHECK(player.restart_auxiliary() == 0x44556677);
    CHECK(player.restart_auxiliary_word() == 0x8899);
    CHECK(opentony::runtime::retail_restart_angle12(0x08000000) == 0);
    CHECK(player.orientation() == opentony::runtime::q12_restart_matrix(
        0x44556677));
    CHECK(player.physics_state() == 3);
    CHECK(player.ground_update_state() == 1);
    opentony::runtime::InputState input;
    input.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Left));
    const auto turn = player.update_ground_turn(input);
    CHECK(turn.accumulator == -0x3c00);
    CHECK(player.turn_mirror() == -0x3c00);
    // The accumulator producer runs before B010. Retail defers the -4
    // table-unit yaw until the 0x00496360 pre-query phase.
    const auto pre_turn_orientation = player.orientation();
    player.apply_ground_turn_velocity_phase();
    CHECK(player.orientation() != pre_turn_orientation);
    CHECK(player.orientation().at(1, 1) == -0x1000);
    CHECK(player.retail_basis().at_310c[1] == -0x1000);

    opentony::runtime::PlayerState air_orientation;
    air_orientation.set_physics_state(1);
    opentony::runtime::InputState up_input;
    up_input.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Up));
    const auto air_angle = air_orientation.compute_in_air_orientation_angle(
        up_input,
        640,
        true);
    CHECK(air_angle.has_value());
    CHECK(*air_angle == -50);

    const opentony::runtime::Q12Matrix3 captured_orientation{
        {-4096, 0, -4, 0, -4091, -191, 4, 191, -4091}};
    air_orientation.set_orientation(captured_orientation);
    const auto pivot_delta = air_orientation.apply_in_air_orientation_pivot(-50);
    CHECK(pivot_delta == opentony::runtime::FixedPosition({0, -1890, -21840}));
    const opentony::runtime::Q12Matrix3 expected_air_orientation{
        {-4096, 0, -4, 0, -4064, -504, 4, 503, -4064}};
    CHECK(air_orientation.orientation() == expected_air_orientation);

    const opentony::runtime::AirOrientationTurnConfig air_turn_config{
        80,
        0,
        0x100,
        true,
    };
    CHECK(opentony::runtime::compute_air_orientation_turn_angle(
        45 * 0x1000,
        air_turn_config) == 42);
    opentony::runtime::PlayerState air_turn;
    air_turn.set_physics_state(1);
    opentony::runtime::InputState right_input;
    right_input.begin_frame(opentony::runtime::movement_bit(
        opentony::runtime::MovementAction::Right));
    air_turn.update_in_air_orientation_accumulator(right_input, 0x100);
    CHECK(air_turn.turn_accumulator() == 0xa000);
    CHECK(air_turn.apply_in_air_orientation_turn(
        opentony::runtime::AirOrientationTurnConfig{
            80,
            0,
            0x100,
            true,
        }) == 9);
    air_turn.set_orientation(opentony::runtime::q12_identity_matrix());
    air_turn.set_turn_accumulator(45 * 0x1000);
    CHECK(air_turn.apply_in_air_orientation_turn(air_turn_config) == 42);
    CHECK(air_turn.orientation()
        == opentony::runtime::q12_yaw_matrix(-42));

    player.set_collision_response({0x1000, 0x2000, 0});
    CHECK(player.remove_collision_normal_component({0x1000, 0, 0}) == 0x1000);
    CHECK(player.collision_response() == opentony::runtime::FixedPosition({0, 0x2000, 0}));
    player.set_collision_response({-0x1000, 0, 0});
    const auto response = player.apply_collision_response({0x1000, 0, 0});
    CHECK(response.dot == -0x1000);
    CHECK(response.adjusted);
    CHECK(player.collision_response() == opentony::runtime::FixedPosition({0xcd, 0, 0}));

    opentony::runtime::PlayerState collision_orientation;
    collision_orientation.set_air_motion({0, 0x1000, 0});
    const auto orientation_result =
        collision_orientation.apply_collision_orientation({0, 0, 0x1000}, 0x200);
    CHECK(orientation_result.adjusted);
    CHECK(orientation_result.forward_dot == -0x1000);
    CHECK(orientation_result.lateral_dot == 0);
    CHECK(collision_orientation.air_motion()
        == opentony::runtime::FixedPosition({0, 0x1000, 0}));
    CHECK(collision_orientation.orientation()
        != opentony::runtime::q12_identity_matrix());

    player.apply_bouncy_platform_response(2, {100, 0, -200}, 0);
    CHECK(player.collision_response()
        == opentony::runtime::FixedPosition({150, -0x50000, -300}));
    player.apply_bouncy_platform_response(1, {100, 0, -200}, 0x20);
    CHECK(player.collision_response()
        == opentony::runtime::FixedPosition({100, -0x20000, -200}));
    player.apply_bouncy_platform_response(1, {100, 0, -200}, 0x28);
    CHECK(player.collision_response()
        == opentony::runtime::FixedPosition({100, -0x40000, -200}));

    opentony::runtime::PlayerState ollie_player;
    ollie_player.set_physics_state(0);
    opentony::runtime::InputState ollie_input;
    ollie_input.begin_frame(opentony::runtime::kKickActionBit);
    const auto charging = ollie_player.run_ollie_prephysics(ollie_input);
    CHECK(charging.event == opentony::runtime::OlliePrePhysicsEvent::Charging);
    CHECK(charging.latch_set);
    CHECK(charging.pending_set);
    CHECK(charging.animation_request_issued);
    CHECK(charging.animation_request_id == 8);
    CHECK(charging.animation_request_start == 0);
    CHECK(charging.animation_request_end == 0x1a);
    CHECK(charging.animation_request_alternate == 0x13);
    CHECK(ollie_player.ollie().charge == 1);

    ollie_input.begin_frame(0);
    const auto launched = ollie_player.run_ollie_prephysics(ollie_input);
    CHECK(launched.event == opentony::runtime::OlliePrePhysicsEvent::Launched);
    CHECK(launched.state_requested);
    CHECK(launched.requested_state == 1);
    CHECK(launched.request_reason == opentony::runtime::kOrdinaryLaunchReason);
    CHECK(ollie_player.physics_state() == 1);
    CHECK(ollie_player.last_state_request().from == 0);
    CHECK(ollie_player.last_state_request().to == 1);
    CHECK(ollie_player.ollie().launch_count == 1);

    opentony::runtime::PlayerState air_hold;
    air_hold.set_physics_state(1);
    air_hold.set_collision_response({10, 20, 30});
    air_hold.set_motion_correction({40, 50, 60});
    opentony::runtime::InputState jump_input;
    jump_input.begin_frame(opentony::runtime::kJumpActionBit);
    jump_input.begin_frame(opentony::runtime::kJumpActionBit);
    jump_input.begin_frame(opentony::runtime::kJumpActionBit);
    CHECK(air_hold.apply_in_air_jump_hold_effect(jump_input));
    CHECK(air_hold.collision_response()[1] == 0);
    CHECK(air_hold.motion_correction()[1] == 0);

    opentony::runtime::PlayerState ground_player;
    ground_player.set_collision_response({0x1000, 0, 0});
    ground_player.clear_motion_correction();
    ground_player.prepare_ground_basis_correction(false);
    CHECK(ground_player.collision_response()
        == opentony::runtime::FixedPosition({0, 0, 0}));
    CHECK(ground_player.motion_correction()
        == opentony::runtime::FixedPosition({0, 0, 0}));

    ground_player.set_collision_response({0, 0, 0x1000});
    ground_player.clear_motion_correction();
    ground_player.prepare_ground_basis_correction(true, 0x100);
    CHECK(ground_player.motion_correction()[2] == -8);
    ground_player.clear_motion_correction();
    ground_player.set_collision_response({0, 0, 0x1000});
    ground_player.prepare_ground_basis_correction(true, 0x280);
    CHECK(ground_player.motion_correction()[2] == -20);

    opentony::runtime::PlayerState blocked_player;
    blocked_player.set_control_blocked(true);
    blocked_player.set_collision_response({100, -30, 50});
    blocked_player.set_motion_correction({1, 2, 3});
    blocked_player.set_control_blocked_velocity_decay_divisor(4);
    blocked_player.apply_control_blocked_reset();
    CHECK(blocked_player.collision_response()
        == opentony::runtime::FixedPosition({75, 0, 38}));
    CHECK(blocked_player.motion_correction()
        == opentony::runtime::FixedPosition({0, 2, 0}));

    opentony::runtime::PlayerState integrated;
    integrated.set_collision_response({100, 200, 300});
    integrated.set_motion_correction({0x1000, 0, 0});
    integrated.integrate_position();
    // dt=1: velocity contributes directly; the frame-scaled correction is
    // then divided by two by the retail helper sequence.
    CHECK(integrated.position()
        == opentony::runtime::FixedPosition({2148, 200, 300}));

    opentony::runtime::PlayerState action_player;
    const std::vector<std::uint8_t> response_command{
        opentony::runtime::kSetResponseVectorOpcode,
        0x03, 0x00,
        0xfe, 0xff,
        0x01, 0x00,
    };
    std::size_t action_cursor = 0;
    const auto action_result = action_player.dispatch_action_command(
        response_command,
        action_cursor);
    CHECK(action_result.recognized);
    CHECK(action_cursor == response_command.size());
    CHECK(action_player.collision_response()
        == opentony::runtime::FixedPosition({
            0x3000,
            -0x2000,
            0x1000,
        }));
    std::cout << "Player state tests passed\n";
}
