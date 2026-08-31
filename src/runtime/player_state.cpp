#include "player_state.hpp"

#include "ground_surface_response.hpp"
#include "tricks_bin.hpp"

#include <algorithm>
#include <cstdint>

namespace opentony::runtime {

namespace {

// FUN_004983f0 seeds the common-air orientation pivot with +0x46000 on the
// local Y axis. The two FUN_004e85a0 conversions around FUN_0049c330 then
// compensate the candidate position by old_pivot - new_pivot in Q16 space.
constexpr FixedPosition kAirUprightPivot{0, 0x46000, 0};

[[nodiscard]] std::int32_t arithmetic_shift_2(std::int32_t value) noexcept {
    if (value >= 0) {
        return value / 4;
    }
    return -(((-value) + 3) / 4);
}

[[nodiscard]] std::int32_t wrap32(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int32_t multiply32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return wrap32(static_cast<std::int64_t>(left) * right);
}

[[nodiscard]] std::int32_t divide32(
    std::int32_t numerator,
    std::int32_t denominator) noexcept {
    return denominator == 0 ? 0 : numerator / denominator;
}

} // namespace

void PlayerState::request_physics_state(
    std::int32_t state,
    std::uint32_t reason) noexcept {
    request_physics_state_from_basis(
        state,
        reason,
        retail_basis_.at_30f4);
}

void PlayerState::request_physics_state_from_basis(
    std::int32_t state,
    std::uint32_t reason,
    const FixedPosition& transition_basis) noexcept {
    // FUN_004900b0 has a transition-owned side effect when a non-state-2
    // handler enters raw state 2: it copies the current +30f4 basis vector to
    // +3118, scales each component by 0x32c8/0x1000, and negates it.  That
    // vector is consumed by the following state-2 surface response; keeping
    // the write on the transition preserves its causal boundary.
    if (state == 2 && physics_state_ != 2) {
        ground_surface_response_correction_[0] =
            -fixed_multiply_q12(transition_basis[0], 0x32c8);
        ground_surface_response_correction_[1] =
            fixed_multiply_q12(transition_basis[1], 0x32c8);
        ground_surface_response_correction_[2] =
            -fixed_multiply_q12(transition_basis[2], 0x32c8);
    }
    last_state_request_ = PhysicsStateRequest{
        physics_state_,
        state,
        reason,
        physics_state_ != state,
    };
    physics_state_ = state;
}

bool PlayerState::reset_surface_response_context() noexcept {
    const GroundSurfaceResponseResetResult reset =
        compute_ground_surface_response_reset(
            GroundSurfaceResponseResetInput{
                physics_state_,
                surface_response_action_context_2cc8_,
                surface_response_game_mode_,
            });
    if (!reset.applied) {
        return false;
    }
    surface_response_level_flag_3074_ = 0;
    surface_response_level_flag_3078_ = 0;
    surface_response_level_flag_307c_ = 0;
    surface_response_dispatch_active_3064_ = 1;
    surface_response_auxiliary_2cbc_ = 0;
    ground_surface_response_timer_ = 0xf;
    if (reset.clear_spin_phase_2e80) {
        surface_response_spin_phase_2e80_ = 0;
    }
    surface_response_action_gate_2c80_ = 0;
    surface_response_phase_refresh_gate_2c84_ = 0;
    // FUN_0048f5f0's mode-2/4 auxiliary-list clear is an external service;
    // the native player boundary records the deterministic predicate but does
    // not fabricate the service-owned list.
    return true;
}

void PlayerState::advance_surface_response_phase(
    std::int32_t frame_scale_q8) noexcept {
    const GroundSurfaceResponsePhaseResult phase =
        compute_ground_surface_response_phase(
            GroundSurfaceResponsePhaseInput{
                surface_response_phase_countdown_2c88_,
                surface_response_phase_2c8c_,
                surface_response_phase_rate_2c90_,
                frame_scale_q8,
            });
    surface_response_phase_countdown_2c88_ = phase.countdown_after;
    surface_response_phase_2c8c_ = phase.phase_after;
    surface_response_phase_rate_2c90_ = phase.rate_after;
}

void PlayerState::apply_ground_response_yaw(std::int32_t angle12) noexcept {
    // FUN_0049b500 has the same +0x2f64 early-out as the ordinary grounded
    // response phase.  The outer restart caller reaches it, but the writer
    // must remain a no-op while that control gate is set.
    if (angle12 == 0 || control_blocked()) {
        return;
    }
    const Q12Matrix3 saved_orientation = orientation_;
    orientation_ = q12_apply_ground_yaw(orientation_, angle12);
    retail_basis_ = retail_basis_from_matrix(orientation_);
    collision_response_ = q12_rotate_ground_velocity(
        collision_response_,
        saved_orientation,
        angle12);
}

GroundCollisionRecoveryExitResult
PlayerState::exit_ground_collision_recovery() noexcept {
    GroundCollisionRecoveryExitResult result{};
    if (physics_state_ == 2) {
        request_physics_state(1, 0x1605);
        result.state_request = last_state_request_;
        result.state_two_shortcut = true;
        return result;
    }

    request_physics_state(1, 0x160b);
    result.state_request = last_state_request_;
    result.ordinary_cleanup = true;

    // This is the exact store ordering in FUN_004956f0 after its ordinary
    // state request. The service call at FUN_0048f5f0 is reported by the
    // result hook; it does not belong to PlayerState's portable storage.
    collision_recovery_auxiliary_x_ = 0;
    collision_recovery_auxiliary_y_ = 0;
    collision_recovery_correction_gate_ = 0;
    collision_recovery_active_ = 1;
    collision_recovery_latch_ = 0;
    // FUN_0048f5f0 follows the state request on this branch. Keep its guard
    // and deterministic reset stores in the semantic helper rather than
    // reducing the call to an unconditional +0x2d90 write.
    static_cast<void>(reset_surface_response_context());

    if (!control_blocked_) {
        if (collision_recovery_stream_ != 0 && field_2dd4_ != 0) {
            result.action_stream_restart_requested = true;
            result.action_stream_reference = collision_recovery_stream_;
            collision_recovery_stream_ = 0;
            field_2dd4_ = 0;
            return result;
        }

        collision_recovery_block_ = 0;
        if (field_2dd8_ != 0) {
            result.animation_request_issued = true;
            result.animation = 0x1a;
            result.animation_reason = 0x1628;
            field_2dd4_ = 0;
            return result;
        }
        result.animation_request_issued = true;
        result.animation = 0x1b;
        result.animation_reason = 0x162a;
    }
    field_2dd4_ = 0;
    return result;
}

void PlayerState::apply_restart(
    FixedPosition position,
    std::uint32_t auxiliary,
    std::uint16_t auxiliary_word) noexcept {
    position_ = position;
    previous_position_ = position;
    older_position_ = position;
    outer_floor_reference_position_ = position;
    collision_response_ = {};
    motion_correction_ = {};
    air_motion_ = {};
    queued_motion_ = {};
    action_stream_active_ = false;
    action_stream_relative_ = 0;
    action_stream_cursor_ = 0;
    ground_physics_mode_ = 0;
    ground_motion_event_pending_ = false;
    ground_motion_event_reason_ = 0;
    ground_motion_animation_speed_ = 0;
    field_2dd4_ = 0;
    field_2dd8_ = 0;
    collision_recovery_stream_ = 0;
    collision_recovery_auxiliary_x_ = 0;
    collision_recovery_auxiliary_y_ = 0;
    collision_recovery_correction_gate_ = 0;
    collision_recovery_active_ = 0;
    collision_recovery_latch_ = 0;
    collision_recovery_block_ = 0;
    orientation_ = q12_restart_matrix(auxiliary);
    retail_basis_ = retail_basis_from_matrix(orientation_);
    ground_surface_recovery_target_ = retail_basis_.at_310c;
    ground_surface_recovery_base_ = retail_basis_.at_310c;
    ground_surface_response_correction_ = {};
    ground_surface_response_normal_ = retail_basis_.at_310c;
    ground_surface_response_mode_ = 0;
    ground_surface_response_correction_units_ = 0;
    ground_surface_response_timer_ = 0;
    ground_surface_response_phase_accumulator_ = 0;
    ground_surface_response_phase_count_ = 0;
    surface_response_phase_countdown_2c88_ = 0;
    surface_response_phase_2c8c_ = 0;
    surface_response_phase_rate_2c90_ = 0;
    surface_response_action_gate_2c80_ = 0;
    surface_response_phase_refresh_gate_2c84_ = 0;
    surface_response_spin_phase_2e80_ = 0;
    surface_response_dispatch_active_3064_ = 0;
    surface_response_level_flag_3074_ = 0;
    surface_response_level_flag_3078_ = 0;
    surface_response_level_flag_307c_ = 0;
    surface_response_auxiliary_2cbc_ = 0;
    ground_surface_recovery_progress_q11_ = 0;
    orientation_basis_normalization_pending_ = true;
    ground_turn_saved_orientation_ = orientation_;
    ground_turn_angle12_ = 0;
    ground_turn_saved_orientation_valid_ = false;
    restart_auxiliary_ = auxiliary;
    restart_auxiliary_word_ = auxiliary_word;
}

void PlayerState::update_collision_recovery_window(
    const InputState& input) noexcept {
    const std::int32_t heading = input.horizontal_axis();
    const bool cancel_window =
        (!input.held(movement_bit(MovementAction::Up))
         && heading > -0x29)
         || input.held(movement_bit(MovementAction::Left))
         || input.held(movement_bit(MovementAction::Right))
         || heading > 0x31
         || heading < -0x31;
    if (cancel_window) {
        collision_recovery_frame_ = 0;
        return;
    }
    if (collision_recovery_frame_ == 0) {
        collision_recovery_frame_ = frame_counter_;
    }
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
    const OllieImpulseRandom& release_random =
        input.early_release_random_available
        ? input.early_release_random
        : input.random;
    const std::int32_t cap =
        0xf - (release_random.first + release_random.second) / 0x14;
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
    const OllieImpulseRandom& cap_random =
        config.charge_cap_random_available
        ? config.charge_cap_random
        : config.impulse.random;
    result.cap = 0xf -
        (cap_random.first + cap_random.second) / 0x14;
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
            if (config.charge_cap_refresh_random_available) {
                result.cap = 0xf -
                    (config.charge_cap_refresh_random.first +
                     config.charge_cap_refresh_random.second) / 0x14;
            }
            ollie_.charge = result.cap;
            result.capped = true;
        }

