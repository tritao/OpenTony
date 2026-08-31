#pragma once

#include "air_contact.hpp"
#include "position_commit.hpp"
#include "fixed_math.hpp"
#include "collision_response.hpp"
#include "fixed_matrix.hpp"
#include "ground_turn.hpp"
#include "ground_brake.hpp"
#include "input_state.hpp"
#include "ollie.hpp"
#include "platform_response.hpp"
#include "velocity_projection.hpp"
#include "velocity_damping.hpp"
#include "air_motion.hpp"
#include "queued_motion.hpp"
#include "action_commands.hpp"
#include "action_profile.hpp"
#include "action_sequence.hpp"
#include "ground_motion.hpp"
#include "ground_animation.hpp"
#include "ground_physics.hpp"
#include "ground_motion_threshold.hpp"
#include "ground_surface_response.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace opentony::assets {
struct TricksBinView;
}

namespace opentony::runtime {

constexpr std::uint32_t kGroundStopReason = 0x2c56;

struct PhysicsStateRequest {
    std::int32_t from = 0;
    std::int32_t to = 0;
    std::uint32_t reason = 0;
    bool changed = false;

    friend bool operator==(
        const PhysicsStateRequest&,
        const PhysicsStateRequest&) = default;
};

struct GroundCollisionRecoveryExitResult final {
    PhysicsStateRequest state_request{};
    bool state_two_shortcut{};
    bool ordinary_cleanup{};
    bool action_stream_restart_requested{};
    std::int32_t action_stream_reference{};
    bool animation_request_issued{};
    std::uint32_t animation{};
    std::uint32_t animation_reason{};
};

// Raw skater fields written by the level-script dispatcher. The retail
// opcodes 0xa3/0xb1 write the skater object at +0x3198/+0x319c. Their
// consumer-owned meanings are not established yet, so keep the offset names
// and presence bits at this boundary instead of assigning gameplay semantics.
struct PlayerScriptSkaterFields final {
    bool has_3198{};
    std::uint32_t field_3198{};
    bool has_319c{};
    std::uint32_t field_319c{};

    friend bool operator==(
        const PlayerScriptSkaterFields&,
        const PlayerScriptSkaterFields&) = default;
};

// Raw, renderer-independent player boundary recovered from the retail skater
// object. The fields intentionally retain their fixed-point representation and
// raw physics-state values until the state-specific producers are confirmed.
class PlayerState final {
public:
    explicit PlayerState(FixedPosition position = {}) noexcept
        : position_(position),
          previous_position_(position),
          outer_floor_reference_position_(position),
          orientation_(q12_identity_matrix()),
          retail_basis_(retail_basis_from_matrix(orientation_)) {}

