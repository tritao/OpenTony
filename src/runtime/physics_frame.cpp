#include "physics_frame.hpp"

#include "tricks_bin.hpp"

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
    result.physics_state_before = player.physics_state();
    result.action_profile = map_action_profile(
        input.action_mask(),
        input.horizontal_axis(),
        input.vertical_axis());
    result.position_commit.position = player.position();

    player.begin_physics_frame();
    // Retail canonicalizes the initially published grounded basis before
    // FUN_00493370 consumes it. The initial recording snapshot remains raw;
    // this is the one frame-boundary write that turns it canonical.
    if (player.physics_state() == 0 || player.physics_state() == 7) {
        player.normalize_orientation_basis();
    }
    result.queued_motion = player.drain_queued_motion(frame_scale_q8);
    if (hooks.apply_queued_motion) {
        result.queued_motion_world_delta = player.apply_queued_motion(
            result.queued_motion);
    }
    if (hooks.on_queued_motion) {
        hooks.on_queued_motion(player, result.queued_motion);
    }

    // FUN_00493370 runs before the correction reset and the state dispatcher.
    // The grounded turn producer includes the recovered signed +0x31a1
    // analog target; this caller still owns the limit/profile inputs and the
    // non-ground branches.
    if (hooks.apply_ground_turn
        && (player.physics_state() == 0 || player.physics_state() == 7)) {
        GroundTurnConfig turn_config = hooks.ground_turn_config;
        turn_config.frame_scale_q8 = frame_scale_q8;
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
                    resolved.strong_profile ||
                    resolved.profile_table_value_nonzero;
                resolved.ordinary_ground_state =
                    player.physics_state() == 0;
                resolved.correction_gate_open =
                    resolved.correction_gate_open &&
                    player.ground_motion_correction_gate_open();
                resolved.cooldown_active =
                    resolved.cooldown_active ||
                    player.ground_motion_cooldown() > 0;
                resolved.pending_animation_event =
                    resolved.pending_animation_event ||
                    player.ground_motion_event_pending();
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
            if (hooks.on_ground_motion_event
                && result.ground_motion->animation_event_written) {
                hooks.on_ground_motion_event(player, *result.ground_motion);
            }
        }
    }
    if (hooks.on_prephysics) {
        hooks.on_prephysics(player, input);
    }
    if (hooks.ollie_input) {
        result.ollie = player.run_ollie_prephysics(
            input,
            hooks.ollie_input(player, input));
        if (result.ollie->animation_request_issued &&
            hooks.on_ollie_animation_request) {
            hooks.on_ollie_animation_request(player, *result.ollie);
        }
    }
    // FUN_00492190 publishes the current profile immediately before
    // FUN_004925e0 scans the generated sequence table. Keep that ordering at
    // the native action boundary rather than deriving history from the raw
    // four-frame input snapshots after the fact.
    player.publish_action_profile(
        result.action_profile,
        static_cast<std::uint32_t>(player.frame_counter()));
    if (hooks.on_action_history) {
        hooks.on_action_history(player, result.action_profile);
    }
    if (hooks.action_sequence_source.has_value()
        && hooks.action_sequence_source->tricks != nullptr) {
        std::span<const std::uint8_t> sequence_table =
            hooks.action_sequence_source->sequence_table;
        if (sequence_table.empty()
            && hooks.action_sequence_source->use_source_sequence_fallback) {
            const auto source_table =
                hooks.action_sequence_source->tricks->source_sequence_table();
            if (source_table.has_value()) {
                sequence_table = *source_table;
            }
        }
        if (!sequence_table.empty()) {
            ActionSequenceMatcherInput matcher = hooks.action_sequence_source->matcher;
            if (matcher.selected_action == 0) {
                matcher.selected_action = result.action_profile.selected_action;
            }
            if (matcher.now == 0) {
                matcher.now = static_cast<std::uint32_t>(player.frame_counter());
            }
            result.action_sequence = player.run_action_sequences(
                *hooks.action_sequence_source->tricks,
                sequence_table,
                matcher);
            if (hooks.on_action_sequence) {
                hooks.on_action_sequence(player, *result.action_sequence);
            }
        }
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
    if (hooks.ground_physics_input) {
        const std::optional<GroundPhysicsInput> ground_physics_input =
            hooks.ground_physics_input(player, input);
        if (ground_physics_input.has_value()) {
            GroundPhysicsInput resolved = *ground_physics_input;
            resolved.frame_scale_q8 = frame_scale_q8;
            result.ground_physics = player.update_ground_physics(resolved);
        }
    }

    bool collision_transient_requested = false;
    bool collision_transient_exit_requested = false;
    bool ground_leave_air_requested = false;
    bool state_two_entered_from_recovery = false;
    FixedPosition collision_transient_exit_normal{};
    FixedPosition collision_transient_basis{};
    bool collision_transient_basis_valid = false;
    const PhysicsDispatchHooks dispatch_hooks{
        [&result, &hooks, &input, &collision_transient_requested,
         &collision_transient_exit_requested,
         &ground_leave_air_requested,
         &state_two_entered_from_recovery,
         &collision_transient_exit_normal, &collision_transient_basis,
         &collision_transient_basis_valid,
         frame_scale_q8](
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
                hooks.air_action_control_input) {
                const std::optional<AirActionControlConfig> control_input =
                    hooks.air_action_control_input(current_player, input);
                if (control_input.has_value()) {
                    result.air_action_control =
                        current_player.apply_air_action_control(
                            input,
                            *control_input);
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
            const FixedPosition ground_old_up = current_player.retail_basis().at_310c;
            const FixedPosition start = current_player.position();
            FixedPosition desired = current_player.integrated_position(
                frame_scale_q8);
            if (stage == PhysicsDispatchStage::InAir_97f40
                && current_player.physics_state() != 2
                && hooks.air_upright_input) {
                const std::optional<FixedPosition> global_up =
                    hooks.air_upright_input(current_player, input);
                if (global_up.has_value()) {
                    current_player.apply_upright_correction(*global_up);
                }
            }
            PositionCollisionProbe probe = hooks.collision_probe;
            std::optional<PositionCollisionHit> queried_hit;
            std::optional<PositionCollisionHit> movement_collision;
            bool ground_surface_target_changed = false;
            bool use_ground_response_basis_tail = true;
            if (hooks.collision_query) {
                // The support query and FUN_00496955's ordinary movement
                // query are separate retail operations. Preserve the latter
                // hit because its normal/contact feed the later ground
                // recovery path; a recovery candidate is only selected for
                // a miss.
                movement_collision = hooks.collision_query(start, desired);
                if (movement_collision.has_value()) {
                    queried_hit = movement_collision;
                }
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
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && (current_player.physics_state() == 0
                    || current_player.physics_state() == 2)
                && hooks.ground_surface_response_input) {
                const std::optional<GroundSurfaceResponseInput> response_input =
                    hooks.ground_surface_response_input(current_player, input);
                if (response_input.has_value()) {
                    static_cast<void>(current_player.apply_ground_surface_response(
                        *response_input,
                        frame_scale_q8,
                        current_player.physics_state() != 2));
                    if (current_player.physics_state() == 0) {
                        collision_transient_basis =
                            current_player.retail_basis().at_30f4;
                        collision_transient_basis_valid = true;
                    }
                }
            }
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && (current_player.physics_state() == 0
                    || current_player.physics_state() == 2)
                && hooks.apply_ground_surface_recovery
                && hooks.collision_query) {
                // FUN_00490730 builds the support probe from the live
                // position and sweeps eight thousand world units upward.
                // This query supplies the surface normal for recovery; the
                // movement query below remains responsible for position.
                const FixedPosition surface_start = current_player.position();
                FixedPosition surface_end = surface_start;
                surface_end[1] += 0x1f40000;
                result.ground_surface_hit = hooks.collision_query(
                    surface_start,
                    surface_end);
                if (result.ground_surface_hit.has_value()) {
                    if (!movement_collision.has_value()) {
                        const auto offset = [](const FixedPosition& position,
                                               const FixedPosition& axis,
                                               std::int32_t scale) {
                            FixedPosition result = position;
                            for (std::size_t index = 0;
                                 index < result.size();
                                 ++index) {
                                result[index] = static_cast<std::int32_t>(
                                    static_cast<std::int64_t>(result[index])
                                    + static_cast<std::int64_t>(axis[index])
                                        * scale);
                            }
                            return result;
                        };
                        // When the ordinary movement query misses, retail
                        // probes from the integrated movement candidate along
                        // the prior up axis and commits the recovered contact
                        // before the shared position fallback runs.
                        const FixedPosition recovery_axis =
                            current_player.physics_state() == 2
                            ? current_player.air_motion()
                            : ground_old_up;
                        const FixedPosition recovery_start = offset(
                            desired,
                            recovery_axis,
                            70);
                        const FixedPosition recovery_end = offset(
                            desired,
                            recovery_axis,
                            -186);
                        const std::optional<PositionCollisionHit> recovery_hit =
                            hooks.collision_query(
                                recovery_start,
                                recovery_end);
                        if (recovery_hit.has_value()) {
                            if (current_player.physics_state() == 0
                                && recovery_hit->surface_bit_6) {
                                // FUN_004972df enters raw state 2 after the
                                // recovery sweep identifies the special
                                // surface. Capture the pre-transition basis
                                // before the recovery orientation update;
                                // FUN_004900b0 consumes that exact vector.
                                const FixedPosition transition_basis =
                                    current_player.retail_basis().at_30f4;
                                current_player.request_physics_state_from_basis(
                                    2,
                                    0x1ac9,
                                    transition_basis);
                                state_two_entered_from_recovery = true;
                            }
                            if (current_player.physics_state() == 2
                                && recovery_hit->normal[1] < 0) {
                                collision_transient_exit_requested = true;
                                collision_transient_exit_normal =
                                    recovery_hit->normal;
                            }
                            if (!queried_hit.has_value()) {
                                queried_hit = recovery_hit;
                            }
                            ground_surface_target_changed =
                                current_player.update_ground_surface_recovery(
                                    recovery_hit->normal,
                                    hooks.ground_surface_recovery_delta_q11);
                            // State 0 seeds the transient correction with the
                            // recovered 0x1964 vertical vector, then removes
                            // its surface-normal component through
                            // FUN_00490610.
                            FixedPosition surface_correction{0, 0x1964, 0};
                            static_cast<void>(remove_normal_component(
                                surface_correction,
                                recovery_hit->normal));
                            if (current_player.physics_state() != 2) {
                                current_player.set_ground_surface_response_surface(
                                    surface_correction,
                                    recovery_hit->normal);
                            }
                            if (current_player.physics_state() != 2) {
                                current_player.add_motion_correction(
                                    surface_correction);
                            } else if (state_two_entered_from_recovery) {
                                // The state-two recovery tail receives the
                                // same 0x1964 vertical write twice before the
                                // outer +58 -> +4c handoff.
                                current_player.add_motion_correction(
                                    surface_correction);
                                current_player.add_motion_correction(
                                    surface_correction);
                            }
                            static_cast<void>(current_player.project_collision_velocity(
                                recovery_hit->normal));
                            const FixedPosition recovery_candidate = offset(
                                recovery_hit->position,
                                recovery_axis,
                                30);
                            const PositionCollisionProbe recovery_probe =
                                [&hooks, &desired](
                                    const FixedPosition& candidate) {
                                    return hooks.collision_query(
                                        desired,
                                    candidate).has_value();
                                };
                            const PositionCommitResult recovery_commit =
                                PositionCommitter::commit(
                                    desired,
                                    recovery_candidate,
                                    recovery_probe,
                                    hooks.bypass_collision);
                            // FUN_00496550 lets FUN_00496060 try this recovery
                            // candidate even when the supporting normal is
                            // unchanged.  The post-call displacement check
                            // then restores the ordinary integrated position
                            // for a short correction (FUN_004f5f90 < 0x1000)
                            // but keeps a substantial recovery move.
                            const FixedPosition recovery_displacement{
                                recovery_commit.position[0] - desired[0],
                                recovery_commit.position[1] - desired[1],
                                recovery_commit.position[2] - desired[2],
                            };
                            if (fixed_dot_q12(
                                    recovery_displacement,
                                    recovery_displacement) >= 0x1000) {
                                desired = recovery_commit.position;
                            }
                        } else if (current_player.physics_state() == 0
                                   && result.ground_surface_hit.has_value()
                                   && result.ground_surface_hit->normal[1] < 0) {
                            // FUN_00495719 is the grounded leave-air path
                            // reached when the support sweep still sees a
                            // surface but both movement/recovery candidates
                            // miss. Keep the request at the same boundary as
                            // retail FUN_004956f0, before the state-specific
                            // response tail below.
                            ground_leave_air_requested = true;
                        }
                    }

                }
            }
            if (movement_collision.has_value()
                && stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0
                && hooks.collision_query) {
                const auto offset = [](const FixedPosition& position,
                                       const FixedPosition& axis,
                                       std::int32_t scale) {
                    FixedPosition result = position;
                    for (std::size_t index = 0;
                         index < result.size();
                         ++index) {
                        result[index] = static_cast<std::int32_t>(
                            static_cast<std::int64_t>(result[index])
                            + static_cast<std::int64_t>(axis[index]) * scale);
                    }
                    return result;
                };

                // FUN_0048ea80 publishes the surface-bit-40 result used by
                // the movement-hit branch below.  Warehouse's ordinary
                // ground result takes the 0x00496a82 path when that bit is
                // clear; a surface-bit-40 result then takes the direct
                // LAB_00496ebe path unless its face-bit-80 result is set.
                // Keep this raw flag ordering instead of folding it into a
                // geometry/material name: it determines whether retail
                // commits the ordinary candidate and rebuilds the recovery
                // basis before the secondary sweep.
                const bool use_movement_recovery_base =
                    !movement_collision->surface_bit_6;
                use_ground_response_basis_tail = use_movement_recovery_base;
                // FUN_004972da requests raw state 2 after the direct
                // surface-bit-40 ground path. Defer the write until the
                // complete state-0 collision helper has finished so the
                // state-specific basis tail still runs in this frame.
                if (movement_collision->surface_bit_6) {
                    collision_transient_requested = true;
                }
                const bool ground_facing_movement =
                    movement_collision->normal[1] < 0;
                if (use_movement_recovery_base && ground_facing_movement) {
                    const bool recovery_target_changed =
                        current_player.update_ground_surface_recovery(
                            movement_collision->normal,
                            hooks.ground_surface_recovery_delta_q11);
                    ground_surface_target_changed =
                        ground_surface_target_changed || recovery_target_changed;
                }
                const FixedPosition recovery_direction =
                    current_player.air_motion();
                // The ordinary movement-hit recovery sweep is anchored at the
                // offset contact for ground-facing movement. The direct
                // surface-bit path uses the integrated point, while a
                // horizontal wall uses the live position.
                FixedPosition movement_candidate = movement_collision->position;
                for (std::size_t index = 0; index < movement_candidate.size(); ++index) {
                    movement_candidate[index] = static_cast<std::int32_t>(
                        static_cast<std::int64_t>(movement_candidate[index])
                        + static_cast<std::int64_t>(
                            movement_collision->normal[index]) * 0x1e);
                }
                const FixedPosition recovery_base = !use_movement_recovery_base
                    ? desired
                    : ground_facing_movement ? movement_candidate : start;
                const FixedPosition recovery_start = offset(
                    recovery_base,
                    recovery_direction,
                    70);
                const FixedPosition recovery_end = offset(
                    recovery_base,
                    recovery_direction,
                    -186);
                const std::optional<PositionCollisionHit> recovery_hit =
                    hooks.collision_query(recovery_start, recovery_end);
                if (recovery_hit.has_value()) {
                    const bool recovery_target_changed =
                        current_player.update_ground_surface_recovery(
                            recovery_hit->normal,
                            hooks.ground_surface_recovery_delta_q11);
                    ground_surface_target_changed =
                        ground_surface_target_changed || recovery_target_changed;
                    FixedPosition surface_correction{0, 0x1964, 0};
                    static_cast<void>(remove_normal_component(
                        surface_correction,
                        recovery_hit->normal));
                    // State 2 keeps the transition-owned +3118 vector from
                    // FUN_004900b0.  The shared recovery path still produces
                    // the per-frame +58 motion correction, but it must not
                    // replace the vector consumed by FUN_0049c060 on the
                    // following state-two frame.
                    if (current_player.physics_state() != 2) {
                        current_player.set_ground_surface_response_surface(
                            surface_correction,
                            recovery_hit->normal);
                    }
                    // FUN_00490610 adds the projected surface correction to
                    // the transient correction already produced before the
                    // movement query. The grounded tail then consumes that
                    // complete accumulated +0x58 value.
                    current_player.add_motion_correction(surface_correction);
                    if (!use_movement_recovery_base) {
                        // The direct surface-bit path reaches the shared
                        // 0x004975c7 tail with the static 0x1964 correction
                        // applied a second time. It also skips the generic
                        // response-basis subtraction below.
                        current_player.add_motion_correction(surface_correction);
                    }
                    static_cast<void>(current_player.project_collision_velocity(
                        recovery_hit->normal));
                    const FixedPosition recovery_candidate = offset(
                        recovery_hit->position,
                        recovery_direction,
                        30);
                    const PositionCollisionProbe recovery_probe =
                        [&hooks, &recovery_base](
                            const FixedPosition& candidate) {
                            return hooks.collision_query(
                                recovery_base,
                                candidate).has_value();
                        };
                    const PositionCommitResult recovery_commit =
                        PositionCommitter::commit(
                            recovery_base,
                            recovery_candidate,
                            recovery_probe,
                            hooks.bypass_collision);
                            if (current_player.physics_state() == 2
                                || ground_surface_target_changed) {
                                desired = recovery_commit.position;
                            }
                } else {
                    FixedPosition surface_correction{0, 0x1964, 0};
                    static_cast<void>(remove_normal_component(
                        surface_correction,
                        movement_collision->normal));
                    if (current_player.physics_state() != 2) {
                        current_player.set_ground_surface_response_surface(
                            surface_correction,
                            movement_collision->normal);
                    }
                }
                // A non-ground ordinary hit is handled by the wall branch of
                // FUN_00496955/00496a82. It retains the live position for the
                // outer commit; the integrated point is only the transient
                // candidate used by the branch's collision tests.
                if (use_movement_recovery_base && !ground_facing_movement) {
                    desired = start;
                }
            }
            result.position_commit = current_player.commit_position(
                desired,
                probe,
                hooks.bypass_collision);
            // The grounded steering velocity phase belongs after the
            // collision routine has produced its response and before the
            // outer correction handoff. It is an explicit state phase, not a
            // part of the position candidate selection.
            if (ground_leave_air_requested
                && stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0) {
                // FUN_004956f0's complementary state-1 handoff retains the
                // raw 0x1964 vertical correction in +0x58 before the outer
                // response integration. It is not the projected support
                // correction used by a recovered collision candidate.
                current_player.add_motion_correction(
                    FixedPosition{0, 0x1964, 0});
                current_player.request_physics_state(1, 0x160b);
            }
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0
                && !result.ground_surface_hit.has_value()) {
                current_player.apply_ground_turn_velocity_phase();
            }
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
                && hooks.remove_hit_normal_component
                && !result.ground_surface_hit.has_value()) {
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
            bool accepted_air_contact = false;
            if (result.collision_hit.has_value()
                && stage == PhysicsDispatchStage::InAir_97f40) {
                if (hooks.on_air_contact) {
                    accepted_air_contact = hooks.on_air_contact(
                        current_player,
                        *result.collision_hit,
                        result.position_commit);
                } else if (hooks.standard_air_contact_input) {
                    const std::optional<StandardAirContactInput> contact_input =
                        hooks.standard_air_contact_input(
                            current_player, input, *result.collision_hit);
                    accepted_air_contact = contact_input.has_value()
                        && accepts_standard_air_contact(
                            *result.collision_hit,
                            current_player.physics_state(),
                            input.action(kJumpActionBit).held,
                            input.action(kJumpActionBit).inactive_frames,
                            current_player.frame_counter(),
                            *contact_input);
                }
            }
            if (accepted_air_contact) {
                // FUN_00497f40's accepted-contact branch commits the
                // interpolated collision point produced by the air query
                // before requesting ground state. The shared candidate
                // committer may otherwise report its current-position
                // fallback when every axis candidate still intersects.
                FixedPosition contact_position =
                    result.collision_hit.has_value()
                    ? result.collision_hit->position
                    : result.position_commit.position;
                if (result.collision_hit.has_value()) {
                    // The accepted-contact path does not commit the raw
                    // intersection point.  Retail FUN_00498a66 builds the
                    // landing candidate as contact + normal * 0x1e before
                    // calling FUN_00496060.  The normal is a signed Q12
                    // short, so this is a direct fixed-point offset (not a
                    // Q12 rescale); Warehouse's downward floor therefore
                    // moves the candidate by -0x1e000 on Y.
                    for (std::size_t axis = 0; axis < contact_position.size();
                         ++axis) {
                        contact_position[axis] = static_cast<std::int32_t>(
                            static_cast<std::int64_t>(contact_position[axis])
                            + static_cast<std::int64_t>(
                                result.collision_hit->normal[axis])
                                * 0x1e);
                    }
                }
                result.landed = current_player.accept_air_contact(
                    contact_position,
                    result.collision_hit.has_value()
                    ? result.collision_hit->normal
                    : current_player.air_motion());
                if (result.landed && hooks.landing_animation_request) {
                    result.landing_animation_request =
                        hooks.landing_animation_request(
                            current_player,
                            *result.collision_hit);
                }
            }
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0
                && hooks.apply_ground_basis_correction
                && use_ground_response_basis_tail) {
                // This tail runs after the candidate position has been
                // integrated/committed. Its response projection is visible
                // to the outer +58 -> +4c handoff in the same frame.
                current_player.prepare_ground_basis_correction(
                    hooks.apply_ground_basis_forward_term,
                    frame_scale_q8,
                    8,
                    true);
            }
            result.position_integrated = true;
        },
    };
    result.dispatch = PhysicsDispatcher::dispatch(player, dispatch_hooks);

    if (collision_transient_requested) {
        if (collision_transient_basis_valid) {
            player.request_physics_state_from_basis(
                2,
                0x1ac9,
                collision_transient_basis);
        } else {
            player.request_physics_state(2, 0x1ac9);
        }
    }

    if (collision_transient_exit_requested) {
        // FUN_00497479 exits the collision-transient state after the
        // recovery candidate has been selected. The remainder of that
        // helper then runs the ordinary state-0 surface handoff in the same
        // frame: seed +0x58 with the projected 0x1964 vector, remove the
        // response's lateral basis component, and apply the forward term.
        player.request_physics_state(
            0,
            0x1b19);
        FixedPosition surface_correction{0, 0x1964, 0};
        static_cast<void>(remove_normal_component(
            surface_correction,
            collision_transient_exit_normal));
        player.set_ground_surface_response_surface(
            surface_correction,
            collision_transient_exit_normal);
        player.add_motion_correction(surface_correction);
        if (hooks.apply_ground_basis_correction) {
            player.prepare_ground_basis_correction(
                hooks.apply_ground_basis_forward_term,
                frame_scale_q8,
                8,
                true);
        }
    }

    // The state-2 +0x2dac value is published after the collision candidate
    // has been selected. It participates in the outer +58 -> +4c handoff,
    // but not in this frame's position integration.
    if ((result.physics_state_before == 2
         || state_two_entered_from_recovery)
        && player.physics_state() == 2
        && hooks.state_two_motion_input) {
        const std::optional<AirSpeedConfig> state_two_motion_input =
            hooks.state_two_motion_input(player, input);
        if (state_two_motion_input.has_value()) {
            player.set_motion_correction(FixedPosition{
                0,
                compute_air_speed_scalar(*state_two_motion_input),
                0,
            });
        }
    }

    // The common air handler's 0x004992f0 fallthrough runs only when the
    // in-air path did not accept a landing. It publishes +0x2dac into the
    // temporary correction after the position step, so the value affects the
    // next frame's displacement and the outer response handoff below.
    bool dispatched_in_air = false;
    for (std::size_t index = 0;
         index < result.dispatch.stage_count;
         ++index) {
        dispatched_in_air = dispatched_in_air
            || result.dispatch.stages[index] == PhysicsDispatchStage::InAir_97f40;
    }
    if (dispatched_in_air
        && player.physics_state() != 0
        && player.physics_state() != 7
        && hooks.air_gravity_acceleration_input) {
        const std::optional<std::int32_t> acceleration =
            hooks.air_gravity_acceleration_input(player, input);
        if (acceleration.has_value()) {
            player.apply_air_gravity_acceleration(*acceleration);
            result.air_gravity_acceleration = *acceleration;
        }
    }

    if (hooks.integrate_motion_correction) {
        // This is the outer-frame +58 -> +4c handoff after dispatch.
        player.integrate_motion_correction(frame_scale_q8);
        result.motion_correction_integrated = true;
    }
    if (hooks.motion_correction_input) {
        const std::optional<FixedPosition> motion_correction =
            hooks.motion_correction_input(player, result.dispatch);
        if (motion_correction.has_value()) {
            // Preserve the completed +0x58 value without folding it into
            // response a second time.
            player.set_motion_correction(*motion_correction);
        }
    }
    if (hooks.response_correction_input) {
        const std::optional<FixedPosition> response_correction =
            hooks.response_correction_input(player, result.dispatch);
        if (response_correction.has_value()) {
            player.add_collision_response(*response_correction);
        }
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
    result.physics_state_after = player.physics_state();
    result.state_request = player.last_state_request();
    return result;
}

} // namespace opentony::runtime
