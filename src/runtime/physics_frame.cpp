#include "physics_frame.hpp"

#include "collision_recovery.hpp"
#include "tricks_bin.hpp"

namespace opentony::runtime {
namespace {

bool has_position_integrator(PhysicsDispatchStage stage) noexcept {
    return stage == PhysicsDispatchStage::GroundCollision_96550
        || stage == PhysicsDispatchStage::InAir_97f40;
}

// FUN_00462a20 stores the segment length in world units and the collision
// parameter as Q14.  The native query retains the latter; recover the former
// from the fixed-point endpoint delta using the same Q12 magnitude helper.
// The endpoint delta is Q12 world units, so its magnitude is divided by 64
// after the helper's Q12 normalization (the retail query's frame lengths are
// 256 for the exact unit axis and 255 for the short normalized Warehouse
// axis).
[[nodiscard]] std::int32_t retail_segment_length(
    const FixedPosition& start,
    const FixedPosition& end) noexcept {
    const FixedPosition delta{
        static_cast<std::int32_t>(
            static_cast<std::int64_t>(end[0]) - start[0]),
        static_cast<std::int32_t>(
            static_cast<std::int64_t>(end[1]) - start[1]),
        static_cast<std::int32_t>(
            static_cast<std::int64_t>(end[2]) - start[2]),
    };
    return retail_vector_magnitude_q12(delta) / 64;
}

[[nodiscard]] std::int32_t retail_hit_distance(
    const FixedPosition& start,
    const FixedPosition& end,
    std::uint32_t parameter_q14) noexcept {
    const std::int64_t distance =
        static_cast<std::int64_t>(retail_segment_length(start, end))
        * static_cast<std::int64_t>(parameter_q14)
        / 0x4000;
    return distance > std::numeric_limits<std::int32_t>::max()
        ? std::numeric_limits<std::int32_t>::max()
        : static_cast<std::int32_t>(distance);
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
    player.update_collision_recovery_window(input);
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
    // The state-1/2 portion of FUN_00493370 advances +0x3144 before the
    // dispatcher enters FUN_00497f40. Keep this producer before animation,
    // correction reset, and the in-air collision handoff.
    player.update_in_air_orientation_accumulator(input, frame_scale_q8);
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
    bool collision_response_projection_pending = false;
    bool ground_leave_air_requested = false;
    std::int32_t ground_leave_air_reason = 0x1ab6;
    bool state_two_entered_from_recovery = false;
    bool air_normal_recovery_handled = false;
    FixedPosition collision_transient_exit_normal{};
    FixedPosition collision_transient_basis{};
    bool collision_transient_basis_valid = false;
    const PhysicsDispatchHooks dispatch_hooks{
        [&result, &hooks, &input, &collision_transient_requested,
         &collision_transient_exit_requested,
         &collision_response_projection_pending,
         &ground_leave_air_requested,
         &ground_leave_air_reason,
         &state_two_entered_from_recovery,
         &air_normal_recovery_handled,
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
                if (stage == PhysicsDispatchStage::GroundFinal_9d9c0
                    && hooks.collision_query
                    && (current_player.physics_state() == 0
                        || current_player.physics_state() == 7)) {
                    // FUN_0049d9c0 performs the final grounded bounce probes
                    // after FUN_00496550/FUN_00495cc0.  The probe starts at
                    // the live position offset along +310c, then searches
                    // along +/- +3100.  When the returned normal is
                    // orthogonal to +310c, FUN_004957c0 makes a second
                    // normal-offset query and, on a miss, commits the
                    // directional endpoint directly.  This is the source of
                    // the Warehouse wall's position -0xa000 correction at
                    // frame 114; it is not part of the main movement query.
                    const auto bounce_probe = [&](
                        std::int32_t direction,
                        std::int32_t distance) {
                        // The retail helper queries from the candidate published
                        // by GroundCollision, but its successful normal endpoint
                        // is based on the frame-start position restored by that
                        // collision path.  Keep those two roles separate.
                        const RetailBasis& basis = current_player.retail_basis();
                        const GroundedBounceProbeGeometry geometry =
                            make_grounded_bounce_probe_geometry(
                                current_player.position(),
                                basis.at_3100,
                                basis.at_310c,
                                direction,
                                distance);
                        const std::optional<PositionCollisionHit> hit =
                            hooks.collision_query(
                                geometry.first_start,
                                geometry.first_end);
                        if (!hit.has_value()) {
                            return false;
                        }
                        const std::int32_t normal_alignment = fixed_dot_q12(
                            hit->normal,
                            basis.at_310c);
                        if (!accepts_grounded_bounce_normal(normal_alignment)) {
                            return false;
                        }

                        // FUN_004957c0 routes the first accepted probe
                        // through FUN_0049bad0 before its normal-offset
                        // verification query.  That helper publishes the
                        // inward response and the collision-aligned basis;
                        // keep the caller's 200-word heading gate and the
                        // helper's zero->0x19 fallback in this same phase.
                        const FixedPosition local_normal{
                            -hit->normal[0],
                            -hit->normal[1],
                            -hit->normal[2],
                        };
                        const std::int32_t forward_dot = fixed_dot_q12(
                            basis.at_30f4,
                            local_normal);
                        const std::int32_t yaw_offset = forward_dot > 0xb50
                            ? kGroundedBounceAlternateHeading
                            : kGroundedBounceDefaultHeading;
                        static_cast<void>(
                            current_player.apply_collision_response(
                                hit->normal,
                                kGroundedBounceResponseBiasQ12));
                        static_cast<void>(
                            current_player.apply_collision_orientation(
                                hit->normal,
                                yaw_offset));

                        // The second query in FUN_004957c0 begins one raw
                        // normal unit into the hit and ends at the signed
                        // normal * distance endpoint.  A clear second probe
                        // writes that endpoint to the live position.
                        const FixedPosition normal_start =
                            make_grounded_bounce_verification_start(
                                current_player.previous_position(),
                                hit->normal);
                        const FixedPosition normal_end =
                            make_grounded_bounce_verification_end(
                                current_player.previous_position(),
                                hit->normal,
                                direction,
                                distance);
                        if (hooks.collision_query(normal_start, normal_end)
                                .has_value()) {
                            return false;
                        }
                        current_player.set_position(normal_end);
                        result.position_commit = PositionCommitResult{
                            normal_end,
                            false,
                            false,
                            0,
                            static_cast<std::uint8_t>(
                                PositionCommitCandidate::Direct),
                        };
                        return true;
                    };

                    const FixedPosition saved_position = current_player.position();
                    const std::int32_t bounce_distance =
                        grounded_bounce_probe_distance(
                            current_player.control_blocked());
                    if (!bounce_probe(-1, bounce_distance)) {
                        if (!bounce_probe(1, bounce_distance)) {
                            return;
                        }
                        if (!bounce_probe(-1, bounce_distance)) {
                            return;
                        }
                        current_player.set_position(saved_position);
                        result.position_commit = PositionCommitResult{
                            saved_position,
                            false,
                            false,
                            0,
                            static_cast<std::uint8_t>(
                                PositionCommitCandidate::Direct),
                        };
                    } else {
                        if (!bounce_probe(1, bounce_distance)) {
                            return;
                        }
                        current_player.set_position(saved_position);
                        result.position_commit = PositionCommitResult{
                            saved_position,
                            false,
                            false,
                            0,
                            static_cast<std::uint8_t>(
                                PositionCommitCandidate::Direct),
                        };
                    }
                }
                return;
            }

            // Keep the old live position as the current point for the shared
            // FUN_00496060 axis fallback while testing the integrated point as
            // the desired position.
            const FixedPosition start = current_player.position();
            FixedPosition desired = current_player.integrated_position(
                frame_scale_q8);
            if (stage == PhysicsDispatchStage::GroundCollision_96550) {
                // FUN_00496550 performs the fixed-step velocity/correction
                // add into the live position before it builds either
                // collision line. Keep `start` as the pre-step point for
                // the movement query and fallback probes, while the player
                // object itself now exposes the integrated point just as
                // retail +0x08 does at 0x00496f2b.
                current_player.set_position(desired);
            }
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && (current_player.physics_state() == 0
                    || current_player.physics_state() == 2)
                && hooks.ground_surface_response_input) {
                // FUN_00496360 is called by FUN_00496550 before the ordinary
                // movement line is built and queried. Its response/service
                // writes therefore belong after integration, but before the
                // first collision query.
                const std::optional<GroundSurfaceResponseInput> response_input =
                    hooks.ground_surface_response_input(current_player, input);
                if (response_input.has_value()) {
                    static_cast<void>(current_player.apply_ground_surface_response(
                        *response_input,
                        frame_scale_q8,
                        current_player.physics_state() == 0));
                    if (current_player.physics_state() == 0) {
                        collision_transient_basis =
                            current_player.retail_basis().at_30f4;
                        collision_transient_basis_valid = true;
                    }
                }
            }
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0) {
                // FUN_00496550 calls FUN_00496360 after integrating the live
                // point and before it builds the ordinary movement query.
                // FUN_00496360 always reaches FUN_0049b500, including when
                // its optional FUN_0049c060 service path has no draw.  The
                // response phase therefore belongs at this boundary rather
                // than after collision selection.
                current_player.apply_ground_turn_velocity_phase();
            }
            if (stage == PhysicsDispatchStage::InAir_97f40
                && current_player.physics_state() != 2
                && hooks.air_orientation_pivot_input) {
                const std::optional<std::int32_t> angle =
                    hooks.air_orientation_pivot_input(
                        current_player,
                        input,
                        frame_scale_q8);
                if (angle.has_value()) {
                    const FixedPosition pivot_delta =
                        current_player.apply_in_air_orientation_pivot(*angle);
                    for (std::size_t index = 0; index < desired.size(); ++index) {
                        desired[index] += pivot_delta[index];
                    }
                }
            }
            if (stage == PhysicsDispatchStage::InAir_97f40
                && current_player.physics_state() == 1
                && hooks.air_orientation_turn_input) {
                const std::optional<AirOrientationTurnConfig> turn_input =
                    hooks.air_orientation_turn_input(
                        current_player,
                        input,
                        frame_scale_q8);
                if (turn_input.has_value()) {
                    // The producer writes the tangent basis before the
                    // common upright helper and before the air query.
                    static_cast<void>(
                        current_player.apply_in_air_orientation_turn(
                            *turn_input));
                }
            }
            PositionCollisionProbe probe = hooks.collision_probe;
            std::optional<PositionCollisionHit> queried_hit;
            std::optional<PositionCollisionHit> movement_collision;
            bool use_ground_response_basis_tail = true;
            bool movement_recovery_candidate_selected = false;
            const auto ground_collision_query =
                [&hooks, &current_player](
                    const FixedPosition& query_start,
                    const FixedPosition& query_end) {
                    const std::optional<PositionCollisionHit> hit =
                        hooks.collision_query(query_start, query_end);
                    if (hit.has_value()) {
                        // FUN_0048ea80 leaves the decoded material class in
                        // the player object. A miss does not clear it, so
                        // retain the previous value across helper probes and
                        // frames just as retail does.
                        current_player.set_ground_surface_class(
                            hit->raw_type_bits_9_12);
                    }
                    return hit;
                };
            if (hooks.collision_query) {
                // The support query and FUN_00496955's ordinary movement
                // query are separate retail operations. Preserve the latter
                // hit because its normal/contact feed the later ground
                // recovery path; a recovery candidate is only selected for
                // a miss.
                movement_collision = ground_collision_query(start, desired);
                if (movement_collision.has_value()) {
                    queried_hit = movement_collision;
                }
                probe = [&ground_collision_query, &start, &queried_hit](
                    const FixedPosition& candidate) {
                    const std::optional<PositionCollisionHit> hit =
                        ground_collision_query(start, candidate);
                    if (hit.has_value() && !queried_hit.has_value()) {
                        queried_hit = hit;
                    }
                    return hit.has_value();
                };
            }
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && (current_player.physics_state() == 0
                    || current_player.physics_state() == 2)
                && hooks.apply_ground_surface_recovery
                && hooks.collision_query
                // FUN_00496550 reaches its support sweep after the ordinary
                // movement-hit branch.  A miss has no movement-hit branch,
                // so it is the only case handled by this fallback block.
                && !movement_collision.has_value()) {
                // The second line in FUN_00496550 is built from the live
                // point and the published +310c axis at both ends. It is a
                // 70-unit forward / 256-unit reverse support sweep; the
                // +3100 tangent axis belongs to the separate bounce helper.
                const RetailBasis& recovery_basis =
                    current_player.retail_basis();
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
                const FixedPosition recovery_axis = recovery_basis.at_310c;
                const FixedPosition recovery_start = offset(
                    current_player.position(),
                    recovery_axis,
                    0x46);
                const FixedPosition recovery_end = offset(
                    recovery_start,
                    recovery_axis,
                    -0x100);
                result.ground_surface_hit = ground_collision_query(
                    recovery_start,
                    recovery_end);
                if (result.ground_surface_hit.has_value()) {
                        // When the ordinary movement query misses, retail
                        // probes from the live point along the published up
                        // axis and commits the recovered contact before the
                        // shared position fallback runs.
                        const std::optional<PositionCollisionHit> recovery_hit =
                            result.ground_surface_hit;
                        if (recovery_hit.has_value()) {
                            collision_transient_exit_normal = recovery_hit->normal;
                            // Retail takes the long-recovery exit before the
                            // normal/recovery-basis update.  FUN_0049704f's
                            // state-0 threshold is +0x2da4 + 0x74; Warehouse
                            // leaves +0x2da4 at zero.  This is the causal
                            // state-1 handoff observed at frames 36 and 139,
                            // and is intentionally based on the query
                            // distance rather than a recording frame index.
                            const bool recovery_leave_air =
                                current_player.physics_state() == 0
                                && retail_hit_distance(
                                       recovery_start,
                                       recovery_end,
                                    recovery_hit->hit_parameter_q14)
                                    > 0x74;
                            // FUN_004956f0 is also the complementary exit
                            // when the grounded recovery hit is no longer a
                            // ground-facing surface. In this path the
                            // movement query missed, the support sweep still
                            // found the floor, and the secondary recovery
                            // normal is horizontal; retail leaves the
                            // integrated position and response untouched.
                            const bool non_ground_recovery_leave_air =
                                current_player.physics_state() == 0
                                && !recovery_hit->surface_bit_6
                                && recovery_hit->normal[1] >= 0;
                            // A downward support hit is the other entry to
                            // FUN_004956f0. It takes precedence over the
                            // short secondary-recovery projection: the
                            // retail frame leaves the response untouched and
                            // adds the raw +0x1964 handoff correction below.
                            const bool support_leave_air =
                                current_player.physics_state() == 0
                                && support_hit_requests_ground_exit(
                                    *recovery_hit,
                                    current_player.ground_surface_recovery_target(),
                                    current_player.retail_basis().at_30f4,
                                    current_player.collision_response());
                            const bool leave_air =
                                recovery_leave_air
                                || non_ground_recovery_leave_air
                                || support_leave_air;
                            // FUN_0049704f branches to FUN_004956f0 when the
                            // recovery window is old.  The special material
                            // bit on the secondary contact does not itself
                            // select the transient state-2 path: the retail
                            // state-2 request at 0x004972da is the recent
                            // recovery-window branch below.
                            const bool recovery_window_recent =
                                current_player.collision_recovery_frame() == 0
                                || current_player.frame_counter()
                                       - current_player.collision_recovery_frame()
                                    < 5;
                            const bool ordinary_recovery_state_one =
                                current_player.physics_state() == 0
                                && recovery_hit->surface_bit_6
                                && !leave_air
                                && !recovery_window_recent;
                            const bool transient_exit =
                                current_player.physics_state() == 2
                                && recovery_hit->normal[1] < 0;
                            if (ordinary_recovery_state_one) {
                                // This is the 0x004971ee path: damp the
                                // response along the selected recovery normal,
                                // halve Y, then request state 1 with reason
                                // 0x1ab6. It jumps to the common tail without
                                // publishing the state-2 surface response.
                                current_player.apply_ground_leave_air_response(
                                    recovery_hit->normal);
                                current_player.request_physics_state(
                                    1,
                                    0x1ab6);
                                queried_hit = recovery_hit;
                                // FUN_00496060 accepts the recovered contact
                                // candidate on this branch. The candidate is
                                // the contact plus the live air-motion axis;
                                // its collision fallback is not selected in
                                // the Warehouse trace.
                                desired = offset(
                                    recovery_hit->position,
                                    current_player.air_motion(),
                                    30);
                                // The state-1 common tail adds the raw
                                // recovery vector to +0x58 after the handoff;
                                // it is separate from the state-0/state-2
                                // surface-response publication above.
                                current_player.add_motion_correction(
                                    FixedPosition{0, 0x1964, 0});
                            } else if ((current_player.physics_state() == 0
                                 && !leave_air)
                                || (current_player.physics_state() == 2
                                    && !transient_exit)) {
                                // The ordinary grounded recovery path reaches
                                // the velocity projection before its basis
                                // tail; the long-recovery exit takes the
                                // complementary state-1 path instead.
                                static_cast<void>(
                                    current_player.project_collision_velocity(
                                        recovery_hit->normal));
                            } else if (current_player.physics_state() != 0) {
                                collision_response_projection_pending = true;
                            }
                            if (leave_air) {
                                if (support_leave_air
                                    && recovery_hit->surface_bit_8_clear) {
                                    // The inverse24-set state-0 branch
                                    // reaches the contact-plus-axis write at
                                    // 0x00496f80 before the outer commit. The
                                    // inverse24-clear branch exits directly
                                    // and preserves the integrated point.
                                    desired = offset(
                                        recovery_hit->position,
                                        recovery_axis,
                                        30);
                                }
                                ground_leave_air_requested = true;
                                if (non_ground_recovery_leave_air) {
                                    ground_leave_air_reason = 0x160b;
                                }
                            } else if (!ordinary_recovery_state_one) {
                                if (current_player.physics_state() == 0
                                    && recovery_hit->surface_bit_6
                                    && recovery_window_recent) {
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
                                if (!transient_exit) {
                                    static_cast<void>(
                                        current_player.update_ground_surface_recovery(
                                            recovery_hit->normal,
                                            hooks.ground_surface_recovery_delta_q11));
                                }
                                // State 0 seeds the transient correction with the
                                // recovered 0x1964 vertical vector, then removes
                                // its surface-normal component through
                                // FUN_00490610.
                                FixedPosition surface_correction{0, 0x1964, 0};
                                static_cast<void>(remove_normal_component(
                                    surface_correction,
                                    recovery_hit->normal));
                                if (current_player.physics_state() == 2) {
                                    // Retail keeps +3128/+312c as the
                                    // state-two response normal.  It is
                                    // consumed by the next frame's
                                    // FUN_0049c060 call, after this recovery
                                    // query has published its normal.
                                    current_player.set_ground_surface_response_normal(
                                        recovery_hit->normal);
                                } else {
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
                                const FixedPosition recovery_candidate = offset(
                                    recovery_hit->position,
                                    recovery_axis,
                                    30);
                            const PositionCollisionProbe recovery_probe =
                            [&ground_collision_query, &desired](
                                const FixedPosition& candidate) {
                                return ground_collision_query(
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
                            }
                        }
                }
                if (!result.ground_surface_hit.has_value()
                    && current_player.physics_state() == 0) {
                    // A missed FUN_00496fd7 support query reaches the common
                    // FUN_0049756f label and therefore calls
                    // FUN_004956f0. The helper's ordinary branch is the
                    // 0x160b state-1 handoff; it is not limited to a
                    // downward hit returned by the query.
                    ground_leave_air_requested = true;
                    ground_leave_air_reason = 0x160b;
                } else if (!result.ground_surface_hit.has_value()
                           && current_player.physics_state() == 2) {
                    // When the second FUN_00466090 sweep misses, retail
                    // FUN_004956f0 is the state-2 exit. It requests state 1
                    // with reason 0x1605; the common tail then selects the
                    // raw +0x1964 correction instead of the state-2 +0x2dac
                    // writer.
                    current_player.set_motion_correction(
                        FixedPosition{0, 0x1964, 0});
                    const GroundCollisionRecoveryExitResult exit =
                        current_player.exit_ground_collision_recovery();
                    if (hooks.on_ground_collision_recovery_exit) {
                        hooks.on_ground_collision_recovery_exit(
                            current_player,
                            exit);
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
                const bool ground_facing_movement =
                    movement_collision->normal[1] < 0;
                if (use_movement_recovery_base
                    && ground_facing_movement) {
                    static_cast<void>(current_player.update_ground_surface_recovery(
                        movement_collision->normal,
                        hooks.ground_surface_recovery_delta_q11));
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
                if (use_movement_recovery_base && !ground_facing_movement) {
                    // The horizontal movement-hit branch writes the live
                    // position back to +0xbc before the shared support tail.
                    // Its integrated point is only the candidate tested by
                    // FUN_00496955; it is not the anchor for 0x00496fd7.
                    current_player.set_position(start);
                }
                FixedPosition recovery_base = current_player.position();
                if (use_movement_recovery_base
                    && ground_facing_movement) {
                    // The ordinary ground-facing movement-hit path commits
                    // the contact-plus-normal candidate itself. The later
                    // secondary recovery sweep can alter response/basis
                    // state, but it does not replace this candidate with the
                    // integrated point when the surface-bit-6 path is clear.
                    desired = movement_candidate;
                    movement_recovery_candidate_selected = true;
                    // The movement-hit path calls FUN_00496060 immediately;
                    // its selected contact candidate is therefore the live
                    // point from which the later support sweep is built.
                    current_player.set_position(desired);
                    recovery_base = current_player.position();
                }
                const FixedPosition recovery_start = offset(
                    recovery_base,
                    recovery_direction,
                    70);
                const FixedPosition recovery_end = offset(
                    recovery_base,
                    recovery_direction,
                    -186);
                const std::optional<PositionCollisionHit> recovery_hit =
                    ground_collision_query(recovery_start, recovery_end);
                result.ground_surface_hit = recovery_hit;
                if (recovery_hit.has_value()) {
                    // The grounded movement-hit path shares the long-recovery
                    // exit with FUN_004956f0.  The sweep distance is measured
                    // from the actual secondary query, so this remains causal
                    // when the same geometry is reached on a different frame.
                    const bool recovery_window_recent =
                        current_player.collision_recovery_frame() == 0
                        || current_player.frame_counter()
                               - current_player.collision_recovery_frame()
                            < 5;
                    const bool recent_special_recovery =
                        current_player.physics_state() == 0
                        && movement_collision->surface_bit_6
                        && recovery_window_recent;
                    const bool recovery_leave_air =
                        (current_player.physics_state() == 0
                         && movement_collision->surface_bit_6
                         && !recovery_window_recent)
                        || retail_hit_distance(
                            recovery_start,
                            recovery_end,
                            recovery_hit->hit_parameter_q14)
                            > 0x74;
                    if (recent_special_recovery) {
                        // FUN_004972df enters the collision-transient state
                        // when the wall contact is still inside the recent
                        // recovery window. This is the movement-hit sibling
                        // of the support/recovery state-2 path above; it must
                        // not be mistaken for the older state-1 leave-air
                        // branch merely because the hit carries bit 6.
                        const FixedPosition transition_basis =
                            current_player.retail_basis().at_30f4;
                        current_player.request_physics_state_from_basis(
                            2,
                            0x1ac9,
                            transition_basis);
                        state_two_entered_from_recovery = true;
                    }
                    if (recovery_leave_air) {
                        // The long-recovery exit retains the ordinary
                        // movement contact as the position candidate.  The
                        // secondary sweep only decides the state handoff; it
                        // does not replace that contact with its own recovery
                        // candidate.
                        if (!movement_collision->surface_bit_6) {
                            desired = movement_candidate;
                        } else {
                            // The direct surface path still runs the shared
                            // candidate committer before FUN_00497265 makes
                            // the state-1 request. It skips the response and
                            // basis side effects, but its accepted candidate
                            // remains part of the position result.
                            const FixedPosition recovery_candidate = offset(
                                recovery_hit->position,
                                recovery_direction,
                                30);
                                const PositionCollisionProbe recovery_probe =
                                [&ground_collision_query, &recovery_base](
                                    const FixedPosition& candidate) {
                                    return ground_collision_query(
                                        recovery_base,
                                        candidate).has_value();
                                };
                            const PositionCommitResult recovery_commit =
                                PositionCommitter::commit(
                                    recovery_base,
                                    recovery_candidate,
                                    recovery_probe,
                                    hooks.bypass_collision);
                            const FixedPosition recovery_displacement{
                                recovery_commit.position[0] - recovery_base[0],
                                recovery_commit.position[1] - recovery_base[1],
                                recovery_commit.position[2] - recovery_base[2],
                            };
                            if (fixed_dot_q12(
                                    recovery_displacement,
                                    recovery_displacement) >= 0x1000) {
                                desired = recovery_commit.position;
                            }
                        }
                        if (current_player.physics_state() == 0
                            && movement_collision->surface_bit_6) {
                            // The complementary FUN_004956f0 path modifies
                            // the response using the final recovery normal
                            // before issuing its state-1 request. The direct
                            // surface-bit-6-clear path keeps +0x4c intact
                            // and only performs the common correction handoff.
                            current_player.apply_ground_leave_air_response(
                                recovery_hit->normal);
                        }
                        ground_leave_air_requested = true;
                    } else {
                        collision_transient_exit_normal = recovery_hit->normal;
                        static_cast<void>(
                            current_player.update_ground_surface_recovery(
                                recovery_hit->normal,
                                hooks.ground_surface_recovery_delta_q11));
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
                        // The support-hit common tail calls FUN_00490680 only
                        // after the selected surface correction has been
                        // added to +0x58. Keep this projection at that
                        // boundary; it is not the earlier FUN_00490610
                        // response stage.
                        if (current_player.physics_state() != 1) {
                            static_cast<void>(
                                current_player.project_collision_velocity(
                                    recovery_hit->normal));
                        }
                        if (!use_movement_recovery_base) {
                            // The direct surface-bit path reaches the shared
                            // 0x004975c7 tail with the static 0x1964 correction
                            // applied a second time. It also skips the generic
                            // response-basis subtraction below.
                            current_player.add_motion_correction(surface_correction);
                        }
                        const FixedPosition recovery_candidate = offset(
                            recovery_hit->position,
                            recovery_direction,
                            30);
                        const PositionCollisionProbe recovery_probe =
                            [&ground_collision_query, &recovery_base](
                                const FixedPosition& candidate) {
                                return ground_collision_query(
                                    recovery_base,
                                    candidate).has_value();
                            };
                        const PositionCommitResult recovery_commit =
                            PositionCommitter::commit(
                                recovery_base,
                                recovery_candidate,
                                recovery_probe,
                                hooks.bypass_collision);
                        // FUN_004f5f90 rejects a short secondary recovery
                        // displacement after FUN_00496060 selects its candidate.
                        // The wall branch therefore falls back to the live
                        // position for the small frame-206 correction, while the
                        // larger Warehouse recovery in the canonical corpus is
                        // still committed.
                        const FixedPosition recovery_displacement{
                            recovery_commit.position[0] - recovery_base[0],
                            recovery_commit.position[1] - recovery_base[1],
                            recovery_commit.position[2] - recovery_base[2],
                        };
                        if (fixed_dot_q12(
                                recovery_displacement,
                                recovery_displacement) >= 0x1000) {
                            desired = recovery_commit.position;
                            movement_recovery_candidate_selected =
                                recovery_commit.position != recovery_base;
                        }
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
                    // A movement hit with no support hit reaches the common
                    // tail's local_e0 add, but not FUN_00490680 (the support
                    // query has no collision record at that point).
                    current_player.add_motion_correction(surface_correction);
                }
                // A non-ground ordinary hit is handled by the wall branch of
                // FUN_00496955/00496a82. It retains the live position for the
                // outer commit; the integrated point is only the transient
                // candidate used by the branch's collision tests.
                if (use_movement_recovery_base
                    && !ground_facing_movement
                    && !movement_recovery_candidate_selected) {
                    desired = start;
                }
            }
            result.position_commit = current_player.commit_position(
                desired,
                probe,
                hooks.bypass_collision);
            if (stage == PhysicsDispatchStage::InAir_97f40
                && current_player.physics_state() != 2
                && hooks.air_upright_input) {
                const std::optional<FixedPosition> global_up =
                    hooks.air_upright_input(current_player, input);
                if (global_up.has_value()) {
                    current_player.apply_upright_correction(*global_up);
                }
            }
            // The grounded steering velocity phase belongs after the
            // collision routine has produced its response and before the
            // outer correction handoff. It is an explicit state phase, not a
            // part of the position candidate selection.
            if (ground_leave_air_requested
                && stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0) {
                // FUN_004956f0 performs its state request before the common
                // tail adds the raw +0x1964 vertical correction to +0x58.
                if (ground_leave_air_reason == 0x160b) {
                    const GroundCollisionRecoveryExitResult exit =
                        current_player.exit_ground_collision_recovery();
                    if (hooks.on_ground_collision_recovery_exit) {
                        hooks.on_ground_collision_recovery_exit(
                            current_player,
                            exit);
                    }
                } else {
                    current_player.request_physics_state(
                        1,
                        ground_leave_air_reason);
                }
                current_player.add_motion_correction(
                    FixedPosition{0, 0x1964, 0});
            }
            if (hooks.collision_query) {
                result.collision_hit = queried_hit;
            }
            StandardAirContactDisposition air_contact_disposition =
                StandardAirContactDisposition::None;
            if (result.collision_hit.has_value()
                && stage == PhysicsDispatchStage::InAir_97f40
                && hooks.standard_air_contact_input) {
                const std::optional<StandardAirContactInput> contact_input =
                    hooks.standard_air_contact_input(
                        current_player, input, *result.collision_hit);
                if (contact_input.has_value()) {
                    air_contact_disposition =
                        classify_standard_air_contact(
                            *result.collision_hit,
                            current_player.physics_state(),
                            input.action(kJumpActionBit).held,
                            input.action(kJumpActionBit).inactive_frames,
                            current_player.frame_counter(),
                            *contact_input);
                }
            }
            std::optional<AirNormalRecoveryInput> air_normal_recovery_input;
            if (result.collision_hit.has_value()
                && stage == PhysicsDispatchStage::InAir_97f40
                && hooks.air_normal_recovery_input) {
                air_normal_recovery_input = hooks.air_normal_recovery_input(
                    current_player,
                    input,
                    *result.collision_hit);
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
                && !result.ground_surface_hit.has_value()
                && !air_normal_recovery_input.has_value()
                && air_contact_disposition
                    != StandardAirContactDisposition::SurfaceRecovery) {
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
            if (air_normal_recovery_input.has_value()) {
                // FUN_00496060 receives the collision contact plus the raw
                // signed-short normal as its candidate. The normal is a
                // fixed-point position offset here, not a Q12 product.
                FixedPosition recovery_position =
                    result.collision_hit->position;
                for (std::size_t index = 0;
                     index < recovery_position.size();
                     ++index) {
                    recovery_position[index] += static_cast<std::int16_t>(
                        result.collision_hit->normal[index]);
                }
                result.position_commit = current_player.commit_position(
                    recovery_position,
                    PositionCollisionProbe{},
                    true);
                current_player.clear_motion_correction();
                current_player.request_physics_state(1, 0x1f83);
                current_player.apply_air_normal_recovery(
                    result.collision_hit->normal,
                    *air_normal_recovery_input);
                air_normal_recovery_handled = true;
                result.position_integrated = true;
                return;
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
                    accepted_air_contact =
                        air_contact_disposition
                        != StandardAirContactDisposition::None;
                }
            }
            if (accepted_air_contact) {
                if (air_contact_disposition
                    == StandardAirContactDisposition::SurfaceRecovery) {
                    // FUN_00497f40 has already written the integrated
                    // position before this query. Its accepted surface path
                    // calls FUN_00496060 with a corrective candidate; when
                    // that candidate is blocked, the live integrated point
                    // remains authoritative and FUN_00497aa0 returns to the
                    // existing in-air state. Preserve that ordering rather
                    // than turning a surface-recovery contact into a landing.
                    result.position_commit = current_player.commit_position(
                        desired,
                        PositionCollisionProbe{},
                        true);
                    current_player.request_physics_state(1, 0x1cb1);
                    result.position_integrated = true;
                    return;
                }
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
                // FUN_00498a66 routes the contact-plus-normal candidate back
                // through FUN_00496060. That committer is what preserves the
                // unobstructed axis from the already integrated position; a
                // direct write would incorrectly retain the full collision
                // contact on every axis.
                result.position_commit = current_player.commit_position(
                    contact_position,
                    probe,
                    hooks.bypass_collision);
                if (air_contact_disposition
                    == StandardAirContactDisposition::Landing) {
                    // FUN_00497f40's ordinary accepted-contact path requests
                    // state 0, then runs FUN_00491780. The helper itself
                    // owns the response-normal projection and orientation
                    // recovery; class/material bits do not select a second
                    // synthetic landing handoff.
                    current_player.request_physics_state(0, 0x1fd6);
                    // Retail 0x00497f40 clears +0x3144 before
                    // FUN_00491780/FUN_00497960. This belongs to the
                    // state-1 -> state-0 landing handoff.
                    current_player.set_turn_accumulator(0);
                    current_player.apply_collision_transient_exit_orientation(
                        result.collision_hit->normal);
                    // The state request makes the post-landing state differ
                    // from the in-air state, so retail's trailing
                    // FUN_0049d080 publication runs on the accepted normal
                    // after FUN_00491780. This is the ordinary landing path,
                    // not a material-specific replay branch.
                    current_player.apply_orientation_recovery(
                        result.collision_hit->normal,
                        true);
                    current_player.seed_ground_surface_recovery(
                        result.collision_hit->normal);
                    result.landed = true;
                } else {
                    result.landed = current_player.accept_air_contact(
                        current_player.position(),
                        result.collision_hit.has_value()
                        ? result.collision_hit->normal
                        : current_player.air_motion());
                }
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
                && !current_player.control_blocked()
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
            if (stage == PhysicsDispatchStage::GroundCollision_96550
                && current_player.physics_state() == 0
                && hooks.apply_ground_basis_correction
                && !current_player.control_blocked()
                && current_player.ground_surface_class() == 3) {
                // The class-3 tail at 0x0049789f follows the ordinary basis
                // correction, but is not gated by its local response/profile
                // predicate. It projects the same response onto +0x30f4 and
                // subtracts a 0xb4 term scaled by the current frame clock.
                current_player.prepare_ground_basis_correction(
                    true,
                    frame_scale_q8,
                    0xb4,
                    false);
            }
            if (collision_response_projection_pending
                && current_player.physics_state() == 0
                && !collision_transient_exit_requested) {
                static_cast<void>(current_player.project_collision_velocity(
                    collision_transient_exit_normal));
                collision_response_projection_pending = false;
            }
            result.position_integrated = true;
        },
    };
    // FUN_0049e680 refreshes +0x2dc8 before the outer floor check and before
    // entering Skater_PhysicsDispatcher. Keep the threshold producer at that
    // same pre-dispatch boundary.
    if (hooks.ground_motion_threshold_input) {
        const std::optional<GroundMotionThresholdInput> threshold_input =
            hooks.ground_motion_threshold_input(player, input);
        if (threshold_input.has_value()) {
            result.ground_motion_threshold =
                player.update_ground_motion_threshold(*threshold_input);
        }
    }
    if (hooks.apply_outer_floor_recovery && hooks.collision_query) {
        result.outer_floor_recovery = apply_outer_floor_recovery(
            player,
            player.position(),
            hooks.collision_query,
            hooks.outer_floor_restart_at_start,
            hooks.on_outer_floor_external_service);
    }
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
        if (collision_response_projection_pending
            && player.physics_state() == 2) {
            static_cast<void>(player.project_collision_velocity(
                collision_transient_exit_normal));
            collision_response_projection_pending = false;
        }
    }

    if (collision_transient_exit_requested) {
        // FUN_00497479 exits the collision-transient state after the
        // recovery candidate has been selected. The remainder of that
        // helper then runs the ordinary state-0 surface handoff in the same
        // frame: seed +0x58 with the projected 0x1964 vector, remove the
        // response's lateral basis component, and apply the forward term.
        player.apply_collision_transient_exit_orientation(
            collision_transient_exit_normal);
        static_cast<void>(player.update_ground_surface_recovery(
            collision_transient_exit_normal,
            hooks.ground_surface_recovery_delta_q11));
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
        // FUN_004975? calls FUN_00490680 before the state-0 basis tail. The
        // tail therefore projects the already-normalized response, rather
        // than changing the vector that the projection receives.
        static_cast<void>(player.project_collision_velocity(
            collision_transient_exit_normal));
        if (hooks.apply_ground_basis_correction) {
            player.prepare_ground_basis_correction(
                hooks.apply_ground_basis_forward_term,
                frame_scale_q8,
                8,
                true);
        }
        collision_response_projection_pending = false;
    }

    // FUN_0049d8a0 runs after the selected dispatcher stage.  The in-process
    // state reset currently models the proven +0x2f64 lateral-correction
    // gate; preserve Y so the just-produced ground correction is integrated
    // into the persistent response below.
    if (player.control_blocked()) {
        player.apply_control_blocked_reset(frame_scale_q8);
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
        && !air_normal_recovery_handled
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
