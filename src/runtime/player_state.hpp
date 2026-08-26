#pragma once

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

#include <cstdint>
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
          orientation_(q12_identity_matrix()),
          retail_basis_(retail_basis_from_matrix(orientation_)) {}

    [[nodiscard]] const FixedPosition& position() const noexcept {
        return position_;
    }
    [[nodiscard]] const FixedPosition& previous_position() const noexcept {
        return previous_position_;
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
    [[nodiscard]] std::int32_t ground_motion_cooldown() const noexcept {
        return ground_motion_cooldown_;
    }
    [[nodiscard]] std::uint16_t animation_state() const noexcept {
        return animation_state_;
    }
    [[nodiscard]] std::int16_t animation_frame() const noexcept {
        return animation_frame_;
    }
    [[nodiscard]] std::int32_t ground_motion_threshold() const noexcept {
        return ground_motion_threshold_;
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
    [[nodiscard]] const Q12Matrix3& orientation() const noexcept {
        return orientation_;
    }
    [[nodiscard]] const RetailBasis& retail_basis() const noexcept {
        return retail_basis_;
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
    }
    void set_collision_response(FixedPosition response) noexcept {
        collision_response_ = response;
    }
    void set_motion_correction(FixedPosition correction) noexcept {
        motion_correction_ = correction;
    }
    void set_air_motion(FixedPosition motion) noexcept {
        air_motion_ = motion;
    }
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

    // Completes the orientation/basis portion of retail FUN_00497df0 after
    // its scalar gravity update. The direction is normalized in +310c and
    // the resulting columns are copied back to the nine-short orientation.
    [[nodiscard]] AirMotionBasisResult update_air_motion_basis() noexcept;

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

    // The in-air handler requests ground state only after its caller has
    // classified a contact as acceptable. This keeps wall/rail filtering out
    // of the raw state transition while preserving the commit-before-request
    // ordering and retail landing reason.
    [[nodiscard]] bool accept_air_contact(
        FixedPosition contact_position) noexcept;

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

    // Completes the ordinary state-0 phase of FUN_0049b500 after the current
    // frame's candidate position has been integrated. The saved orientation
    // is captured by update_ground_turn(); the response update therefore
    // affects the following frame's displacement, not this one.
    void apply_ground_turn_velocity_phase() noexcept;

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
        previous_position_ = position_;
        ground_turn_saved_orientation_valid_ = false;
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

    // FUN_0049f4c0's confirmed platform/bounce producer. Triggering flags,
    // sounds, and platform lifetime are owned by the caller; this method only
    // writes the recovered +4c/+50/+54 response vector.
    void apply_bouncy_platform_response(
        std::int32_t platform_type,
        const FixedPosition& source_vector,
        std::int32_t source_magnitude_q12) noexcept;

    // Ground collision FUN_00496550 removes the motion component along the
    // +3100 basis vector from the temporary +58 correction. A separate,
    // configurable forward-basis term covers the branch that is known to use
    // the observed factor 8; ownership/surface conditions remain outside this
    // raw boundary.
    void prepare_ground_basis_correction(
        bool apply_forward_term,
        std::int32_t forward_scale = 8) noexcept;

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

private:
    FixedPosition position_{};
    FixedPosition previous_position_{};
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
    std::uint16_t animation_state_{};
    std::int16_t animation_frame_{};
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
    PlayerScriptSkaterFields script_skater_fields_{};
};

} // namespace opentony::runtime
