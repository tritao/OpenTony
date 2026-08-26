#include "physics_frame.hpp"

namespace opentony::runtime {
namespace {

bool has_position_integrator(PhysicsDispatchStage stage) noexcept {
    return stage == PhysicsDispatchStage::GroundCollision_96550
        || stage == PhysicsDispatchStage::InAir_97f40;
}

} // namespace

PlayerPhysicsFrameResult PlayerPhysicsFrame::step(
    PlayerState& player,
    const InputState& input,
    const PlayerPhysicsFrameHooks& hooks,
    std::int32_t frame_scale_q8) {
    PlayerPhysicsFrameResult result{};
    result.action_profile = map_action_profile(
        input.action_mask(),
        input.horizontal_axis(),
        input.vertical_axis());
    result.position_commit.position = player.position();

    player.begin_physics_frame();
    result.queued_motion = player.drain_queued_motion(frame_scale_q8);
    if (hooks.on_queued_motion) {
        hooks.on_queued_motion(player, result.queued_motion);
    }

    // FUN_00493370 runs before the correction reset and the state dispatcher.
    // The native turn producer is deliberately opt-out because its remaining
    // release/analog branches still depend on unrecovered stat fields.
    if (hooks.apply_ground_turn
        && (player.physics_state() == 0 || player.physics_state() == 7)) {
        GroundTurnConfig turn_config = hooks.ground_turn_config;
        // FUN_00493370 selects +0x5a000 when +0x31a2 exceeds 0x1e or the
        // profile's Down slot is active. +0x31a2 is copied from action-state
        // +0x148, the retained vertical axis.
        if (turn_config.limit == 0x2d000 &&
            (result.action_profile.vertical_axis > 0x1e ||
             result.action_profile.slot_at_offset(0xb0))) {
            turn_config.limit = 0x5a000;
        }
        // +0x31a1 is copied from action-state +0x149, the horizontal lean.
        if (turn_config.lean == 0) {
            turn_config.lean = result.action_profile.horizontal_axis;
        }
        result.ground_turn = player.update_ground_turn(
            input,
            turn_config);
    }
    GroundAnimationInput animation_input{
        player.turn_mirror(),
        result.action_profile.vertical_axis,
        result.action_profile.slot_at_offset(0xb0),
        false,
        false,
        player.animation_state(),
        player.animation_frame(),
    };
    if (hooks.ground_animation_input) {
        animation_input = hooks.ground_animation_input(
            player,
            input,
            result.action_profile);
    }
    result.ground_animation = player.update_ground_animation(animation_input);
    // Retail FUN_0049e680 clears +0x58/+0x5c/+0x60 after FUN_00493370 has
    // produced its transient turn correction, immediately before B010.
    player.clear_motion_correction();
    // FUN_0049b010 decrements +0x2f2c before evaluating its ordinary branch.
    player.tick_ground_motion_cooldown();
    if (hooks.ground_motion_input) {
        const std::optional<GroundMotionInput> ground_motion_input =
            hooks.ground_motion_input(player, input, result.action_profile);
        if (ground_motion_input.has_value()) {
            GroundMotionInput resolved = *ground_motion_input;
            if (resolved.use_player_state) {
                resolved.strong_profile = resolved.strong_profile ||
                    result.action_profile.slot_at_offset(0x10);
                resolved.producer_enabled = resolved.producer_enabled ||
                    resolved.strong_profile;
                resolved.ordinary_ground_state =
                    player.physics_state() == 0;
                resolved.correction_gate_open =
                    resolved.correction_gate_open &&
                    player.ground_motion_correction_gate_open();
                resolved.cooldown_active =
                    resolved.cooldown_active ||
                    player.ground_motion_cooldown() > 0;
                resolved.animation_state =
                    static_cast<std::int16_t>(player.animation_state());
                resolved.animation_frame = player.animation_frame();
                if (resolved.response_speed_metric < 0) {
                    resolved.response_speed_metric =
                        player.ground_motion_speed_metric();
                }
                if (resolved.response_speed_threshold < 0) {
                    resolved.response_speed_threshold =
                        player.ground_motion_threshold();
                }
                if (resolved.forward_basis_y == 0) {
                    resolved.forward_basis_y =
                        player.retail_basis().at_30f4[1];
                }
            }
            result.ground_motion = player.apply_ground_motion(resolved);
        }
    }
    if (hooks.on_prephysics) {
        hooks.on_prephysics(player, input);
    }
    if (hooks.ollie_input) {
        result.ollie = player.run_ollie_prephysics(
            input,
            hooks.ollie_input(player, input));
    }
    if (hooks.on_action_stream) {
        hooks.on_action_stream(player);
    }
    if (hooks.ground_brake_input) {
        const std::optional<GroundBrakeInput> brake_input =
            hooks.ground_brake_input(player, input);
        if (brake_input.has_value()) {
            result.ground_brake = player.apply_ground_brake(*brake_input);
        }
    }

    const PhysicsDispatchHooks dispatch_hooks{
        [&result, &hooks, &input, frame_scale_q8](
            PhysicsDispatchStage stage,
            PlayerState& current_player) {
            if (hooks.on_stage) {
                hooks.on_stage(stage, current_player, input);
            }

            if (stage == PhysicsDispatchStage::InAir_97f40 &&
                hooks.air_gravity_input) {
                const std::optional<AirGravityConfig> gravity_input =
                    hooks.air_gravity_input(current_player, input);
                if (gravity_input.has_value()) {
                    result.air_gravity = current_player.apply_air_gravity(
                        *gravity_input);
                    if (hooks.apply_air_motion_basis) {
                        result.air_motion_basis =
                            current_player.update_air_motion_basis();
                    }
                }
            }

            if (stage == PhysicsDispatchStage::InAir_97f40 &&
                hooks.air_direction_input) {
                const std::optional<AirDirectionInputConfig> direction_input =
                    hooks.air_direction_input(current_player, input);
                if (direction_input.has_value()) {
                    result.air_direction_input =
                        current_player.apply_air_direction_input(
                            input,
                            *direction_input);
                }
            }
            if (stage == PhysicsDispatchStage::InAir_97f40 &&
                !hooks.air_direction_input && hooks.air_speed_input) {
                const std::optional<AirSpeedConfig> speed_input =
                    hooks.air_speed_input(current_player, input);
                if (speed_input.has_value()) {
                    result.air_direction_input =
                        current_player.apply_air_direction_input(
                            input,
                            *speed_input);
                }
            }

            if (stage == PhysicsDispatchStage::InAir_97f40 &&
                hooks.apply_in_air_jump_hold_effect) {
                result.in_air_jump_hold_applied =
                    current_player.apply_in_air_jump_hold_effect(input);
            }

            if (!has_position_integrator(stage) || !hooks.integrate_position) {
                return;
            }

            // Keep the old live position as the current point for the shared
            // FUN_00496060 axis fallback while testing the integrated point as
            // the desired position.
            const FixedPosition start = current_player.position();
            const FixedPosition desired = current_player.integrated_position(
                frame_scale_q8);
            PositionCollisionProbe probe = hooks.collision_probe;
            std::optional<PositionCollisionHit> queried_hit;
            if (hooks.collision_query) {
                probe = [&hooks, &start, &queried_hit](
                    const FixedPosition& candidate) {
                    const std::optional<PositionCollisionHit> hit =
                        hooks.collision_query(start, candidate);
                    if (hit.has_value() && !queried_hit.has_value()) {
                        queried_hit = hit;
                    }
                    return hit.has_value();
                };
            }
            result.position_commit = current_player.commit_position(
                desired,
                probe,
                hooks.bypass_collision);
            if (hooks.collision_query) {
                result.collision_hit = queried_hit;
            }
            if (result.collision_hit.has_value()
                && hooks.collision_response_bias_q12) {
                const std::optional<std::int32_t> bias =
                    hooks.collision_response_bias_q12(
                        current_player,
                        *result.collision_hit,
                        stage);
                if (bias.has_value()) {
                    result.collision_response =
                        current_player.apply_collision_response(
                            result.collision_hit->normal,
                            *bias);
                }
            }
            if (result.collision_hit.has_value()
                && hooks.collision_orientation_yaw) {
                const std::optional<std::int32_t> yaw =
                    hooks.collision_orientation_yaw(
                        current_player,
                        *result.collision_hit,
                        stage);
                if (yaw.has_value()) {
                    result.collision_orientation =
                        current_player.apply_collision_orientation(
                            result.collision_hit->normal,
                            *yaw);
                }
            }
            if (result.collision_hit.has_value()
                && hooks.remove_hit_normal_component) {
                static_cast<void>(current_player.remove_collision_normal_component(
                    result.collision_hit->normal));
                result.hit_normal_removed = true;
            }
            if (result.collision_hit.has_value() && hooks.on_collision) {
                hooks.on_collision(
                    current_player,
                    *result.collision_hit,
                    result.position_commit);
            }
            if (result.collision_hit.has_value() &&
                stage == PhysicsDispatchStage::InAir_97f40 &&
                hooks.on_air_contact &&
                hooks.on_air_contact(
                    current_player,
                    *result.collision_hit,
                    result.position_commit)) {
                result.landed = current_player.accept_air_contact(
                    result.position_commit.position);
            }
            result.position_integrated = true;
        },
    };
    result.dispatch = PhysicsDispatcher::dispatch(player, dispatch_hooks);

    if (hooks.integrate_motion_correction) {
        // This is the outer-frame +58 -> +4c handoff after dispatch.
        player.integrate_motion_correction(frame_scale_q8);
        result.motion_correction_integrated = true;
    }
    if (hooks.ground_motion_threshold_input) {
        const std::optional<GroundMotionThresholdInput> threshold_input =
            hooks.ground_motion_threshold_input(player, input);
        if (threshold_input.has_value()) {
            result.ground_motion_threshold =
                player.update_ground_motion_threshold(*threshold_input);
        }
    }
    if (hooks.velocity_damping_input) {
        const std::optional<VelocityDampingInput> damping_input =
            hooks.velocity_damping_input(player, result.dispatch);
        if (damping_input.has_value()) {
            result.velocity_damping = player.apply_velocity_damping(
                *damping_input);
            result.velocity_damped = true;
        }
    }
    if (hooks.on_postphysics) {
        hooks.on_postphysics(player, result.dispatch);
    }
    return result;
}

} // namespace opentony::runtime