        if ((physics_state_ == 0 || physics_state_ == 7 ||
             physics_state_ == 4 || physics_state_ == 5) &&
            ollie_.animation_gate == 0) {
            ollie_.latched = 1;
            result.latch_set = true;
            // The first held kick frame enters this block with no pending
            // release. Retail requests animation 8 through RunAnim with the
            // full 0..0x1a range and a 0x13 alternate endpoint, then marks
            // the kick pending below. Re-arming the request on later held
            // frames would replace the cursor every update.
            if (ollie_.pending == 0 &&
                (turn_accumulator_ < 0xa000
                 && turn_accumulator_ > -0xa000) &&
                ollie_.special_mode == 0) {
                result.animation_request_issued = true;
                result.animation_request_id = 8;
                result.animation_request_start = 0;
                result.animation_request_end = 0x1a;
                result.animation_request_alternate = 0x13;
            }
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
    if (config.early_release_random_available) {
        impulse.early_release_random = config.early_release_random;
        impulse.early_release_random_available = true;
    }
    ollie_.speed_metric = impulse.horizontal_speed_metric;
    ollie_.wallie = impulse.wallie ? 1 : 0;
    static_cast<void>(apply_ollie_impulse(impulse));
    ollie_.launch_frame = current_frame;
    result.launch_consumed = true;
    result.event = OlliePrePhysicsEvent::Launched;

    if (physics_state_ != 2) {
        // Retail requests the ordinary air state when the latched mode is
        // zero. A nonzero mode is the alternate launch path.
        const bool alternate = ollie_.mode != 0 && physics_state_ == 0;
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

void PlayerState::apply_air_gravity_acceleration(
    std::int32_t acceleration) noexcept {
    const std::uint32_t sum = static_cast<std::uint32_t>(motion_correction_[1])
        + static_cast<std::uint32_t>(acceleration);
    motion_correction_[1] = static_cast<std::int32_t>(sum);
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

std::optional<std::int32_t>
PlayerState::compute_in_air_orientation_angle(
    const InputState& input,
    std::int32_t frame_scale_q8,
    bool alignment_gate_open) const noexcept {
    // FUN_00498459 is only reached for the ordinary state-1 in-air path.
    if (physics_state_ != 1) {
        return std::nullopt;
    }

    // The two local action-bank slots at +0x10/+0x20 return from the
    // producer before any directional angle is written.
    if (input.action(0x0080).held || input.action(0x0020).held) {
        return std::nullopt;
    }

    const bool negative = input.held(movement_bit(MovementAction::Up))
        || input.vertical_axis() < -0x28;
    const bool positive = input.held(movement_bit(MovementAction::Down))
        || input.vertical_axis() > 0x28;
    std::int32_t angle = 0;
    if (negative) {
        angle = -fixed_scale_q8(20, frame_scale_q8);
    }
    // Retail's positive branch is later in the producer and therefore wins
    // when both directional slots happen to be active.
    if (positive) {
        angle = fixed_scale_q8(20, frame_scale_q8);
    }
    if (angle == 0) {
        return std::nullopt;
    }

    // Warehouse takes the +29b0 == 0 fallback, which compares the current
    // position against the published forward basis. The target-backed branch
    // remains outside this renderer-independent boundary.
    if (alignment_gate_open
        && fixed_dot_q12(retail_basis_.at_30f4, position_) > 0) {
        angle = -angle;
    }
    return angle;
}

void PlayerState::update_in_air_orientation_accumulator(
    const InputState& input,
    std::int32_t frame_scale_q8) noexcept {
    // This is the ordinary state-1/2 direction path in FUN_00493370. The
    // separate +2e80 action/spin branch is intentionally not folded into
    // this helper until its producer contract is recovered.
    if (physics_state_ != 1 && physics_state_ != 2) {
        return;
    }
    const bool left = input.held(movement_bit(MovementAction::Left));
    const bool right = input.held(movement_bit(MovementAction::Right));
    if (!left && !right) {
        return;
    }

    const std::int32_t step = fixed_scale_q8(0xa000, frame_scale_q8);
    std::int64_t next = turn_accumulator_;
    // As with the grounded producer, Left is the first branch in retail and
    // therefore wins if both direction records are active.
    next += left ? -static_cast<std::int64_t>(step) : step;
    next = std::clamp<std::int64_t>(next, -0xa0000, 0xa0000);
    turn_accumulator_ = static_cast<std::int32_t>(next);
}

FixedPosition PlayerState::apply_in_air_orientation_pivot(
    std::int32_t angle12) noexcept {
    const FixedPosition old_pivot = q12_transform_vector(
        orientation_,
        kAirUprightPivot);

    if (angle12 != 0) {
        // FUN_004e80e0 is called with [0, angle, 0] at this early site, which
        // produces a local X-axis rotation. FUN_004e3130 multiplies it on the
        // right of the current orientation.
        const Q12Matrix3 yaw = q12_yaw_matrix(angle12);
        Q12Matrix3 roll = q12_identity_matrix();
        roll.at(1, 1) = yaw.at(0, 0);
        roll.at(1, 2) = yaw.at(0, 2);
        roll.at(2, 1) = yaw.at(2, 0);
        roll.at(2, 2) = yaw.at(0, 0);
        orientation_ = q12_matrix_multiply(orientation_, roll);
    }

    // The retail branch tests the signed +0x2e60 short before applying the
    // old-pivot minus new-pivot compensation. In this matrix representation
    // that value is the local orientation row/column at (1, 1).
    if (orientation_.at(1, 1) > 0) {
        return {};
    }
    const FixedPosition new_pivot = q12_transform_vector(
        orientation_,
        kAirUprightPivot);
    FixedPosition pivot_delta{};
    for (std::size_t index = 0; index < pivot_delta.size(); ++index) {
        pivot_delta[index] = old_pivot[index] - new_pivot[index];
    }
    return pivot_delta;
}

void PlayerState::apply_upright_correction(
    const FixedPosition& global_up) noexcept {
    // FUN_0049c330 crosses the current forward axis with the shared up
    // vector, then takes the Q12 dot with the candidate air direction. The
    // small fixed roll is selected only outside the +/-0x29 deadband.
    // FUN_004e2ff0 applies the Q12 shift to each cross-product component
    // before FUN_004f5f90 computes the second Q12 dot. Keep this distinct
    // from raw fixed-point cross products used by other recovered helpers.
    const FixedPosition turn_axis = q12_cross(
        retail_basis_.at_30f4,
        global_up);
    const std::int32_t alignment = fixed_dot_q12(
        turn_axis,
        air_motion_);
    std::int32_t angle = 0;
    if (alignment > 0x29) {
        angle = 0xb;
    } else if (alignment < -0x29) {
        angle = -0xb;
    }

    if (angle != 0) {
        // FUN_004e80e0 builds this as the local Z-axis rotation and
        // FUN_004e3130 multiplies it on the right of the current matrix.
        // Reuse its exact truncated Q12 sine/cosine values for the roll
        // matrix rather than introducing a second trig implementation.
        const Q12Matrix3 yaw = q12_yaw_matrix(angle);
        Q12Matrix3 roll = q12_identity_matrix();
        roll.at(0, 0) = yaw.at(0, 0);
        roll.at(0, 1) = static_cast<std::int16_t>(-yaw.at(2, 0));
        roll.at(1, 0) = yaw.at(2, 0);
        roll.at(1, 1) = yaw.at(0, 0);
        orientation_ = q12_matrix_multiply(orientation_, roll);
    }

    // The retail helper always republishes the short matrix through
    // FUN_0049c7d0, including the deadband path.
    retail_basis_ = retail_basis_from_matrix(orientation_);
    air_motion_ = retail_basis_.at_310c;
    orientation_basis_normalization_pending_ = false;
}

void PlayerState::apply_air_orientation_turn(
    std::int32_t angle12) noexcept {
    if (angle12 == 0) {
        return;
    }
    // The in-air turn producer uses the same right-multiplied Y transform as
    // the recovered retail orientation path. This preserves the air axis
    // while publishing the rotated tangent basis through FUN_0049c7d0.
    orientation_ = q12_matrix_multiply(
        orientation_,
        q12_yaw_matrix(angle12));
    retail_basis_ = retail_basis_from_matrix(orientation_);
    orientation_basis_normalization_pending_ = false;
}

std::int32_t PlayerState::apply_orientation_turn_producer(
    AirOrientationTurnConfig config) noexcept {
    const std::int32_t producer_angle =
        compute_air_orientation_turn_angle(turn_accumulator_, config);
    if (producer_angle != 0) {
        // FUN_00497f40's matrix construction is the opposite signed angle
        // from the positive scalar carried by the accumulator formula.
        apply_air_orientation_turn(-producer_angle);
    }
    return producer_angle;
}

std::int32_t PlayerState::apply_in_air_orientation_turn(
    AirOrientationTurnConfig config) noexcept {
    return apply_orientation_turn_producer(config);
}

void PlayerState::normalize_orientation_basis() noexcept {
    if (!orientation_basis_normalization_pending_) {
        return;
    }
    retail_basis_.at_30f4 = q12_normalize(retail_basis_.at_30f4);
    retail_basis_.at_3100 = q12_normalize(retail_basis_.at_3100);
    retail_basis_.at_310c = q12_normalize(retail_basis_.at_310c);
    orientation_.at(0, 0) = static_cast<std::int16_t>(retail_basis_.at_3100[0]);
    orientation_.at(1, 0) = static_cast<std::int16_t>(retail_basis_.at_3100[1]);
    orientation_.at(2, 0) = static_cast<std::int16_t>(retail_basis_.at_3100[2]);
    orientation_.at(0, 1) = static_cast<std::int16_t>(retail_basis_.at_310c[0]);
    orientation_.at(1, 1) = static_cast<std::int16_t>(retail_basis_.at_310c[1]);
    orientation_.at(2, 1) = static_cast<std::int16_t>(retail_basis_.at_310c[2]);
    orientation_.at(0, 2) = static_cast<std::int16_t>(retail_basis_.at_30f4[0]);
    orientation_.at(1, 2) = static_cast<std::int16_t>(retail_basis_.at_30f4[1]);
    orientation_.at(2, 2) = static_cast<std::int16_t>(retail_basis_.at_30f4[2]);
    orientation_basis_normalization_pending_ = false;
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

AirActionControlResult PlayerState::apply_air_action_control(
    const InputState& input,
    AirActionControlConfig config) noexcept {
    config.kick_held = input.action(kKickActionBit).held;
    config.up_held = input.held(movement_bit(MovementAction::Up));
    config.down_held = input.held(movement_bit(MovementAction::Down));
    // The spin records are non-directional action records in the retail
    // action table and therefore use the low action-mask bits.
    config.spin_left_held = input.action(kSpinLeftActionBit).held;
    config.spin_right_held = input.action(kSpinRightActionBit).held;
    const AirActionControlResult result =
        opentony::runtime::apply_air_action_control(
            collision_response_,
            motion_correction_,
            retail_basis_,
            config);
    motion_correction_ = result.motion_correction;
    return result;
}

bool PlayerState::accept_air_contact(
    FixedPosition contact_position,
    FixedPosition contact_normal) noexcept {
    if (physics_state_ != 1 && physics_state_ != 3) {
        return false;
    }
    // begin_physics_frame() already captured the frame-start position in
    // +0xbc. Retail's accepted-contact write changes the live position and
    // leaves that history value intact for the frame-end observation.
    position_ = contact_position;
    // FUN_00498a66 publishes the accepted surface normal as the new air
    // direction and rebuilds the [forward, right, normal] basis before the
    // state request is observed by the outer frame.
    apply_orientation_recovery(contact_normal, true);
    // The following grounded frame continues recovery from this landing
    // surface; do not compare it against the pre-airborne support target.
    ground_surface_recovery_target_ = contact_normal;
    ground_surface_recovery_base_ = retail_basis_.at_310c;
    ground_surface_recovery_progress_q11_ = 0;
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

void PlayerState::apply_collision_transient_exit_orientation(
    const FixedPosition& collision_normal) noexcept {
    // FUN_00491780 first removes the collision-normal component from a local
    // copy of +0x4c and rejects the handoff when the remaining response is
    // too small. The normalized response becomes the negative forward axis.
    FixedPosition projected_response = collision_response_;
    static_cast<void>(remove_normal_component(
        projected_response,
        collision_normal));
    if (retail_vector_speed_metric(projected_response) <= 0x1e000) {
        return;
    }

    const FixedPosition response_direction = q12_normalize(
        projected_response);
    FixedPosition current_forward = retail_basis_.at_30f4;
    static_cast<void>(remove_normal_component(
        current_forward,
        collision_normal));
    current_forward = q12_normalize(current_forward);
    const std::int32_t response_forward_dot = fixed_dot_q12(
        response_direction,
        current_forward);

    // FUN_00491780 leaves the existing basis untouched when the projected
    // response is already decisively aligned or opposed to the current
    // forward axis.  The Warehouse state-two exit takes this path at the
    // observed -4064 dot product; only the narrow band reaches the
    // history-based rebuild below.
    if (response_forward_dot <= -0x666
        || response_forward_dot >= 0x666) {
        return;
    }

    // FUN_0046d970 is the only conditional side effect between the first
    // projection and the history-based rebuild. It can flip the tangent
    // axes and their handedness bit before the history-based state decision
    // continues; it does not replace the projected response.
    static_cast<void>(apply_collision_orientation_handedness());

    // FUN_004cabf0 receives +0xbc as its second vector and +0x2e00 as its
    // third, producing the displacement between the last two frame starts.
    FixedPosition history_delta{};
    for (std::size_t index = 0; index < history_delta.size(); ++index) {
        history_delta[index] = previous_position_[index]
            - older_position_[index];
    }
    const std::int64_t absolute_sum =
        static_cast<std::int64_t>(history_delta[0] < 0
            ? -static_cast<std::int64_t>(history_delta[0])
            : history_delta[0])
        + static_cast<std::int64_t>(history_delta[1] < 0
            ? -static_cast<std::int64_t>(history_delta[1])
            : history_delta[1])
        + static_cast<std::int64_t>(history_delta[2] < 0
            ? -static_cast<std::int64_t>(history_delta[2])
            : history_delta[2]);
    if (absolute_sum > 0x8000) {
        for (std::int32_t& component : history_delta) {
            // The retail instruction is x86 SAR, not C++ division: negative
            // components round toward negative infinity.
            component = component >= 0
                ? component / 16
                : -(((-component) + 15) / 16);
        }
    }
    const FixedPosition history_direction = q12_normalize(history_delta);

    // The cross helper consumes the signed-short publication boundary, just
    // as the two calls to FUN_004e2ff0 do in the retail routine.
    const auto short_vector = [](const FixedPosition& vector) noexcept {
        return FixedPosition{
            static_cast<std::int16_t>(vector[0]),
            static_cast<std::int16_t>(vector[1]),
            static_cast<std::int16_t>(vector[2]),
        };
    };
    const FixedPosition target = short_vector(collision_normal);
    retail_basis_.at_30f4 = short_vector({
        -history_direction[0],
        -history_direction[1],
        -history_direction[2],
    });
    retail_basis_.at_310c = target;
    retail_basis_.at_3100 = q12_cross(
        short_vector(retail_basis_.at_30f4),
        target);

    const auto publish_basis = [this]() noexcept {
        orientation_.at(0, 0) = static_cast<std::int16_t>(
            retail_basis_.at_3100[0]);
        orientation_.at(1, 0) = static_cast<std::int16_t>(
            retail_basis_.at_3100[1]);
        orientation_.at(2, 0) = static_cast<std::int16_t>(
            retail_basis_.at_3100[2]);
        orientation_.at(0, 1) = static_cast<std::int16_t>(
            retail_basis_.at_310c[0]);
        orientation_.at(1, 1) = static_cast<std::int16_t>(
            retail_basis_.at_310c[1]);
        orientation_.at(2, 1) = static_cast<std::int16_t>(
            retail_basis_.at_310c[2]);
        orientation_.at(0, 2) = static_cast<std::int16_t>(
            retail_basis_.at_30f4[0]);
        orientation_.at(1, 2) = static_cast<std::int16_t>(
            retail_basis_.at_30f4[1]);
        orientation_.at(2, 2) = static_cast<std::int16_t>(
            retail_basis_.at_30f4[2]);
    };
    publish_basis();

    retail_basis_.at_30f4 = q12_cross(
        target,
        short_vector(retail_basis_.at_3100));
    publish_basis();
}

bool PlayerState::apply_collision_orientation_handedness() noexcept {
    // FUN_0046d970 rejects animation 0x22 and either of its two object-owned
    // guard words before testing the response/forward alignment. The guards
    // are explicit raw inputs here and are clear for the qualified Warehouse
    // path.
    if (animation_state_ == 0x22
        || collision_orientation_gate_a_ != 0
        || collision_orientation_gate_b_ != 0) {
        return false;
    }
    if (fixed_dot_q12(collision_response_, retail_basis_.at_30f4) <= 0x5000) {
        return false;
    }

    for (std::size_t index = 0; index < 3; ++index) {
        retail_basis_.at_30f4[index] = -retail_basis_.at_30f4[index];
        retail_basis_.at_3100[index] = -retail_basis_.at_3100[index];
    }
    collision_orientation_handedness_ =
        !collision_orientation_handedness_;
    orientation_.at(0, 0) = static_cast<std::int16_t>(
        retail_basis_.at_3100[0]);
    orientation_.at(1, 0) = static_cast<std::int16_t>(
        retail_basis_.at_3100[1]);
    orientation_.at(2, 0) = static_cast<std::int16_t>(
        retail_basis_.at_3100[2]);
    orientation_.at(0, 2) = static_cast<std::int16_t>(
        retail_basis_.at_30f4[0]);
    orientation_.at(1, 2) = static_cast<std::int16_t>(
        retail_basis_.at_30f4[1]);
    orientation_.at(2, 2) = static_cast<std::int16_t>(
        retail_basis_.at_30f4[2]);
    orientation_basis_normalization_pending_ = false;
    return true;
}

bool PlayerState::apply_landing_orientation_handoff(
    const FixedPosition& collision_normal) noexcept {
    // 0x00497960 is a state-0 landing-only branch.  +0x2f64 is the
    // control-blocked gate and +0x30b8 is the raw physics-state field.
    if (control_blocked_ || physics_state_ == 2) {
        return false;
    }

    const FixedPosition short_normal{
        static_cast<std::int16_t>(collision_normal[0]),
        static_cast<std::int16_t>(collision_normal[1]),
        static_cast<std::int16_t>(collision_normal[2]),
    };
    const std::int32_t normal_alignment = fixed_dot_q12(
        retail_basis_.at_310c,
        short_normal);
    if (normal_alignment >= 2000
        || retail_basis_.at_310c[1] <= -2000
        || (collision_response_[1] <= 150000 && normal_alignment >= 200)) {
        return false;
    }

    // The PC swaps only the forward/air columns here. The lateral column is
    // preserved by the helper, and PublishOrientationBasis mirrors all three
    // integer vectors back to the packed short matrix.
    const FixedPosition old_air = retail_basis_.at_310c;
    const FixedPosition old_forward = retail_basis_.at_30f4;
    retail_basis_.at_30f4 = FixedPosition{
        -old_air[0], -old_air[1], -old_air[2]};
    retail_basis_.at_310c = old_forward;
    air_motion_ = retail_basis_.at_310c;
    orientation_.at(0, 0) = static_cast<std::int16_t>(
        retail_basis_.at_3100[0]);
    orientation_.at(1, 0) = static_cast<std::int16_t>(
        retail_basis_.at_3100[1]);
    orientation_.at(2, 0) = static_cast<std::int16_t>(
        retail_basis_.at_3100[2]);
    orientation_.at(0, 1) = static_cast<std::int16_t>(
        retail_basis_.at_310c[0]);
    orientation_.at(1, 1) = static_cast<std::int16_t>(
        retail_basis_.at_310c[1]);
    orientation_.at(2, 1) = static_cast<std::int16_t>(
        retail_basis_.at_310c[2]);
    orientation_.at(0, 2) = static_cast<std::int16_t>(
        retail_basis_.at_30f4[0]);
    orientation_.at(1, 2) = static_cast<std::int16_t>(
        retail_basis_.at_30f4[1]);
    orientation_.at(2, 2) = static_cast<std::int16_t>(
        retail_basis_.at_30f4[2]);
    orientation_basis_normalization_pending_ = false;

    // 0x00497960 calls the state request before the basis publication and
    // then writes (-short_normal * 10), clearing the Y response word.
    request_physics_state(3, 0);
    collision_response_[0] = -short_normal[0] * 10;
    collision_response_[1] = 0;
    collision_response_[2] = -short_normal[2] * 10;
    return true;
}

CollisionResponseResult PlayerState::apply_collision_response(
    const FixedPosition& surface_delta,
    std::int32_t bias_q12) {
    return opentony::runtime::apply_inward_response(
        collision_response_,
        surface_delta,
        bias_q12);
}

void PlayerState::apply_air_normal_recovery(
    const FixedPosition& collision_normal,
    const AirNormalRecoveryInput& input) noexcept {
    // The caller has already selected the in-air non-landing branch and
    // committed its contact-plus-normal position. FUN_0049d080 resets the
    // recovery target to world up before FUN_0049bad0 handles the contact.
    apply_orientation_recovery(FixedPosition{0, -0x1000, 0}, true);
    static_cast<void>(apply_collision_response(collision_normal, 0xcd));
    static_cast<void>(apply_collision_orientation(collision_normal, 0x19));

    const auto quarter = [](std::int32_t value) noexcept {
        return arithmetic_shift_2(value);
    };
    const FixedPosition short_normal{
        static_cast<std::int16_t>(collision_normal[0]),
        static_cast<std::int16_t>(collision_normal[1]),
        static_cast<std::int16_t>(collision_normal[2]),
    };
    for (std::size_t index = 0; index < collision_response_.size(); ++index) {
        collision_response_[index] += quarter(short_normal[index]);
    }
    if (input.gate_random == 0) {
        collision_response_[0] += input.x_random - 0x2000;
        collision_response_[2] += input.z_random - 0x2000;
    }
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

std::int32_t PlayerState::compute_ground_surface_response_delta(
    const GroundSurfaceResponseInput& input) noexcept {
    // FUN_0049c060 uses the response speed metric for its cap. Its surface
    // correction path consumes the same magnitude helper before the caller's
    // fixed-point scale, so this value is the Q12 magnitude's high component.
    ground_surface_response_correction_units_ = 0;
    if (control_blocked_) {
        return 0;
    }
    std::int32_t response_metric = retail_vector_speed_metric(
        collision_response_);
    const std::int32_t correction_magnitude = retail_vector_magnitude_q12(
        ground_surface_response_correction_);
    const auto randomized_speed_limit = [](std::int32_t roll) noexcept {
        return divide32(
            multiply32(roll + 0x186, 0x2d000),
            0x118);
    };

    const std::int32_t cap = randomized_speed_limit(input.cap_random);
    if (cap < response_metric) {
        response_metric = input.capped_response_random_available
            ? randomized_speed_limit(input.capped_response_random)
            : cap;
    }
    const std::int32_t randomized_target = randomized_speed_limit(
        input.target_random);
    const std::int32_t correction_factor = divide32(
        multiply32(
            multiply32(
                divide32(
                    multiply32(correction_magnitude, 0xc80),
                    13000),
                correction_magnitude),
            0x40),
        13000);
    const std::int32_t numerator = multiply32(
        randomized_target + 0x28 * 0x1000 - response_metric,
        correction_factor);
    const std::int32_t response_delta = divide32(
        numerator,
        randomized_speed_limit(input.denominator_random));
    ground_surface_response_correction_units_ = response_delta;
    if (response_delta == 0) {
        ground_surface_response_mode_ = 0;
        return 0;
    }

    // FUN_0049c060 compares the current tangent against the cross-product
    // heading and latches the sign until the surface response crosses the
    // opposite threshold. The cross order is the retail scratch order.
    const FixedPosition direction = q12_normalize(
        ground_surface_response_correction_);
    FixedPosition tangent = retail_basis_.at_30f4;
    static_cast<void>(remove_normal_component(
        tangent,
        ground_surface_response_normal_));
    const FixedPosition heading = q12_cross(
        ground_surface_response_normal_,
        direction);
    const std::int32_t heading_dot = fixed_dot_q12(tangent, heading);

    std::int32_t signed_delta = 0;
    if (heading_dot < 0x2a) {
        if (heading_dot < -0x29) {
            if (ground_surface_response_mode_ != 1) {
                signed_delta = response_delta;
                ground_surface_response_mode_ = 2;
            }
        } else {
            ground_surface_response_mode_ = 0;
        }
    } else if (ground_surface_response_mode_ != 2) {
        signed_delta = -response_delta;
        ground_surface_response_mode_ = 1;
    }
    if (signed_delta == 0) {
        return 0;
    }

    ground_surface_response_correction_units_ = signed_delta;
    return signed_delta;
}

std::int32_t PlayerState::apply_ground_surface_response(
    const GroundSurfaceResponseInput& input,
    std::int32_t frame_scale_q8,
    bool rotate_collision_response) noexcept {
    const std::int32_t signed_delta =
        compute_ground_surface_response_delta(input);
    const std::int32_t angle12 = fixed_scale_q8(
        signed_delta,
        frame_scale_q8);
    if (angle12 != 0) {
        const Q12Matrix3 old_orientation = orientation_;
        orientation_ = q12_apply_ground_yaw(orientation_, angle12);
        retail_basis_ = retail_basis_from_matrix(orientation_);
        // FUN_0049b500's second phase is caller-selected. The state-0
        // grounded path rotates the response vector; the transient state-2
        // call only publishes the orientation/basis and passes param_3=0.
        if (rotate_collision_response) {
            collision_response_ = q12_rotate_ground_velocity(
                collision_response_,
                old_orientation,
                angle12);
        }
    }
    return angle12;
}

GroundSurfaceResponseStepResult
PlayerState::apply_ground_surface_response_step(
    GroundSurfaceResponseStepInput input,
    const std::optional<GroundSurfaceResponseInput>& response_input) noexcept {
    input.physics_state = physics_state_;
    input.ground_update_state = ground_update_state_;
    input.turn_accumulator_q12 = turn_accumulator_;
    input.surface_response_timer = ground_surface_response_timer_;

    GroundSurfaceResponseStepResult result =
        compute_ground_surface_response_step(input);
    ground_surface_response_correction_units_ = 0;
    bool response_input_missing = false;
    if (result.response_call_requested) {
        if (response_input.has_value()) {
            ground_surface_response_correction_units_ =
                compute_ground_surface_response_delta(*response_input);
        } else {
            response_input_missing = true;
        }
        input.response_correction_units =
            ground_surface_response_correction_units_;
        result = compute_ground_surface_response_step(input);
    }
    result.response_input_missing = response_input_missing;

    ground_surface_response_timer_ = result.timer_after;
    if (result.special_ground_phase) {
        if (result.angle12 != 0) {
            // FUN_00496360 passes zero as the response phase for state 2 and
            // for state 1 with +0x30c4 set. The matrix writer still receives
            // the positive scalar with the same opposite signed angle as the
            // ordinary in-air turn producer.
            apply_air_orientation_turn(-result.angle12);
            result.orientation_written = true;
        }
    } else {
        const bool had_turn_phase = ground_turn_saved_orientation_valid_;
        apply_ground_turn_velocity_phase(
            result.response_correction_units,
            input.frame_scale_q8);
        result.orientation_written = had_turn_phase
            && result.angle12 != 0;
    }

    if (field_2dd4_ == 0) {
        ground_surface_response_phase_accumulator_ = wrap32(
            static_cast<std::int64_t>(
                ground_surface_response_phase_accumulator_)
            + result.angle12);
        result.phase_accumulator_written = true;
        if (ground_surface_response_phase_accumulator_ >= 0x800) {
            ground_surface_response_phase_accumulator_ -= 0x800;
            ++ground_surface_response_phase_count_;
            result.phase_accumulator_wrapped = true;
            result.phase_refresh_service_requested =
                !input.phase_refresh_blocked_2c84;
        } else if (ground_surface_response_phase_accumulator_ <= -0x800) {
            ground_surface_response_phase_accumulator_ += 0x800;
            ++ground_surface_response_phase_count_;
            result.phase_accumulator_wrapped = true;
            result.phase_refresh_service_requested =
                !input.phase_refresh_blocked_2c84;
        }
    }
    return result;
}

void PlayerState::apply_ground_leave_air_response(
    const FixedPosition& collision_normal) noexcept {
    // FUN_00496550 computes local_108 as the response magnitude multiplied
    // by 0x40, then uses (local_108 + sign-adjustment) >> 2 as the scale for
    // the signed-short collision normal. The normal is published as shorts
    // by the retail collision service, so retain that boundary here.
    const std::int32_t response_metric = retail_vector_speed_metric(
        collision_response_);
    const std::int32_t normal_scale =
        (response_metric + ((response_metric >> 31) & 3)) >> 2;
    const FixedPosition normal_short{
        static_cast<std::int16_t>(collision_normal[0]),
        static_cast<std::int16_t>(collision_normal[1]),
        static_cast<std::int16_t>(collision_normal[2]),
    };
    collision_response_[0] -= fixed_multiply_q12(
        normal_short[0], normal_scale);
    collision_response_[1] /= 2;
    collision_response_[2] -= fixed_multiply_q12(
        normal_short[2], normal_scale);
}

void PlayerState::apply_orientation_recovery(
    const FixedPosition& surface_normal,
    bool recovery_complete) noexcept {
    // FUN_0049d080 uses the full-width recovery base, which is the currently
    // published +310c axis at this renderer-independent state boundary. Its
    // non-terminal progress branch is a signed x86 SAR by two.
    FixedPosition target = surface_normal;
    if (!recovery_complete) {
        target = {};
        for (std::size_t index = 0; index < target.size(); ++index) {
            target[index] = ground_surface_recovery_base_[index]
                + arithmetic_shift_2(
                    surface_normal[index] - ground_surface_recovery_base_[index]);
        }
    }
    target = q12_normalize(target);

    // The retail object source is the signed-short current forward column
    // (+2e5c, +2e62, +2e68). Keep the short publication boundary between the
    // two cross products, as FUN_0049d080 does.
    const FixedPosition current_forward{
        orientation_.at(0, 2),
        orientation_.at(1, 2),
        orientation_.at(2, 2),
    };
    const FixedPosition target_short{
        static_cast<std::int16_t>(target[0]),
        static_cast<std::int16_t>(target[1]),
        static_cast<std::int16_t>(target[2]),
    };
    const FixedPosition right_cross_raw = q12_cross(
        current_forward,
        target_short);
    const FixedPosition right = q12_normalize(right_cross_raw);
    // FUN_0049d080 normalizes the first cross product, but publishes the
    // second cross product directly.  The raw second cross is already close
    // to Q12 length; normalizing it again changes the low bits on sloped
    // surfaces and consequently changes the lateral response projection.
    const FixedPosition right_short{
        static_cast<std::int16_t>(right[0]),
        static_cast<std::int16_t>(right[1]),
        static_cast<std::int16_t>(right[2]),
    };
    const FixedPosition forward = q12_cross(target_short, right_short);

    air_motion_ = target;
    retail_basis_ = RetailBasis{forward, right, target};
    orientation_.at(0, 0) = static_cast<std::int16_t>(right[0]);
    orientation_.at(1, 0) = static_cast<std::int16_t>(right[1]);
    orientation_.at(2, 0) = static_cast<std::int16_t>(right[2]);
    orientation_.at(0, 1) = static_cast<std::int16_t>(target[0]);
    orientation_.at(1, 1) = static_cast<std::int16_t>(target[1]);
    orientation_.at(2, 1) = static_cast<std::int16_t>(target[2]);
    orientation_.at(0, 2) = static_cast<std::int16_t>(forward[0]);
    orientation_.at(1, 2) = static_cast<std::int16_t>(forward[1]);
    orientation_.at(2, 2) = static_cast<std::int16_t>(forward[2]);
    orientation_basis_normalization_pending_ = false;
}

void PlayerState::seed_ground_surface_recovery(
    const FixedPosition& surface_normal) noexcept {
    ground_surface_recovery_target_ = surface_normal;
    // The landing common tail stores the accepted normal in both the target
    // shorts and the persistent +0x3134/+0x3138/+0x313c interpolation base.
    // 0x0049d080 therefore starts the next recovery sample from this target,
    // not from whatever air axis preceded the landing handoff.
    ground_surface_recovery_base_ = surface_normal;
    ground_surface_recovery_progress_q11_ = 0;
    ground_surface_recovery_update_frame_ = frame_counter_;
}

bool PlayerState::update_ground_surface_recovery(
    const FixedPosition& surface_normal,
    std::int32_t delta_q11) noexcept {
    const bool target_changed =
        ground_surface_recovery_target_ != surface_normal;
    if (target_changed) {
        ground_surface_recovery_target_ = surface_normal;
        ground_surface_recovery_progress_q11_ = 0;
    } else {
        // FUN_00496550 can present the same surface twice in one physics
        // frame: the movement hit is followed by the recovery sweep. Retail
        // advances the timer once per frame, not once per call, but both
        // calls still enter FUN_0049d080 with the current progress value.
        if (ground_surface_recovery_update_frame_ != frame_counter_) {
            // DAT_0056a93c is a signed Q11 add in the retail object. Convert
            // via unsigned arithmetic so wraparound remains the x86 result.
            ground_surface_recovery_progress_q11_ = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(ground_surface_recovery_progress_q11_)
                + static_cast<std::uint32_t>(delta_q11));
            ground_surface_recovery_update_frame_ = frame_counter_;
        }
    }
    // FUN_0049d080 is still entered by the caller after the progress update,
    // but its own gate accepts only progress <= 0x18000. At the first sample
    // above that limit it leaves the already-published basis and recovery base
    // untouched while retaining the accumulated progress value.
    if (ground_surface_recovery_progress_q11_ >= 0x18001) {
        return false;
    }
    apply_orientation_recovery(
        surface_normal,
        ground_surface_recovery_progress_q11_ == 0x18000);
    ground_surface_recovery_base_ = retail_basis_.at_310c;
    return target_changed;
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
    std::int32_t frame_scale_q8,
    std::int32_t forward_scale,
    bool apply_response_basis) noexcept {
    const auto project = [](const FixedPosition& vector,
                            const FixedPosition& basis) {
        const std::int32_t dot = fixed_dot_q12(vector, basis);
        return FixedPosition{
            fixed_multiply_q12(dot, basis[0]),
            fixed_multiply_q12(dot, basis[1]),
            fixed_multiply_q12(dot, basis[2]),
        };
    };

    if (apply_response_basis) {
        const FixedPosition lateral = project(
            collision_response_,
            retail_basis_.at_3100);
        // This compatibility response pass remains active for the ordinary
        // flat-contact path. The recovered sloped-surface path uses the
        // retail velocity phase instead and must not receive it twice.
        for (std::size_t index = 0; index < collision_response_.size(); ++index) {
            collision_response_[index] -= lateral[index];
        }
    }

    if (apply_forward_term) {
        const FixedPosition forward = project(
            collision_response_,
            retail_basis_.at_30f4);
        for (std::size_t index = 0; index < motion_correction_.size(); ++index) {
            // The retail tail performs Q12 multiply by the literal 8, then
            // multiplies by DAT_0056865c and shifts by eight.
            const std::int32_t scaled_forward = fixed_multiply_q12(
                forward_scale,
                forward[index]);
            motion_correction_[index] -= fixed_scale_q8(
                scaled_forward,
                frame_scale_q8);
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
        // The squared Q8 scale has already supplied the two frame-scale
        // factors: the first fixed-scale operation accounts for the final
        // arithmetic SAR 8 in FUN_004cacd0. The remaining operation is the
        // literal divide-by-two in FUN_004cac90.
        const std::int32_t acceleration_step = acceleration_raw_step / 2;
        result[index] += velocity_step + acceleration_step;
    }
    return result;
}

GroundTurnResult PlayerState::update_ground_turn(
    const InputState& input,
    GroundTurnConfig config) noexcept {
    // Retail stores the pre-0x0049b500 matrix in +0x2e38 after the grounded
    // accumulator producer. B010 runs before 0x00496360 publishes the new
    // matrix, so preserve the old basis for both B010 and the later response
    // phase. The matrix publication is deliberately deferred to
    // apply_ground_turn_velocity_phase().
    ground_turn_saved_orientation_ = orientation_;
    ground_turn_saved_orientation_valid_ = true;
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
    ground_turn_angle12_ = GroundTurn::angle12(
        resolved.accumulator,
        config.frame_scale_q8);
    return resolved;
}

void PlayerState::apply_ground_turn_velocity_phase(
    std::int32_t additional_turn_units,
    std::int32_t frame_scale_q8) noexcept {
    if (!ground_turn_saved_orientation_valid_) {
        return;
    }
    std::int32_t angle12 = ground_turn_angle12_;
    if (additional_turn_units != 0) {
        angle12 = fixed_scale_q8(
            wrap32(
                static_cast<std::int64_t>(turn_accumulator_ >> 12)
                + additional_turn_units),
            frame_scale_q8);
    }
    if (angle12 == 0) {
        ground_turn_saved_orientation_valid_ = false;
        return;
    }
    // 0x00496360 calls 0x0049b500 after the pre-query ground work and before
    // the ordinary collision line. The first phase multiplies the current
    // orientation; that matters when 0x00496550 has just run 0x0046d970 and
    // flipped the tangent handedness. The second phase below still uses the
    // saved pre-turn matrix, as the retail helper does for its response
    // rescale.
    orientation_ = q12_apply_ground_yaw(
        orientation_,
        angle12);
    retail_basis_ = retail_basis_from_matrix(orientation_);
    collision_response_ = q12_rotate_ground_velocity(
        collision_response_,
        ground_turn_saved_orientation_,
        angle12);
    ground_turn_saved_orientation_valid_ = false;
}

GroundMotionResult PlayerState::apply_ground_motion(
    const GroundMotionInput& input) noexcept {
    const GroundMotionResult result = opentony::runtime::apply_ground_motion(
        motion_correction_,
        retail_basis_,
        input);
    if (result.cooldown_written) {
        ground_motion_cooldown_ = result.cooldown_value;
    }
    if (result.threshold_written) {
        ground_motion_threshold_ = result.threshold_value;
    }
    if (result.pending_animation_event_written) {
        ground_motion_event_pending_ = result.pending_animation_event;
    }
    if (result.event_reason != 0) {
        ground_motion_event_reason_ = result.event_reason;
    }
    if (result.animation_event_written) {
        ground_motion_event_parameter_ = result.animation_event_parameter;
    }
    if (result.animation_speed != 0) {
        ground_motion_animation_speed_ = result.animation_speed;
    }
    return result;
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
