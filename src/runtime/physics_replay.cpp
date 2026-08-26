#include "physics_replay.hpp"

namespace opentony::runtime {

std::vector<PlayerReplaySnapshot> PlayerPhysicsReplay::run(
    PlayerState player,
    const std::vector<PlayerReplayInput>& inputs,
    const PlayerPhysicsFrameHooks& hooks) {
    InputState input;
    std::vector<PlayerReplaySnapshot> snapshots;
    snapshots.reserve(inputs.size());
    for (const PlayerReplayInput& frame : inputs) {
        input.begin_frame(
            frame.action_mask,
            frame.horizontal_axis,
            frame.vertical_axis);
        const PlayerPhysicsFrameResult result = PlayerPhysicsFrame::step(
            player,
            input,
            hooks,
            frame.frame_scale_q8);
        PlayerReplaySnapshot snapshot{};
        snapshot.action_mask = input.action_mask();
        snapshot.effective_movement_mask = input.effective_movement_mask();
        snapshot.position = player.position();
        snapshot.previous_position = player.previous_position();
        snapshot.collision_response = player.collision_response();
    snapshot.motion_correction = player.motion_correction();
    snapshot.air_motion = player.air_motion();
    snapshot.orientation = player.orientation();
    snapshot.retail_basis = player.retail_basis();
    snapshot.restart_auxiliary = player.restart_auxiliary();
    snapshot.restart_auxiliary_word = player.restart_auxiliary_word();
        snapshot.physics_state = player.physics_state();
        snapshot.ground_update_state = player.ground_update_state();
        snapshot.ground_physics_mode = player.ground_physics_mode();
        snapshot.turn_accumulator = player.turn_accumulator();
        snapshot.dispatch_handled = result.dispatch.handled;
        snapshot.position_commit = result.position_commit;
    snapshot.collision_hit = result.collision_hit;
    snapshot.collision_orientation = result.collision_orientation;
        snapshot.hit_normal_removed = result.hit_normal_removed;
        snapshot.landed = result.landed;
        snapshot.in_air_jump_hold_applied = result.in_air_jump_hold_applied;
        snapshot.velocity_damped = result.velocity_damped;
        snapshot.velocity_damping = result.velocity_damping;
    snapshot.ground_brake = result.ground_brake;
        snapshot.ground_physics = result.ground_physics;
        snapshot.ollie = result.ollie;
        snapshot.ollie_state = player.ollie();
        snapshot.state_request = player.last_state_request();
        snapshots.push_back(snapshot);
    }
    return snapshots;
}

} // namespace opentony::runtime