    [[nodiscard]] const FixedPosition& position() const noexcept {
        return position_;
    }
    [[nodiscard]] const FixedPosition& previous_position() const noexcept {
        return previous_position_;
    }
    [[nodiscard]] const FixedPosition& older_position() const noexcept {
        return older_position_;
    }
    [[nodiscard]] const FixedPosition& outer_floor_reference_position() const noexcept {
        return outer_floor_reference_position_;
    }
    [[nodiscard]] bool outer_floor_recovery_blocked() const noexcept {
        return outer_floor_recovery_blocked_;
    }
    [[nodiscard]] std::int32_t outer_floor_distance() const noexcept {
        return outer_floor_distance_;
    }
    [[nodiscard]] const FixedPosition& collision_response() const noexcept {
        return collision_response_;
    }
    [[nodiscard]] const FixedPosition& motion_correction() const noexcept {
        return motion_correction_;
    }
    [[nodiscard]] const FixedPosition& air_motion() const noexcept {
        return air_motion_;
    }
    [[nodiscard]] std::int32_t physics_state() const noexcept {
        return physics_state_;
    }
    [[nodiscard]] std::int32_t ground_update_state() const noexcept {
        return ground_update_state_;
    }
    [[nodiscard]] std::int32_t ground_physics_mode() const noexcept {
        return ground_physics_mode_;
    }
    [[nodiscard]] std::int32_t turn_accumulator() const noexcept {
        return turn_accumulator_;
    }
    [[nodiscard]] std::int32_t turn_mirror() const noexcept {
        return turn_mirror_;
    }
    [[nodiscard]] bool ground_turn_wide_profile() const noexcept {
        return ground_turn_wide_profile_;
    }
    [[nodiscard]] bool ground_turn_policy_changed() const noexcept {
        return ground_turn_policy_changed_;
    }
    [[nodiscard]] bool ground_motion_correction_gate_open() const noexcept {
        return !ground_turn_wide_profile_ || !ground_turn_policy_changed_;
    }
    // FUN_004904d0 leaves +0x2f64 set while the post-landing control-blocked
    // window is active.  Grounded collision tails and the later reset use
    // this raw gate independently of the physics-state enum.
    [[nodiscard]] bool control_blocked() const noexcept {
        return control_blocked_;
    }
    void set_control_blocked(bool blocked) noexcept {
        control_blocked_ = blocked;
    }
    void set_control_blocked_velocity_decay_divisor(
        std::int32_t divisor) noexcept {
        control_blocked_velocity_decay_divisor_ = divisor;
    }
    // The deterministic part of FUN_0049d8a0 clears the lateral transient
    // correction while preserving its Y component for the outer velocity
    // integration.  The remaining launch/animation side effects stay with
    // their existing owners.
    void apply_control_blocked_reset(
        std::int32_t frame_scale_q8 = 0x100) noexcept {
        motion_correction_[0] = 0;
        motion_correction_[2] = 0;
        if (collision_response_[1] < 0) {
            collision_response_[1] = 0;
        }
        if (control_blocked_velocity_decay_divisor_ != 0) {
            for (std::size_t index = 0;
                 index < collision_response_.size();
                 ++index) {
                const std::int32_t quotient = collision_response_[index]
                    / control_blocked_velocity_decay_divisor_;
                collision_response_[index] -= static_cast<std::int32_t>(
                    (static_cast<std::int64_t>(quotient) * frame_scale_q8) / 0x100);
            }
        }
    }
    [[nodiscard]] std::int32_t ground_motion_cooldown() const noexcept {
        return ground_motion_cooldown_;
    }
    [[nodiscard]] std::uint16_t animation_state() const noexcept {
        return animation_state_;
    }
    [[nodiscard]] std::int16_t animation_frame() const noexcept {
        return animation_frame_;
    }
    // Raw skater fields consumed by the level-event update.  Keep the
    // executable offsets at this boundary until the surrounding score/stat
    // service is reconstructed.
    [[nodiscard]] std::int32_t level_event_field_2cdc() const noexcept {
        return field_2cdc_;
    }
    [[nodiscard]] std::int32_t level_event_field_2dd4() const noexcept {
        return field_2dd4_;
    }
    [[nodiscard]] std::uint8_t level_event_field_107() const noexcept {
        return field_107_;
    }
    [[nodiscard]] std::int32_t level_event_field_2a8() const noexcept {
        return field_2a8_;
    }
    [[nodiscard]] std::int32_t level_event_field_16c() const noexcept {
        return field_16c_;
    }
    [[nodiscard]] std::uint32_t level_event_animation_requests() const noexcept {
        return level_event_animation_requests_;
    }
    [[nodiscard]] std::uint32_t last_level_event_animation() const noexcept {
        return last_level_event_animation_;
    }
    [[nodiscard]] std::int32_t ground_motion_threshold() const noexcept {
        return ground_motion_threshold_;
    }
    void set_ground_motion_threshold(std::int32_t threshold) noexcept {
        ground_motion_threshold_ = threshold;
    }
    [[nodiscard]] bool ground_motion_event_pending() const noexcept {
        return ground_motion_event_pending_;
    }
    [[nodiscard]] std::uint32_t ground_motion_event_reason() const noexcept {
        return ground_motion_event_reason_;
    }
    [[nodiscard]] std::uint32_t ground_motion_event_parameter() const noexcept {
        return ground_motion_event_parameter_;
    }
    [[nodiscard]] std::int32_t ground_motion_animation_speed() const noexcept {
        return ground_motion_animation_speed_;
    }
    [[nodiscard]] std::int32_t ground_motion_speed_metric() const noexcept {
        return retail_vector_speed_metric(collision_response_);
    }
    [[nodiscard]] const OllieBookkeeping& ollie() const noexcept {
        return ollie_;
    }
    [[nodiscard]] const PhysicsStateRequest& last_state_request() const noexcept {
        return last_state_request_;
    }
    [[nodiscard]] std::uint32_t restart_auxiliary() const noexcept {
        return restart_auxiliary_;
    }
    [[nodiscard]] std::uint16_t restart_auxiliary_word() const noexcept {
        return restart_auxiliary_word_;
    }
    [[nodiscard]] const PlayerScriptSkaterFields& script_skater_fields() const noexcept {
        return script_skater_fields_;
    }
    [[nodiscard]] std::int32_t frame_counter() const noexcept {
        return frame_counter_;
    }
    [[nodiscard]] std::int32_t collision_recovery_frame() const noexcept {
        return collision_recovery_frame_;
    }
    // Raw +0x2d90 latch consumed by FUN_00496360. Its producer is a
    // recovery/action service; it is not a per-frame random input.
    [[nodiscard]] std::int32_t ground_surface_response_timer() const noexcept {
        return ground_surface_response_timer_;
    }
    [[nodiscard]] std::int32_t ground_surface_response_phase_accumulator() const noexcept {
        return ground_surface_response_phase_accumulator_;
    }
    [[nodiscard]] std::int32_t ground_surface_response_phase_count() const noexcept {
        return ground_surface_response_phase_count_;
    }
    [[nodiscard]] bool surface_response_phase_active() const noexcept {
        return surface_response_phase_countdown_2c88_ != 0;
    }
    [[nodiscard]] bool surface_response_spin_active() const noexcept {
        return surface_response_spin_phase_2e80_ != 0;
    }
    [[nodiscard]] bool surface_response_action_gate_active() const noexcept {
        return surface_response_action_gate_2c80_ != 0;
    }
    [[nodiscard]] bool surface_response_phase_refresh_blocked() const noexcept {
        return surface_response_phase_refresh_gate_2c84_ != 0;
    }
    [[nodiscard]] bool reset_surface_response_context() noexcept;
    void advance_surface_response_phase(
        std::int32_t frame_scale_q8 = 0x100) noexcept;
    void set_surface_response_action_context(std::int32_t value) noexcept {
        surface_response_action_context_2cc8_ = value;
    }
    void set_surface_response_game_mode(std::int32_t value) noexcept {
        surface_response_game_mode_ = value;
    }
    void set_surface_response_phase(
        std::int32_t countdown,
        std::int32_t phase,
        std::int32_t rate) noexcept {
        surface_response_phase_countdown_2c88_ = countdown;
        surface_response_phase_2c8c_ = phase;
        surface_response_phase_rate_2c90_ = rate;
    }
    void set_surface_response_spin_phase(std::int32_t value) noexcept {
        surface_response_spin_phase_2e80_ = value;
    }
    void set_surface_response_action_gate(std::int32_t value) noexcept {
        surface_response_action_gate_2c80_ = value;
    }
    void set_surface_response_phase_refresh_gate(std::int32_t value) noexcept {
        surface_response_phase_refresh_gate_2c84_ = value;
    }
    [[nodiscard]] std::int32_t surface_response_dispatch_active() const noexcept {
        return surface_response_dispatch_active_3064_;
    }
    // Mirror the frame-start +0x2d8c recovery-window gate. The window is
    // armed on a stable UP/heading input and cleared by a cancelled heading
    // or by releasing UP.
    void update_collision_recovery_window(const InputState& input) noexcept;
    [[nodiscard]] const Q12Matrix3& orientation() const noexcept {
        return orientation_;
    }
    [[nodiscard]] const RetailBasis& retail_basis() const noexcept {
        return retail_basis_;
    }
    // The short normal at +0x80 is the current orientation-recovery target
    // consumed by the grounded support tail.
    [[nodiscard]] const FixedPosition& ground_surface_recovery_target() const noexcept {
        return ground_surface_recovery_target_;
    }
    [[nodiscard]] const QueuedMotionState& queued_motion() const noexcept {
        return queued_motion_;
    }
    [[nodiscard]] const ActionCommandRuntimeState& action_command_state() const noexcept {
        return action_command_state_;
    }
    [[nodiscard]] bool action_stream_active() const noexcept {
        return action_stream_active_;
    }
    [[nodiscard]] std::int16_t action_stream_relative() const noexcept {
        return action_stream_relative_;
    }
    [[nodiscard]] std::size_t action_stream_cursor() const noexcept {
        return action_stream_cursor_;
    }
    [[nodiscard]] const RetailActionHistory& action_history() const noexcept {
        return action_history_;
    }

