#include "player_state.hpp"

#include "tricks_bin.hpp"

#include <cstdint>

namespace opentony::runtime {

void PlayerState::request_physics_state(
    std::int32_t state,
    std::uint32_t reason) noexcept {
    last_state_request_ = PhysicsStateRequest{
        physics_state_,
        state,
        reason,
        physics_state_ != state,
    };
    physics_state_ = state;
}

void PlayerState::apply_restart(
    FixedPosition position,
    std::uint32_t auxiliary,
    std::uint16_t auxiliary_word) noexcept {
    position_ = position;
    previous_position_ = position;
    collision_response_ = {};
    motion_correction_ = {};
    air_motion_ = {};
    queued_motion_ = {};
    action_stream_active_ = false;
    action_stream_relative_ = 0;
    action_stream_cursor_ = 0;
    ground_physics_mode_ = 0;
    orientation_ = q12_restart_matrix(auxiliary);
    retail_basis_ = retail_basis_from_matrix(orientation_);
    restart_auxiliary_ = auxiliary;
    restart_auxiliary_word_ = auxiliary_word;
}

bool PlayerState::set_queued_motion_command(
    std::int32_t axis,
    std::int16_t amount,
    std::int16_t rate) noexcept {
    return opentony::runtime::set_queued_motion_command(
        queued_motion_,
        axis,
        amount,
        rate);
}

QueuedMotionDrainResult PlayerState::drain_queued_motion(
    std::int32_t frame_scale_q8) noexcept {
    return opentony::runtime::drain_queued_motion(
        queued_motion_,
        frame_scale_q8);
}

FixedPosition PlayerState::apply_queued_motion(
    const QueuedMotionDrainResult& motion) noexcept {
    if (!motion.moved) {
        return {};
    }
    const FixedPosition world_delta = q12_transform_vector(
        orientation_,
        FixedPosition{
            motion.local_delta[0],
            motion.local_delta[1],
            motion.local_delta[2],
        });
    position_[0] += world_delta[0];
    position_[1] += world_delta[1];
    position_[2] += world_delta[2];
    return world_delta;
}

ActionCommandDispatchResult PlayerState::dispatch_action_command(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor) noexcept {
    return opentony::runtime::dispatch_action_command(
        stream,
        cursor,
        queued_motion_,
        &collision_response_,
        &action_command_state_);
}

ActionStreamDispatchResult PlayerState::run_action_stream(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    std::size_t max_commands) noexcept {
    return opentony::runtime::run_action_stream(
        stream,
        cursor,
        queued_motion_,
        &collision_response_,
        max_commands,
        &action_command_state_);
}

void PlayerState::publish_action_profile(
    const ActionProfileState& profile,
    std::uint32_t timestamp,
    std::int8_t vertical_lean,
    std::int8_t horizontal_lean) noexcept {
    const std::uint8_t selected = select_action_table_entry(
        profile,
        vertical_lean,
        horizontal_lean);
    for (std::uint8_t action = 1; action <= 8; ++action) {
        static_cast<void>(action_history_.publish(
            action,
            selected == action,
            timestamp));
    }
    // FUN_00492190 publishes only these configured profile records. IDs 13
    // and 15 are not touched by this retail function.
    static constexpr std::array<std::pair<std::uint8_t, std::uint16_t>, 6> kProfileRecords{
        std::pair{9, 0x20},
        std::pair{10, 0x10},
        std::pair{11, 0x30},
        std::pair{12, 0x00},
        std::pair{14, 0x50},
        std::pair{16, 0x70},
    };
    for (const auto& [action, offset] : kProfileRecords) {
        static_cast<void>(action_history_.publish(
            action,
            profile.slot_at_offset(offset),
            timestamp));
    }
}

ActionSequenceExecutionResult PlayerState::run_action_sequences(
    const assets::TricksBinView& tricks,
    std::span<const std::uint8_t> sequence_table,
    const ActionSequenceMatcherInput& input,
    std::size_t max_records,
    std::size_t max_commands) noexcept {
    ActionSequenceExecutionResult result{};

    if (action_stream_active_) {
        const auto stream = tricks.action_stream(action_stream_relative_);
        if (!stream.has_value()) {
            // The source image changed or the saved signed offset is no
            // longer valid. Retail would no longer have a usable +29cc
            // cursor; clear the native equivalent rather than replaying an
            // unrelated stream from offset zero.
            action_stream_active_ = false;
            action_stream_cursor_ = 0;
            return result;
        }
        result.stream_resolved = true;
        result.stream_resumed = true;
        result.stream = run_action_stream(
            *stream,
            action_stream_cursor_,
            max_commands);
        if (result.stream.completed || result.stream.malformed) {
            action_stream_active_ = false;
        }
        result.stream_active = action_stream_active_;
        return result;
    }

    result.match = match_action_sequence(
        sequence_table,
        action_history_,
        input,
        max_records);
    if (!result.match.matched) {
        return result;
    }
    const auto stream = tricks.action_stream(result.match.stream_relative);
    if (!stream.has_value()) {
        return result;
    }
    result.stream_resolved = true;
    result.stream_started = true;
    action_stream_active_ = true;
    action_stream_relative_ = result.match.stream_relative;
    action_stream_cursor_ = 0;
    // FUN_00491b80's confirmed stream-start writes. The remaining reset
    // fields belong to the larger animation/action object and stay outside
    // this raw runtime state until their owners are identified.
    action_command_state_.word_29c0 = 0x7b;
    action_command_state_.dword_2e2c = 0;
    result.stream = run_action_stream(
        *stream,
        action_stream_cursor_,
        max_commands);
    if (result.stream.completed || result.stream.malformed) {
        action_stream_active_ = false;
    }
    result.stream_active = action_stream_active_;
    return result;
}

OllieImpulseResult PlayerState::apply_ollie_impulse(
    const OllieImpulseInput& input) noexcept {
    const OllieImpulseResult result = compute_ollie_vertical_impulse(input);
    ollie_.launch_charge = input.charge;
    ollie_.latched = 0;
    ollie_.in_progress = 1;
    ollie_.mode_latched = ollie_.mode;
    ++ollie_.launch_count;
    const std::int32_t cap =
        0xf - (input.random.first + input.random.second) / 0x14;
    if (input.charge < cap) {
        ++ollie_.early_release_count;
    }
    ollie_.charge = 0;
    ollie_.launch_frame = frame_counter_;
    collision_response_[1] += result.delta_y;
    ollie_.pending = 0;
    return result;
}

OlliePrePhysicsResult PlayerState::run_ollie_prephysics(
    const InputState& input,
    const OlliePrePhysicsInput& config) noexcept {
    OlliePrePhysicsResult result;
    result.charge = ollie_.charge;
    result.cap = 0xf -
        (config.impulse.random.first + config.impulse.random.second) / 0x14;
    if (config.prephysics_blocked) {
        return result;
    }

    const std::int32_t current_frame = config.current_frame >= 0
        ? config.current_frame
        : frame_counter_;
    if (input.action(kKickActionBit).held) {
        ++ollie_.charge;
        result.event = OlliePrePhysicsEvent::Charging;
        if (result.cap < ollie_.charge || config.force_cap) {
            ollie_.charge = result.cap;
            result.capped = true;
        }

        if ((physics_state_ == 0 || physics_state_ == 7 ||
             physics_state_ == 4 || physics_state_ == 5) &&
            ollie_.animation_gate == 0) {
            ollie_.latched = 1;
            result.latch_set = true;
        }
        if (ollie_.animation_gate != 0) {
            result.charge = ollie_.charge;
            return result;
        }
        ollie_.pending = 1;
        result.pending_set = true;
        result.charge = ollie_.charge;
        return result;
    }

    ollie_.animation_gate = 0;
    ollie_.pending = 0;
    if (ollie_.latch_timestamp > 0 &&
        current_frame - ollie_.latch_timestamp > 0x14) {
        ollie_.latched = 0;
        result.stale_latch_cleared = true;
        result.event = OlliePrePhysicsEvent::StaleLatchCleared;
    }
    if (ollie_.latched == 0) {
        ollie_.charge = 0;
        result.charge = 0;
        return result;
    }
    if (config.global_release_mode == 1 && physics_state_ != 4 &&
        physics_state_ != 5 && ollie_.special_mode == 0) {
        ollie_.latched = 0;
        ollie_.charge = 0;
        result.event = OlliePrePhysicsEvent::Cancelled;
        result.charge = 0;
        return result;
    }

    OllieImpulseInput impulse = config.impulse;
    impulse.charge = ollie_.charge;
    impulse.wallie = physics_state_ == 5;
    ollie_.speed_metric = impulse.horizontal_speed_metric;
    ollie_.wallie = impulse.wallie ? 1 : 0;
    static_cast<void>(apply_ollie_impulse(impulse));
    ollie_.launch_frame = current_frame;
    result.launch_consumed = true;
    result.event = OlliePrePhysicsEvent::Launched;

    if (physics_state_ != 2) {
        const bool alternate = ollie_.mode == 0 && physics_state_ == 0;
        result.requested_state = alternate ? 3 : 1;
        result.request_reason = alternate
            ? kAlternateLaunchReason
            : kOrdinaryLaunchReason;
        request_physics_state(result.requested_state, result.request_reason);
        result.state_requested = true;
    }
    result.charge = ollie_.charge;
    return result;
}

bool PlayerState::apply_in_air_jump_hold_effect(
    const InputState& input) noexcept {
    const ActionTransition& jump = input.action(kJumpActionBit);
    if (!jump.held || jump.held_frames <= 2) {
        return false;
    }
    motion_correction_[1] = 0;
    collision_response_[1] = 0;
    return true;
}

AirGravityResult PlayerState::apply_air_gravity(
    AirGravityConfig config) noexcept {
    const AirGravityResult result = opentony::runtime::apply_air_gravity(
        air_motion_,
        config);
    air_motion_ = result.velocity;
    return result;
}

AirMotionBasisResult PlayerState::update_air_motion_basis() noexcept {
    const AirMotionBasisResult result = rebuild_air_motion_basis(
        retail_basis_,
        air_motion_);
    air_motion_ = result.direction;
    retail_basis_ = result.basis;
    orientation_.at(0, 0) = static_cast<std::int16_t>(retail_basis_.at_3100[0]);
    orientation_.at(1, 0) = static_cast<std::int16_t>(retail_basis_.at_3100[1]);
    orientation_.at(2, 0) = static_cast<std::int16_t>(retail_basis_.at_3100[2]);
    orientation_.at(0, 1) = static_cast<std::int16_t>(retail_basis_.at_310c[0]);
    orientation_.at(1, 1) = static_cast<std::int16_t>(retail_basis_.at_310c[1]);
    orientation_.at(2, 1) = static_cast<std::int16_t>(retail_basis_.at_310c[2]);
    orientation_.at(0, 2) = static_cast<std::int16_t>(retail_basis_.at_30f4[0]);
    orientation_.at(1, 2) = static_cast<std::int16_t>(retail_basis_.at_30f4[1]);
    orientation_.at(2, 2) = static_cast<std::int16_t>(retail_basis_.at_30f4[2]);
    return result;
}

AirDirectionInputResult PlayerState::apply_air_direction_input(
    const InputState& input,
    AirDirectionInputConfig config) noexcept {
    const AirDirectionInputResult result =
        opentony::runtime::apply_air_direction_input(
            motion_correction_,
            retail_basis_,
            input.held(movement_bit(MovementAction::Up)),
            input.held(movement_bit(MovementAction::Down)),
            config);
    motion_correction_ = result.motion_correction;
    return result;
}

AirDirectionInputResult PlayerState::apply_air_direction_input(
    const InputState& input,
    AirSpeedConfig speed_config,
    std::int32_t scale_percent) noexcept {
    return apply_air_direction_input(
        input,
        AirDirectionInputConfig{
            compute_air_speed_scalar(speed_config),
            scale_percent,
        });
}

bool PlayerState::accept_air_contact(
    FixedPosition contact_position) noexcept {
    if (physics_state_ != 1 && physics_state_ != 3) {
        return false;
    }
    previous_position_ = position_;
    position_ = contact_position;
    request_physics_state(0, kLandingReason);
    return true;
}

VelocityDampingResult PlayerState::apply_velocity_damping(
    VelocityDampingInput input) noexcept {
    input.velocity = collision_response_;
    const VelocityDampingResult result = VelocityDamping::apply(input);
    collision_response_ = result.velocity;
    return result;
}

GroundBrakeResult PlayerState::apply_ground_brake(
    GroundBrakeInput input) noexcept {
    input.response = collision_response_;
    input.physics_state = physics_state_;
    const GroundBrakeResult result = GroundBrake::apply(input);
    collision_response_ = result.response;
    if (result.requested_state7) {
        request_physics_state(7, kGroundStopReason);
    }
    return result;
}

GroundPhysicsResult PlayerState::update_ground_physics(
    GroundPhysicsInput input) noexcept {
    input.response = collision_response_;
    input.physics_state = physics_state_;
    input.ground_update_state = ground_physics_mode_;
    input.animation_frame = animation_frame_;
    input.animation_state = animation_state_;
    const GroundPhysicsResult result =
        opentony::runtime::update_ground_physics(input);
    collision_response_ = result.response;
    ground_physics_mode_ = result.ground_update_state;
    if (result.cooldown_written) {
        ground_motion_cooldown_ = result.cooldown_value;
    }
    if (result.physics_state_requested) {
        request_physics_state(
            result.requested_physics_state,
            result.requested_physics_reason);
    }
    return result;
}

PositionCommitResult PlayerState::commit_position(
    FixedPosition desired,
    const PositionCollisionProbe& probe,
    bool bypass_collision) {
    const PositionCommitResult result = PositionCommitter::commit(
        position_,
        desired,
        probe,
        bypass_collision);
    position_ = result.position;
    return result;
}

std::int32_t PlayerState::remove_collision_normal_component(
    const FixedPosition& normal) {
    return remove_normal_component(collision_response_, normal);
}

VelocityProjectionResult PlayerState::project_collision_velocity(
    const FixedPosition& normal) {
    const VelocityProjectionResult result =
        project_velocity_preserving_magnitude(collision_response_, normal);
    collision_response_ = result.velocity;
    return result;
}

CollisionResponseResult PlayerState::apply_collision_response(
    const FixedPosition& surface_delta,
    std::int32_t bias_q12) {
    return opentony::runtime::apply_inward_response(
        collision_response_,
        surface_delta,
        bias_q12);
}

CollisionOrientationResult PlayerState::apply_collision_orientation(
    const FixedPosition& surface_delta,
    std::int32_t yaw_offset) noexcept {
    const FixedPosition local{
        -surface_delta[0],
        -surface_delta[1],
        -surface_delta[2],
    };
    const std::int32_t forward_dot = fixed_dot_q12(
        retail_basis_.at_30f4,
        local);
    const std::int32_t lateral_dot = fixed_dot_q12(
        retail_basis_.at_3100,
        local);
    if (forward_dot >= 0) {
        return CollisionOrientationResult{
            forward_dot,
            lateral_dot,
            0,
            false,
        };
    }

    const std::int64_t radicand =
        static_cast<std::int64_t>(0x1000000)
        - static_cast<std::int64_t>(forward_dot) * forward_dot;
    std::int64_t low = 0;
    std::int64_t high = radicand > 0 ? radicand + 1 : 1;
    while (low + 1 < high) {
        const std::int64_t middle = low + (high - low) / 2;
        if (middle <= radicand / middle) {
            low = middle;
        } else {
            high = middle;
        }
    }
    std::int32_t sine = static_cast<std::int32_t>(low);
    if (lateral_dot > 0) {
        sine = -sine;
    }

    Q12Matrix3 alignment = q12_identity_matrix();
    alignment.at(0, 0) = static_cast<std::int16_t>(forward_dot);
    alignment.at(0, 2) = static_cast<std::int16_t>(-sine);
    alignment.at(2, 0) = static_cast<std::int16_t>(sine);
    alignment.at(2, 2) = static_cast<std::int16_t>(forward_dot);
    orientation_ = q12_matrix_multiply(orientation_, alignment);

    // FUN_004e80e0 receives the yaw word in its middle short. Its Y-rotation
    // helper is the opposite-sign form of the native yaw matrix.
    const std::int32_t angle = lateral_dot < 1
        ? (0x400 - yaw_offset) & 0xfff
        : (yaw_offset - 0x400) & 0xfff;
    orientation_ = q12_matrix_multiply(
        orientation_,
        q12_yaw_matrix(-angle));

    retail_basis_ = retail_basis_from_matrix(orientation_);
    const FixedPosition direction = q12_normalize(air_motion_);
    const FixedPosition right = q12_normalize(
        q12_cross(
            q12_normalize(retail_basis_.at_30f4),
            direction));
    const FixedPosition forward = q12_cross(direction, right);
    air_motion_ = direction;
    retail_basis_ = RetailBasis{forward, right, direction};
    orientation_.at(0, 0) = static_cast<std::int16_t>(right[0]);
    orientation_.at(1, 0) = static_cast<std::int16_t>(right[1]);
    orientation_.at(2, 0) = static_cast<std::int16_t>(right[2]);
    orientation_.at(0, 1) = static_cast<std::int16_t>(direction[0]);
    orientation_.at(1, 1) = static_cast<std::int16_t>(direction[1]);
    orientation_.at(2, 1) = static_cast<std::int16_t>(direction[2]);
    orientation_.at(0, 2) = static_cast<std::int16_t>(forward[0]);
    orientation_.at(1, 2) = static_cast<std::int16_t>(forward[1]);
    orientation_.at(2, 2) = static_cast<std::int16_t>(forward[2]);
    return CollisionOrientationResult{
        forward_dot,
        lateral_dot,
        angle,
        true,
    };
}

void PlayerState::apply_bouncy_platform_response(
    std::int32_t platform_type,
    const FixedPosition& source_vector,
    std::int32_t source_magnitude_q12) noexcept {
    collision_response_ = PlatformResponse::bouncy_velocity(
        platform_type,
        source_vector,
        source_magnitude_q12);
}

void PlayerState::prepare_ground_basis_correction(
    bool apply_forward_term,
    std::int32_t forward_scale) noexcept {
    const auto project = [](const FixedPosition& vector,
                            const FixedPosition& basis) {
        const std::int32_t dot = fixed_dot_q12(vector, basis);
        return FixedPosition{
            fixed_multiply_q12(dot, basis[0]),
            fixed_multiply_q12(dot, basis[1]),
            fixed_multiply_q12(dot, basis[2]),
        };
    };

    const FixedPosition lateral = project(
        collision_response_,
        retail_basis_.at_3100);
    for (std::size_t index = 0; index < motion_correction_.size(); ++index) {
        motion_correction_[index] -= lateral[index];
    }

    if (apply_forward_term) {
        const FixedPosition forward = project(
            collision_response_,
            retail_basis_.at_30f4);
        for (std::size_t index = 0; index < motion_correction_.size(); ++index) {
            motion_correction_[index] -= fixed_multiply_q12(
                forward_scale,
                forward[index]);
        }
    }
}

void PlayerState::integrate_motion_correction(
    std::int32_t frame_scale_q8) noexcept {
    for (std::size_t index = 0; index < collision_response_.size(); ++index) {
        collision_response_[index] += fixed_scale_q8(
            motion_correction_[index],
            frame_scale_q8);
    }
}

void PlayerState::integrate_position(
    std::int32_t frame_scale_q8) noexcept {
    position_ = integrated_position(frame_scale_q8);
}

FixedPosition PlayerState::integrated_position(
    std::int32_t frame_scale_q8) const noexcept {
    FixedPosition result = position_;
    const std::int32_t frame_scale_squared_q8 = fixed_scale_q8(
        frame_scale_q8,
        frame_scale_q8);
    for (std::size_t index = 0; index < result.size(); ++index) {
        const std::int32_t velocity_step = fixed_scale_q8(
            collision_response_[index],
            frame_scale_q8);
        const std::int32_t acceleration_raw_step = fixed_scale_q8(
            motion_correction_[index],
            frame_scale_squared_q8);
        // FUN_004cacd0 performs the second arithmetic SAR 8 before
        // FUN_004cac90 divides the result by the literal 2.
        const std::int32_t acceleration_step = fixed_scale_q8(
            acceleration_raw_step,
            1) / 2;
        result[index] += velocity_step + acceleration_step;
    }
    return result;
}

GroundTurnResult PlayerState::update_ground_turn(
    const InputState& input,
    GroundTurnConfig config) noexcept {
    const GroundTurnResult result = GroundTurn::update(
        turn_accumulator_,
        input.held(movement_bit(MovementAction::Left)),
        input.held(movement_bit(MovementAction::Right)),
        config);
    GroundTurnResult resolved = result;
    // FUN_00493370's wide-limit branch removes a normalized copy of the
    // persistent response from the temporary +58 correction. The retail
    // denominator is (sqrt(dot) << 6) >> 12, i.e. the recovered metric's
    // high component.
    if (result.wide_profile) {
        const std::int32_t response_metric =
            retail_vector_speed_metric(collision_response_);
        if ((response_metric & 0xfffff000) != 0) {
            const std::int32_t denominator = response_metric >> 12;
            if (denominator != 0) {
                for (std::size_t index = 0;
                     index < motion_correction_.size();
                     ++index) {
                    motion_correction_[index] -=
                        collision_response_[index] / denominator;
                }
                resolved.response_normalized = true;
            }
        }
    }
    turn_accumulator_ = resolved.accumulator;
    turn_mirror_ = resolved.mirror;
    ground_turn_wide_profile_ = resolved.wide_profile;
    ground_turn_policy_changed_ = resolved.policy_changed;
    orientation_ = q12_apply_yaw(
        orientation_,
        GroundTurn::angle12(resolved.accumulator, config.frame_scale_q8));
    retail_basis_ = retail_basis_from_matrix(orientation_);
    return resolved;
}

GroundMotionResult PlayerState::apply_ground_motion(
    const GroundMotionInput& input) noexcept {
    return opentony::runtime::apply_ground_motion(
        motion_correction_,
        retail_basis_,
        input);
}

GroundAnimationResult PlayerState::update_ground_animation(
    const GroundAnimationInput& input) noexcept {
    const GroundAnimationResult result =
        opentony::runtime::update_ground_animation(input);
    animation_state_ = result.animation_state;
    animation_frame_ = result.animation_frame;
    return result;
}

GroundMotionThresholdResult PlayerState::update_ground_motion_threshold(
    const GroundMotionThresholdInput& input) noexcept {
    const GroundMotionThresholdResult result =
        opentony::runtime::update_ground_motion_threshold(
            ground_motion_threshold_,
            input);
    ground_motion_threshold_ = result.threshold;
    return result;
}

} // namespace opentony::runtime