    void set_position(FixedPosition position) noexcept { position_ = position; }
    void set_previous_position(FixedPosition position) noexcept {
        previous_position_ = position;
        older_position_ = position;
    }
    // FUN_00490730 writes +0xbc without touching the preceding +0x2e00
    // history slot. Keep that raw store distinct from the public reset-style
    // setter above.
    void set_previous_position_only(FixedPosition position) noexcept {
        previous_position_ = position;
    }
    void set_outer_floor_reference_position(FixedPosition position) noexcept {
        outer_floor_reference_position_ = position;
    }
    void set_outer_floor_recovery_blocked(bool blocked) noexcept {
        outer_floor_recovery_blocked_ = blocked;
    }
    void set_outer_floor_distance(std::int32_t distance) noexcept {
        outer_floor_distance_ = distance;
    }
    void set_collision_response(FixedPosition response) noexcept {
        collision_response_ = response;
    }
    // Raw gates consumed by FUN_0046d970 at +0x2c7c/+0x2c78. They default to
    // the clear Warehouse values; callers with a recovered owner can fill
    // them without changing the helper's static predicate.
    void set_collision_orientation_helper_gates(
        std::int32_t gate_a,
        std::int32_t gate_b) noexcept {
        collision_orientation_gate_a_ = gate_a;
        collision_orientation_gate_b_ = gate_b;
    }
    void add_collision_response(const FixedPosition& delta) noexcept {
        for (std::size_t index = 0; index < collision_response_.size(); ++index) {
            collision_response_[index] += delta[index];
        }
    }
    void set_motion_correction(FixedPosition correction) noexcept {
        motion_correction_ = correction;
    }
    void set_ground_surface_response_state(
        FixedPosition correction,
        FixedPosition normal,
        std::int32_t mode = 0) noexcept {
        ground_surface_response_correction_ = correction;
        ground_surface_response_normal_ = normal;
        ground_surface_response_mode_ = mode;
    }
    void set_ground_surface_response_surface(
        FixedPosition correction,
        FixedPosition normal) noexcept {
        ground_surface_response_correction_ = correction;
        ground_surface_response_normal_ = normal;
    }
    void set_ground_surface_response_normal(FixedPosition normal) noexcept {
        ground_surface_response_normal_ = normal;
    }
    void set_air_motion(FixedPosition motion) noexcept {
        air_motion_ = motion;
    }
    void set_orientation(Q12Matrix3 orientation) noexcept {
        orientation_ = orientation;
        retail_basis_ = retail_basis_from_matrix(orientation_);
        ground_surface_recovery_target_ = retail_basis_.at_310c;
        ground_surface_recovery_base_ = retail_basis_.at_310c;
        ground_surface_recovery_progress_q11_ = 0;
        ground_surface_recovery_update_frame_ = -1;
        orientation_basis_normalization_pending_ = true;
    }
    // Retail FUN_00490730 reaches FUN_0049b500 with (0x4b0, 1, 0) after a
    // restart-at-start acceptance. This is the same orientation/response
    // contract as the grounded turn phase, but with a caller-supplied angle.
    void apply_ground_response_yaw(std::int32_t angle12) noexcept;
    // Grounded retail frames canonicalize the three published basis vectors
    // before the turn producer consumes them. Keep the raw setter above
    // lossless so recordings can preserve the pre-frame state.
    void normalize_orientation_basis() noexcept;
    [[nodiscard]] bool set_queued_motion_command(
        std::int32_t axis,
        std::int16_t amount,
        std::int16_t rate) noexcept;
    [[nodiscard]] QueuedMotionDrainResult drain_queued_motion(
        std::int32_t frame_scale_q8 = 0x100) noexcept;
    // Completes the confirmed FUN_00493370 queued-motion handoff. The action
    // command stores local-axis amounts; retail transforms the drained vector
    // through the live orientation before adding it to the position.
    [[nodiscard]] FixedPosition apply_queued_motion(
        const QueuedMotionDrainResult& motion) noexcept;
    // Executes one recovered FUN_004be450 action command against the live
    // player boundary: 0x2b/0x2c use queued local-motion fields and 0x0f
    // writes the retail +0x4c/+0x50/+0x54 response vector.
    [[nodiscard]] ActionCommandDispatchResult dispatch_action_command(
        std::span<const std::uint8_t> stream,
        std::size_t& cursor) noexcept;
    [[nodiscard]] ActionStreamDispatchResult run_action_stream(
        std::span<const std::uint8_t> stream,
        std::size_t& cursor,
        std::size_t max_commands = 256) noexcept;
    // Publishes the action records written by FUN_00492190 for this player.
    // The optional lean bytes complete FUN_00492120's four-bit table index.
    void publish_action_profile(
        const ActionProfileState& profile,
        std::uint32_t timestamp,
        std::int8_t vertical_lean = 0,
        std::int8_t horizontal_lean = 0) noexcept;
    // Resolves a generated per-player sequence record against the live
    // history and executes its signed stream offset through the existing
    // bounded command dispatcher.
    [[nodiscard]] ActionSequenceExecutionResult run_action_sequences(
        const assets::TricksBinView& tricks,
        std::span<const std::uint8_t> sequence_table,
        const ActionSequenceMatcherInput& input,
        std::size_t max_records = 256,
        std::size_t max_commands = 256) noexcept;
    // FUN_004c4e30 applies the selected TRG restart's position and the two
    // adjacent restart fields before dispatching its post-name stream.
    void apply_restart(
        FixedPosition position,
        std::uint32_t auxiliary = 0,
        std::uint16_t auxiliary_word = 0) noexcept;
    void set_physics_state(std::int32_t state) noexcept { physics_state_ = state; }
    void set_level_event_field_2cdc(std::int32_t value) noexcept {
        field_2cdc_ = value;
    }
    void set_level_event_field_2dd4(std::int32_t value) noexcept {
        field_2dd4_ = value;
    }
    void set_level_event_field_107(std::uint8_t value) noexcept {
        field_107_ = value;
    }
    void set_level_event_field_2a8(std::int32_t value) noexcept {
        field_2a8_ = value;
    }
    void set_level_event_field_16c(std::int32_t value) noexcept {
        field_16c_ = value;
    }
    // The pending-score transfer is proven as +0x2a8 -> +0x16c followed by
    // clearing +0x2a8. The caller supplies the result returned by the level
    // event service; this player object owns only these two raw words.
    void apply_level_event_score(std::int32_t value) noexcept {
        field_16c_ = static_cast<std::int32_t>(
            static_cast<std::uint32_t>(field_16c_)
            + static_cast<std::uint32_t>(value));
        field_2a8_ = 0;
    }
    // FUN_00469a30 calls the animation service with the selected ID.  The
    // animation service itself is not reconstructed here, so retain a stable
    // request record without confusing it with the +0xf6 selector input.
    void request_level_event_animation(std::uint32_t animation) noexcept {
        last_level_event_animation_ = animation;
        ++level_event_animation_requests_;
    }
    void set_script_skater_fields(
        const PlayerScriptSkaterFields& fields) noexcept {
        script_skater_fields_ = fields;
    }
    void set_ground_update_state(std::int32_t state) noexcept {
        ground_update_state_ = state;
    }
    void set_ground_physics_mode(std::int32_t mode) noexcept {
        ground_physics_mode_ = mode;
    }
    void set_turn_accumulator(std::int32_t value) noexcept {
        turn_accumulator_ = value;
        turn_mirror_ = value;
    }
    void set_animation_state(std::uint16_t value) noexcept {
        animation_state_ = value;
    }
    void set_animation_frame(std::int16_t value) noexcept {
        animation_frame_ = value;
    }
    void set_ground_motion_cooldown(std::int32_t value) noexcept {
        ground_motion_cooldown_ = value < 0 ? 0 : value;
    }
    void set_ground_motion_event_pending(bool value) noexcept {
        ground_motion_event_pending_ = value;
    }
    void tick_ground_motion_cooldown() noexcept {
        if (physics_state_ == 0 && ground_motion_cooldown_ > 0) {
            --ground_motion_cooldown_;
        }
    }

    // This mirrors the raw writer at 0x004902bf and preserves the old state
    // and reason for replay/evidence consumers.
    void request_physics_state(
        std::int32_t state,
        std::uint32_t reason) noexcept;

    // State-2 entry writes its surface-response vector from the basis that
    // existed at the transition callsite, before the remainder of the
    // collision handler performs its recovery-basis update.
    void request_physics_state_from_basis(
        std::int32_t state,
        std::uint32_t reason,
        const FixedPosition& transition_basis) noexcept;

    // Static contract of retail FUN_004956f0. The state-2 branch is a short
    // handoff with reason 0x1605; the ordinary branch owns the cleanup write
    // ordering and returns the external stream/animation requests as a
    // result so their services remain explicit at the frame boundary.
    [[nodiscard]] GroundCollisionRecoveryExitResult
    exit_ground_collision_recovery() noexcept;

    void set_ground_collision_recovery_fields(
        std::int32_t stream_reference,
        std::int32_t stream_lock,
        std::int32_t heading_override) noexcept {
        collision_recovery_stream_ = stream_reference;
        field_2dd4_ = stream_lock;
        field_2dd8_ = heading_override;
    }

    [[nodiscard]] OllieImpulseResult apply_ollie_impulse(
        const OllieImpulseInput& input) noexcept;

    // Executes the confirmed KICK half of retail 0x0049a280. The caller owns
    // unresolved animation/stat/random inputs; this method owns charge,
    // latch, release, impulse, and raw-state request ordering.
    [[nodiscard]] OlliePrePhysicsResult run_ollie_prephysics(
        const InputState& input,
        const OlliePrePhysicsInput& config = {}) noexcept;

    // Retail 0x00497fff clears vertical acceleration and velocity after a
    // held JUMP action has exceeded two updates in the in-air handler.
    [[nodiscard]] bool apply_in_air_jump_hold_effect(
        const InputState& input) noexcept;

    // Executes the scalar gravity/clamp part of retail FUN_00497df0 against
    // the candidate +310c/+3110/+3114 air-motion vector.
    [[nodiscard]] AirGravityResult apply_air_gravity(
        AirGravityConfig config = {}) noexcept;

    // The common-air fallthrough at retail 0x004992f0 adds the current
    // +0x2dac scalar to the temporary +0x58 correction after the air handler
    // has completed its position/contact work.
    void apply_air_gravity_acceleration(std::int32_t acceleration) noexcept;

    // Completes the orientation/basis portion of retail FUN_00497df0 after
    // its scalar gravity update. The direction is normalized in +310c and
    // the resulting columns are copied back to the nine-short orientation.
    [[nodiscard]] AirMotionBasisResult update_air_motion_basis() noexcept;

    // Reconstructs the angle producer at retail 0x00493370/0x00498459.
    // This is kept separate from the later common-air upright helper: the
    // returned angle is applied to the in-air orientation before movement
    // collision, using only the current action records and frame scale.
    [[nodiscard]] std::optional<std::int32_t>
    compute_in_air_orientation_angle(
        const InputState& input,
        std::int32_t frame_scale_q8,
        bool alignment_gate_open) const noexcept;

    // FUN_00493370's state-1/2 action block advances +0x3144 before the
    // later in-air turn producer consumes it. The ordinary directional path
    // uses a 0xa000 Q12 step and clamps the accumulator at +/-0xa0000.
    void update_in_air_orientation_accumulator(
        const InputState& input,
        std::int32_t frame_scale_q8) noexcept;

    // Applies the early in-air orientation pivot and returns the old-pivot
    // minus new-pivot displacement used by the movement candidate.
    [[nodiscard]] FixedPosition apply_in_air_orientation_pivot(
        std::int32_t angle12) noexcept;

    // Reconstructs the common-air upright correction at FUN_0049c330. The
    // helper measures the current air direction against the cross product of
    // the forward basis and the shared global-up vector, then applies the
    // fixed eleven-unit roll when the signed threshold is crossed. This
    // helper republishes the orientation/basis after the movement commit;
    // it does not own the earlier in-air pivot displacement.
    void apply_upright_correction(
        const FixedPosition& global_up) noexcept;

    // Applies the shared recovered orientation-turn producer used by
    // FUN_00496360 and the ordinary state-1 portion of FUN_00497f40. The
    // resulting signed angle is the same right-multiplied Q12 operation in
    // both callers; the state-2 caller simply omits the response phase.
    void apply_air_orientation_turn(std::int32_t angle12) noexcept;

    // Computes and applies the shared profile/accumulator turn producer. The
    // returned value is the positive retail scalar before the matrix writer's
    // opposite signed angle is applied.
    [[nodiscard]] std::int32_t apply_orientation_turn_producer(
        AirOrientationTurnConfig config) noexcept;

    // Compatibility name for the ordinary state-1 0x00497f40 caller.
    [[nodiscard]] std::int32_t apply_in_air_orientation_turn(
        AirOrientationTurnConfig config) noexcept;

    // Applies the confirmed in-air Up/Down action contribution to the
    // temporary +58 correction using an explicit +0x2dac scalar.
    [[nodiscard]] AirDirectionInputResult apply_air_direction_input(
        const InputState& input,
        AirDirectionInputConfig config = {}) noexcept;

    // Convenience boundary that reproduces the retail +0x2dac writer before
    // applying the same Up/Down correction operation.
    [[nodiscard]] AirDirectionInputResult apply_air_direction_input(
        const InputState& input,
        AirSpeedConfig speed_config,
        std::int32_t scale_percent = 150) noexcept;

    // Applies the recovered pre-position in-air action-control block to the
    // temporary +0x58 correction. The caller supplies the raw gravity scalar
    // and global gate; action records are read from InputState here.
    [[nodiscard]] AirActionControlResult apply_air_action_control(
        const InputState& input,
        AirActionControlConfig config) noexcept;

    // The in-air handler requests ground state only after its caller has
    // classified a contact as acceptable. This keeps wall/rail filtering out
    // of the raw state transition while preserving the commit-before-request
    // ordering and retail landing reason.
    [[nodiscard]] bool accept_air_contact(
        FixedPosition contact_position,
        FixedPosition contact_normal) noexcept;

    [[nodiscard]] VelocityDampingResult apply_velocity_damping(
        VelocityDampingInput input = {}) noexcept;

    // Executes the recovered slope threshold/component-braking portion of
    // retail FUN_0049df00. The caller owns surface eligibility and raw
    // normal/stat inputs; a qualifying stop requests state 7.
    [[nodiscard]] GroundBrakeResult apply_ground_brake(
        GroundBrakeInput input = {}) noexcept;

    // Executes the evidence-backed stateful portion of retail FUN_0049df00.
    // The caller supplies surface/profile predicates and raw animation
    // readiness; this method owns response writes, +0x2df8 mode changes,
    // cooldown, and recovered physics-state requests.
    [[nodiscard]] GroundPhysicsResult update_ground_physics(
        GroundPhysicsInput input = {}) noexcept;

    // Connects the confirmed movement records to the bounded grounded turn
    // producer and the recovered Q12 yaw/basis handoff.
    [[nodiscard]] GroundTurnResult update_ground_turn(
        const InputState& input,
        GroundTurnConfig config = {}) noexcept;

    // Completes the ordinary state-0 phase of FUN_0049b500 before the
    // movement collision query. B010 consumes the saved pre-turn basis;
    // this boundary then publishes the new matrix and rotates the response
    // from that same saved matrix.
    void apply_ground_turn_velocity_phase(
        std::int32_t additional_turn_units = 0,
        std::int32_t frame_scale_q8 = 0x100) noexcept;

    // Executes the recovered temporary-correction writes from FUN_0049b010.
    // The caller supplies the profile/animation/stat gates that are still
    // stored in the retail skater object.
    [[nodiscard]] GroundMotionResult apply_ground_motion(
        const GroundMotionInput& input) noexcept;

    // State/frame portion of retail FUN_00492f20. Animation asset dispatch
    // remains outside this renderer-independent boundary.
    [[nodiscard]] GroundAnimationResult update_ground_animation(
        const GroundAnimationInput& input) noexcept;

    [[nodiscard]] GroundMotionThresholdResult
    update_ground_motion_threshold(
        const GroundMotionThresholdInput& input) noexcept;

    // Retail FUN_0049e680 copies the live position into the +0xbc history
    // vector before dispatching the per-state physics routine.
    void begin_physics_frame() noexcept {
        older_position_ = previous_position_;
        previous_position_ = position_;
        ground_turn_saved_orientation_valid_ = false;
        last_state_request_ = {};
        ++frame_counter_;
    }

    // State 7 in the retail dispatcher restores the live position from the
    // +0xbc history vector before its final ground stage.
    void restore_previous_position() noexcept { position_ = previous_position_; }

    // This is the native equivalent of the shared FUN_00496060 handoff. It
    // updates only the live position; history and response are owned by the
    // surrounding physics frame just as in the retail object.
    [[nodiscard]] PositionCommitResult commit_position(
        FixedPosition desired,
        const PositionCollisionProbe& probe,
        bool bypass_collision = false);

    // Applies the recovered FUN_00490610 response primitive to the raw
    // collision/platform vector at retail offset +0x4c.
    [[nodiscard]] std::int32_t remove_collision_normal_component(
        const FixedPosition& normal);

    // Opt-in reconstruction of retail FUN_00490680: remove a collision-normal
    // component and restore the pre-collision response magnitude.
    [[nodiscard]] VelocityProjectionResult project_collision_velocity(
        const FixedPosition& normal);

    // Rebuilds the causal orientation handoff in retail FUN_00491780. This
    // is used only by the state-2 recovery exit, after its collision normal
    // and two-position history have been selected.
    void apply_collision_transient_exit_orientation(
        const FixedPosition& collision_normal) noexcept;

    // Executes FUN_0046d970 at the 0x00496550 pre-query boundary. The helper
    // conditionally flips the two tangent axes and its handedness flag when
    // the response is strongly aligned with the current forward axis.
    [[nodiscard]] bool apply_collision_orientation_handedness() noexcept;

    // Executes the conditional landing handoff at retail 0x00497960. It can
    // reverse the forward/air axes and replace the response with the
    // negative short normal scaled by ten before the final recovery-record
    // write and 0x0049d080 publication.
    [[nodiscard]] bool apply_landing_orientation_handoff(
        const FixedPosition& collision_normal) noexcept;

    [[nodiscard]] CollisionResponseResult apply_collision_response(
        const FixedPosition& surface_delta,
        std::int32_t bias_q12 = 0xcd);

    // Completes the orientation portion of FUN_0049bad0. The caller supplies
    // the source-dependent yaw offset; the method keeps the recovered
    // negative-forward test, Q12 basis rotations, and normalized air-basis
    // rebuild separate from the first response stage.
    [[nodiscard]] CollisionOrientationResult apply_collision_orientation(
        const FixedPosition& surface_delta,
        std::int32_t yaw_offset = 0x19) noexcept;

    // Completes the retained non-landing branch of FUN_00497f40: reset the
    // air basis, run FUN_0049bad0 with its zero/fallback heading, add the
    // quarter-normal response, and consume the causal random results.
    void apply_air_normal_recovery(
        const FixedPosition& collision_normal,
        const AirNormalRecoveryInput& input) noexcept;

    // Reconstructs FUN_0049c060 -> FUN_00496360's signed surface-response
    // heading. The returned angle is the value passed through the grounded
    // FUN_0049b500 matrix writer after the frame-scale conversion.
    [[nodiscard]] std::int32_t apply_ground_surface_response(
        const GroundSurfaceResponseInput& input,
        std::int32_t frame_scale_q8,
        bool rotate_collision_response = true) noexcept;

    // FUN_0049c060's persistent +0x3124 result, before the caller folds it
    // into the +0x3144 turn units and invokes FUN_0049b500.
    [[nodiscard]] std::int32_t compute_ground_surface_response_delta(
        const GroundSurfaceResponseInput& input) noexcept;

    // Executes the complete FUN_00496360 control phase. The optional service
    // input is consumed only when the static timer/turn predicate requests
    // FUN_0049c060; the caller does not pre-apply its result.
    [[nodiscard]] GroundSurfaceResponseStepResult
    apply_ground_surface_response_step(
        GroundSurfaceResponseStepInput input,
        const std::optional<GroundSurfaceResponseInput>& response_input) noexcept;

    // Completes the state-1 leave-air branch in FUN_00496550. Retail scales
    // the final collision normal by one quarter of the current response
    // metric, subtracts it from +0x4c/+0x54, and halves +0x50 before the
    // 0x1ab6 state request.
    void apply_ground_leave_air_response(
        const FixedPosition& collision_normal) noexcept;

    // Rebuilds the grounded collision basis through retail FUN_0049d080.
    // The supplied normal is eased one quarter of the way from the current
    // +310c axis before the fixed-point cross products publish [forward,
    // right, up] back to the orientation and basis fields.
    void apply_orientation_recovery(
        const FixedPosition& surface_normal,
        bool recovery_complete = false) noexcept;

    // Seeds the persistent +0x80/+0x313x recovery record after the in-air
    // handler publishes an accepted landing normal. The next grounded frame
    // must continue from this contact rather than from the prior ground
    // surface's recovery target.
    void seed_ground_surface_recovery(
        const FixedPosition& surface_normal) noexcept;

    // FUN_00496550 remembers the last accepted surface normal at +0x80 and
    // advances its recovery timer at +0x3130. A new normal resets that timer;
    // a repeated normal continues it. Return the former condition because it
    // also selects which position candidate the surrounding frame publishes.
    [[nodiscard]] bool update_ground_surface_recovery(
        const FixedPosition& surface_normal,
        std::int32_t delta_q11) noexcept;

    // FUN_0049f4c0's confirmed platform/bounce producer. Triggering flags,
    // sounds, and platform lifetime are owned by the caller; this method only
    // writes the recovered +4c/+50/+54 response vector.
    void apply_bouncy_platform_response(
        std::int32_t platform_type,
        const FixedPosition& source_vector,
        std::int32_t source_magnitude_q12) noexcept;

    // Ground collision FUN_00496550 removes the response component along the
    // +3100 basis vector. When its profile/speed gate is open, a separate
    // forward-basis term subtracts the observed factor 8, scaled by the
    // current frame clock, into the temporary +58 correction.
    void prepare_ground_basis_correction(
        bool apply_forward_term,
        std::int32_t frame_scale_q8 = 0x100,
        std::int32_t forward_scale = 8,
        bool apply_response_basis = true) noexcept;

    // FUN_00496550 retains the material class decoded by its last collision
    // helper in the raw +0x30b0 field. The later correction tail consumes
    // class 3 independently of the ordinary response-basis gate.
    void set_ground_surface_class(std::int32_t value) noexcept {
        ground_surface_class_ = value;
    }

    [[nodiscard]] std::int32_t ground_surface_class() const noexcept {
        return ground_surface_class_;
    }

    [[nodiscard]] std::int32_t ground_surface_recovery_progress_q11() const noexcept {
        return ground_surface_recovery_progress_q11_;
    }

    // The outer FUN_0049e680 frame applies +58/+5c/+60 back into +4c/+50/+54
    // using DAT_0056865c and an arithmetic Q8 shift.
    void integrate_motion_correction(
        std::int32_t frame_scale_q8 = 0x100) noexcept;

    // Ground and in-air routines use the same position integrator before
    // their collision/state branches: position += velocity*dt +
    // correction*dt^2/2. The retail helper sequence is raw multiply, SAR 8,
    // divide by two, then vector-add.
    [[nodiscard]] FixedPosition integrated_position(
        std::int32_t frame_scale_q8 = 0x100) const noexcept;

    void integrate_position(
        std::int32_t frame_scale_q8 = 0x100) noexcept;

    void clear_motion_correction() noexcept { motion_correction_ = {}; }
    void add_motion_correction(const FixedPosition& delta) noexcept {
        for (std::size_t index = 0; index < motion_correction_.size(); ++index) {
            motion_correction_[index] += delta[index];
        }
    }

private:
    FixedPosition position_{};
    FixedPosition previous_position_{};
    // Retail +0x2e00 is the position from the preceding physics frame;
    // previous_position_ is the current frame's +0xbc history value.
    FixedPosition older_position_{};
    // +0x2e0c/+0x2e10/+0x2e14: persistent scratch reference used by the
    // outer floor/restart helper's two component fallback sweeps.
    FixedPosition outer_floor_reference_position_{};
    // +0x2e90 is a gate: zero enables FUN_00490730. The native name describes
    // the nonzero/blocked sense to avoid hiding the retail comparison.
    bool outer_floor_recovery_blocked_{};
    std::int32_t outer_floor_distance_{};
    FixedPosition collision_response_{};
    FixedPosition motion_correction_{};
    FixedPosition air_motion_{};
    std::int32_t physics_state_{};
    std::int32_t ground_update_state_{};
    std::int32_t ground_physics_mode_{};
    std::int32_t turn_accumulator_{};
    std::int32_t turn_mirror_{};
    bool ground_turn_wide_profile_{};
    bool ground_turn_policy_changed_{};
    bool control_blocked_{};
    std::int32_t control_blocked_velocity_decay_divisor_{};
    std::int32_t collision_orientation_gate_a_{};
    std::int32_t collision_orientation_gate_b_{};
    bool collision_orientation_handedness_{};
    std::uint16_t animation_state_{};
    std::int16_t animation_frame_{};
    std::int32_t field_2cdc_{};
    std::int32_t field_2dd4_{};
    std::int32_t field_2dd8_{};
    std::int32_t collision_recovery_stream_{};
    std::int32_t collision_recovery_auxiliary_x_{};
    std::int32_t collision_recovery_auxiliary_y_{};
    std::int32_t collision_recovery_correction_gate_{};
    std::int32_t collision_recovery_active_{};
    std::int32_t collision_recovery_latch_{};
    std::int32_t collision_recovery_block_{};
    std::int32_t ground_surface_class_{};
    std::uint8_t field_107_{};
    std::int32_t field_2a8_{};
    std::int32_t field_16c_{};
    std::uint32_t level_event_animation_requests_{};
    std::uint32_t last_level_event_animation_{};
    // FUN_0046c98c initializes skater +0x2dc8 before the first frame.
    // Later FUN_0049e680 updates it from the shared RNG/stat service.
    std::int32_t ground_motion_threshold_{0x2e9b6};
    std::int32_t ground_motion_cooldown_{};
    // B010 +0x30a8/+0x30ac/+0x108 state. The event dispatcher itself is
    // still outside PlayerState; retain the last reason for replay/evidence.
    bool ground_motion_event_pending_{};
    std::uint32_t ground_motion_event_reason_{};
    std::uint32_t ground_motion_event_parameter_{};
    std::int32_t ground_motion_animation_speed_{};
    Q12Matrix3 orientation_{q12_identity_matrix()};
    RetailBasis retail_basis_{retail_basis_from_matrix(orientation_)};
    bool orientation_basis_normalization_pending_{true};
    FixedPosition ground_surface_recovery_target_{0, 4096, 0};
    FixedPosition ground_surface_recovery_base_{0, 4096, 0};
    FixedPosition ground_surface_response_correction_{};
    FixedPosition ground_surface_response_normal_{0, 4096, 0};
    std::int32_t ground_surface_response_mode_{};
    std::int32_t ground_surface_response_correction_units_{};
    std::int32_t ground_surface_response_timer_{};
    std::int32_t ground_surface_response_phase_accumulator_{};
    std::int32_t ground_surface_response_phase_count_{};
    // Raw control block consumed by the connected 0x0048f5f0/0x0049e430 /
    // 0x00496360 chain. The 0x2cc8 value is a presence-only service contract;
    // its constructor fallback is known, while later external writers remain
    // outside this player boundary.
    // FUN_0046c720's ordinary constructor fallback for +0x2cc8. Mode/table
    // owners can replace this value through the setter before a reset call.
    std::int32_t surface_response_action_context_2cc8_{0x1c3e};
    std::int32_t surface_response_game_mode_{};
    std::int32_t surface_response_phase_countdown_2c88_{};
    std::int32_t surface_response_phase_2c8c_{};
    std::int32_t surface_response_phase_rate_2c90_{};
    std::int32_t surface_response_action_gate_2c80_{};
    std::int32_t surface_response_phase_refresh_gate_2c84_{};
    std::int32_t surface_response_spin_phase_2e80_{};
    std::int32_t surface_response_dispatch_active_3064_{};
    std::int32_t surface_response_level_flag_3074_{};
    std::int32_t surface_response_level_flag_3078_{};
    std::int32_t surface_response_level_flag_307c_{};
    std::int32_t surface_response_auxiliary_2cbc_{};
    std::int32_t ground_surface_recovery_progress_q11_{};
    std::int32_t ground_surface_recovery_update_frame_{-1};
    Q12Matrix3 ground_turn_saved_orientation_{q12_identity_matrix()};
    std::int32_t ground_turn_angle12_{};
    bool ground_turn_saved_orientation_valid_{};
    QueuedMotionState queued_motion_{};
    ActionCommandRuntimeState action_command_state_{};
    RetailActionHistory action_history_{};
    // FUN_00491b80 installs the resolved image-relative stream at +0x29cc
    // and marks +0x29c8 active. Keep the asset-relative identity alongside
    // the bounded native byte cursor so a 0x2c yield can resume next frame.
    bool action_stream_active_{};
    std::int16_t action_stream_relative_{};
    std::size_t action_stream_cursor_{};
    OllieBookkeeping ollie_{};
    PhysicsStateRequest last_state_request_{};
    std::uint32_t restart_auxiliary_{};
    std::uint16_t restart_auxiliary_word_{};
    std::int32_t frame_counter_{};
    std::int32_t collision_recovery_frame_{};
    PlayerScriptSkaterFields script_skater_fields_{};
};

} // namespace opentony::runtime
