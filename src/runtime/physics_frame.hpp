#pragma once

#include "physics_dispatch.hpp"
#include "action_profile.hpp"
#include "air_contact.hpp"
#include "collision_recovery.hpp"

#include <functional>
#include <optional>
#include <span>

namespace opentony::assets {
struct TricksBinView;
}

namespace opentony::runtime {

struct ActionSequenceSource final {
    const assets::TricksBinView* tricks{};
    std::span<const std::uint8_t> sequence_table{};
    ActionSequenceMatcherInput matcher{};
    // The retail builder normally supplies a per-player table at header
    // sections 3/4. When that heap result is not available, callers may
    // explicitly use the shipped source table as a broad, asset-backed
    // fallback while reconstructing the player-specific filters.
    bool use_source_sequence_fallback{false};
};

// Hooks for the parts of the retail frame whose stat tables or collision
// classifiers are not yet fully recovered. A stage hook runs at stage entry,
// before the confirmed shared position integration and commit for movement
// stages. This lets a caller supply gravity, steering, or surface response
// while retaining the retail frame order.
struct PlayerPhysicsFrameHooks {
    std::function<void(PlayerState&, const InputState&)> on_prephysics;
    // Retail FUN_00493370 drains the queued local-motion words before the
    // later FUN_004be450 action stream can seed the next frame. The frame now
    // performs the confirmed FUN_004e85a0 orientation transform by default;
    // this callback remains available for observers or additional effects.
    std::function<void(PlayerState&, const QueuedMotionDrainResult&)>
        on_queued_motion;
    // FUN_00492ea0 invokes FUN_004be450 after the frame's action update and
    // after the queued-motion drain. This preserves the producer/consumer
    // ordering for callers that decode the recovered action stream.
    std::function<void(PlayerState&)> on_action_stream;
    // Observer at the confirmed FUN_00492190 -> FUN_004925e0 boundary. The
    // history ring is already updated when this runs; it is intentionally an
    // observer, not a replacement for the recovered publisher.
    std::function<void(
        const PlayerState&,
        const ActionProfileState&)> on_action_history;
    // Optional fully connected generated-table/action-archive path. The
    // source is intentionally a view so loaders can keep ownership outside
    // the frame object while PlayerState owns the history ring.
    std::optional<ActionSequenceSource> action_sequence_source;
    std::function<void(PlayerState&, const ActionSequenceExecutionResult&)>
        on_action_sequence;
    // FUN_0049b010 runs after the grounded turn handoff and before the later
    // action bookkeeping. The callback receives the verified action-profile
    // slots; the frame fills the recovered animation, turn-gate, cooldown,
    // and speed fields before invoking the producer.
    std::function<std::optional<GroundMotionInput>(
        const PlayerState&,
        const InputState&,
        const ActionProfileState&)> ground_motion_input;
    // B010 can publish an animation/event request before its later motion
    // branch. The caller-owned animation service applies that request at the
    // same pre-collision boundary.
    std::function<void(PlayerState&, const GroundMotionResult&)>
        on_ground_motion_event;
    // FUN_00496360 invokes FUN_0049c060 immediately before the grounded
    // collision query. The callback supplies only its causal random draws;
    // PlayerState owns the persistent surface correction, heading latch,
    // orientation write, and response rotation.
    std::function<std::optional<GroundSurfaceResponseInput>(
        const PlayerState&, const InputState&)> ground_surface_response_input;
    // FUN_00490730 runs after the frame-local +0x2dcc response handoff and
    // before Skater_PhysicsDispatcher. Its geometry uses the shared collision
    // query; the restart-at-start global and FUN_0046d2e0 service remain
    // explicit caller-owned inputs.
    bool apply_outer_floor_recovery{false};
    bool outer_floor_restart_at_start{false};
    std::function<void(const PositionCollisionHit&)>
        on_outer_floor_external_service;
    // State/frame portion of FUN_00492f20. The default derives its verified
    // lean/profile inputs from the current action profile and PlayerState.
    std::function<GroundAnimationInput(
        const PlayerState&,
        const InputState&,
        const ActionProfileState&)> ground_animation_input;
    // Pre-dispatch +0x2dc8 update. The callback supplies the retail random
    // roll and the unresolved +0x2dd8 special-state predicate.
    std::function<std::optional<GroundMotionThresholdInput>(
        const PlayerState&, const InputState&)> ground_motion_threshold_input;
    // Supplies the recovered random/stat seam for KICK charge/release. The
    // hook is absent by default because the shared retail RNG and animation
    // eligibility are not yet owned by this renderer-independent layer.
    std::function<OlliePrePhysicsInput(
        const PlayerState&, const InputState&)> ollie_input;
    // FUN_0049a280's first-held-kick RunAnim request. The cursor/pose service
    // remains caller-owned, so the frame reports the causal request at the
    // point where the producer emits it.
    std::function<void(
        PlayerState&, const OlliePrePhysicsResult&)>
        on_ollie_animation_request;
    // Optional surface/stat seam for the confirmed grounded threshold and
    // braking branch at retail 0x0049df00.
    std::function<std::optional<GroundBrakeInput>(
        const PlayerState&, const InputState&)> ground_brake_input;
    // Stateful FUN_0049df00 boundary. The callback supplies the unresolved
    // surface/profile predicates; PlayerState fills response, raw mode,
    // animation fields, and applies the returned state/cooldown writes.
    std::function<std::optional<GroundPhysicsInput>(
        const PlayerState&, const InputState&)> ground_physics_input;
    // FUN_004956f0's external cleanup boundary. PlayerState has already
    // performed the deterministic FUN_0048f5f0 stores when this callback
    // runs; the caller owns its service-owned cleanup, action-stream restart,
    // and animation services.
    std::function<void(
        PlayerState&, const GroundCollisionRecoveryExitResult&)>
        on_ground_collision_recovery_exit;
    // Optional config seam for the confirmed scalar in-air gravity producer
    // at FUN_00497df0. The candidate air-motion vector lives separately from
    // the +4c collision/platform response until the remaining transform and
    // movement handoff are recovered.
    std::function<std::optional<AirGravityConfig>(
        const PlayerState&, const InputState&)> air_gravity_input;
    // Common-air fallthrough at retail 0x004992f0. This is deliberately
    // separate from air_gravity_input: the latter updates the +0x310c air
    // direction before dispatch, while this producer adds +0x2dac to the
    // temporary +0x58 correction after air position/contact work.
    std::function<std::optional<std::int32_t>(
        const PlayerState&, const InputState&)> air_gravity_acceleration_input;
    // Optional explicit +0x2dac seam for the confirmed in-air Up/Down
    // contribution to the temporary +58 correction.
    std::function<std::optional<AirDirectionInputConfig>(
        const PlayerState&, const InputState&)> air_direction_input;
    // Optional retail +0x2dac stat/mode/RNG seam. When present without an
    // explicit air_direction_input hook, the frame computes the scalar and
    // applies the confirmed Up/Down producer with its default 150% scale.
    std::function<std::optional<AirSpeedConfig>(
        const PlayerState&, const InputState&)> air_speed_input;
    // FUN_0049e680's raw-state-2 +0x2dac writer runs before the dispatcher,
    // including on the state-2 ground-collision path. The frame-level writer
    // is applied after collision candidate selection so it is retained in the
    // temporary correction vector for the outer +58 -> +4c handoff.
    std::function<std::optional<AirSpeedConfig>(
        const PlayerState&, const InputState&)> state_two_motion_input;
    // Optional pre-position action-control seam from the first
    // 0x00497f40 block. This owns the recovered KICK/UP/DOWN/SPIN terms and
    // stabilization while the gravity scalar/global gate remain caller data.
    std::function<std::optional<AirActionControlConfig>(
        const PlayerState&, const InputState&)> air_action_control_input;
    // Optional completion of FUN_00497df0's basis/orientation handoff after
    // air_gravity_input has applied its scalar update.
    bool apply_air_motion_basis{false};
    // Early FUN_00493370/00498459 angle producer. The frame applies its
    // orientation pivot before the in-air movement collision candidate.
    std::function<std::optional<std::int32_t>(
        const PlayerState&, const InputState&, std::int32_t)>
        air_orientation_pivot_input;
    // Later ordinary state-1 turn block in FUN_00497f40. The frame owns the
    // +3144 accumulator and matrix-write ordering; the caller supplies the
    // existing FUN_0048f3a0(4)/modifier service result.
    std::function<std::optional<AirOrientationTurnConfig>(
        const PlayerState&, const InputState&, std::int32_t)>
        air_orientation_turn_input;
    // FUN_0049c330 runs after the in-air position commit and republishes the
    // short orientation/basis. The shared global-up vector is supplied by the
    // caller; the earlier pivot displacement is a separate phase above.
    std::function<std::optional<FixedPosition>(
        const PlayerState&, const InputState&)> air_upright_input;
    std::function<void(
        PhysicsDispatchStage,
        PlayerState&,
        const InputState&)> on_stage;
    std::function<void(
        PlayerState&,
        const PositionCollisionHit&,
        const PositionCommitResult&)> on_collision;
    // Optional caller-specific entry for the confirmed first stage of
    // FUN_0049bad0. Ground and special-state callers do not all use the same
    // response policy, so the native frame does not apply it to every hit.
    std::function<std::optional<std::int32_t>(
        const PlayerState&,
        const PositionCollisionHit&,
        PhysicsDispatchStage)> collision_response_bias_q12;
    std::function<std::optional<std::int32_t>(
        const PlayerState&,
        const PositionCollisionHit&,
        PhysicsDispatchStage)> collision_orientation_yaw;
    // Returns true only when the caller's surface classifier identifies an
    // in-air hit as a landing. Walls, rails, and non-ground contacts remain
    // in their current state.
    std::function<bool(
        PlayerState&,
        const PositionCollisionHit&,
        const PositionCommitResult&)> on_air_contact;
    // Optional direct bridge for the ordinary 0x00497f40 landing predicate.
    // The callback supplies only the player-owned blocked/last-surface fields;
    // packed material flags are decoded from PositionCollisionHit.
    std::function<std::optional<StandardAirContactInput>(
        const PlayerState&,
        const InputState&,
        const PositionCollisionHit&)> standard_air_contact_input;
    // The non-landing branch of FUN_00497f40 enters the shared normal
    // recovery path after its collision query. The callback supplies only
    // the causal FUN_004c9340 results; position, response, state, and basis
    // writes remain native producers.
    std::function<std::optional<AirNormalRecoveryInput>(
        const PlayerState&,
        const InputState&,
        const PositionCollisionHit&)> air_normal_recovery_input;
    // The accepted-contact caller reaches the known animation request at
    // retail FUN_0049a519 (animation 5, start 0, end -1). The callback keeps
    // that caller-owned request explicit while pose/asset decoding remains
    // outside this frame boundary.
    std::function<std::optional<GroundAnimationRequest>(
        const PlayerState&,
        const PositionCollisionHit&)> landing_animation_request;
    std::function<void(PlayerState&, const PhysicsDispatchResult&)>
        on_postphysics;
    // The outer frame wrapper's completed transient +0x58 correction. It is
    // sampled after the normal +0x58 -> +0x4c handoff so the captured value is
    // authoritative for the frame-end snapshot.
    std::function<std::optional<FixedPosition>(
        const PlayerState&, const PhysicsDispatchResult&)>
        motion_correction_input;
    // The confirmed 0x004ca9f0 response add. This is separate from the
    // transient correction because retail scales the latter before adding it
    // to persistent response velocity.
    std::function<std::optional<FixedPosition>(
        const PlayerState&, const PhysicsDispatchResult&)>
        response_correction_input;
    // Optional post-dispatch producer for the confirmed 0x0049d480 response
    // damping. The callback supplies random/mode-table values; the vector is
    // always taken from the current PlayerState.
    std::function<std::optional<VelocityDampingInput>(
        const PlayerState&, const PhysicsDispatchResult&)> velocity_damping_input;

    PositionCollisionProbe collision_probe;
    PositionCollisionQuery collision_query;
    GroundTurnConfig ground_turn_config{};
    // Automatic +3144/+3148 handoff is restricted to confirmed grounded
    // dispatcher states 0 and 7. Callers can still use on_prephysics for
    // state-specific airborne rotation.
    bool apply_ground_turn{true};
    // Confirmed queued local-motion transform/position commit from
    // FUN_00493370. Disable only for isolated producer tests.
    bool apply_queued_motion{true};
    bool integrate_position{true};
    bool integrate_motion_correction{true};
    // Confirmed tail of FUN_00496550. The first projection always updates
    // persistent response; the forward term is separately gated by the
    // caller-owned profile/speed predicate.
    bool apply_ground_basis_correction{false};
    bool apply_ground_basis_forward_term{false};
    // Ground FUN_00496550 performs a second, basis-oriented surface ray
    // after the ordinary position query. Its hit drives FUN_0049d080 and the
    // grounded response/correction handoff before the final position commit.
    bool apply_ground_surface_recovery{false};
    // Camera timing's DAT_0056a93c reaches the same grounded recovery timer.
    // Replay supplies the recorded Q11 delta so the terminal 0x18000 branch
    // remains causal instead of being inferred from a nominal frame rate.
    std::int32_t ground_surface_recovery_delta_q11{};
    // Equivalent caller gate for skater +0x3200. Static matching proves that
    // a nonzero word bypasses FUN_00496060 collision resolution, but its
    // writer is not present on the selected paths, so this remains explicit.
    bool bypass_collision{false};
    bool apply_in_air_jump_hold_effect{true};
    // FUN_00490610 is confirmed in the ground/air hit path. It is enabled for
    // metadata-producing queries; callers can disable it while recovering a
    // state whose response policy is known to differ.
    bool remove_hit_normal_component{true};
};

struct PlayerPhysicsFrameResult {
    std::int32_t physics_state_before{};
    std::int32_t physics_state_after{};
    PhysicsStateRequest state_request{};
    ActionProfileState action_profile{};
    PhysicsDispatchResult dispatch{};
    PositionCommitResult position_commit{};
    bool position_integrated{};
    bool motion_correction_integrated{};
    std::optional<GroundTurnResult> ground_turn;
    std::optional<PositionCollisionHit> collision_hit;
    std::optional<PositionCollisionHit> ground_surface_hit;
    bool hit_normal_removed{};
    std::optional<CollisionResponseResult> collision_response;
    std::optional<CollisionOrientationResult> collision_orientation;
    bool in_air_jump_hold_applied{};
    bool landed{};
    bool velocity_damped{};
    VelocityDampingResult velocity_damping{};
    std::optional<GroundBrakeResult> ground_brake;
    std::optional<GroundPhysicsResult> ground_physics;
    std::optional<AirGravityResult> air_gravity;
    std::optional<std::int32_t> air_gravity_acceleration;
    std::optional<AirMotionBasisResult> air_motion_basis;
    std::optional<AirActionControlResult> air_action_control;
    std::optional<AirDirectionInputResult> air_direction_input;
    std::optional<OlliePrePhysicsResult> ollie;
    std::optional<ActionSequenceExecutionResult> action_sequence;
    std::optional<GroundMotionResult> ground_motion;
    std::optional<GroundAnimationResult> ground_animation;
    std::optional<GroundAnimationRequest> landing_animation_request;
    std::optional<GroundMotionThresholdResult> ground_motion_threshold;
    std::optional<OuterFloorRecoveryResult> outer_floor_recovery;
    QueuedMotionDrainResult queued_motion{};
    FixedPosition queued_motion_world_delta{};
};

// Native execution boundary for the confirmed portion of FUN_0049e680.
// Branch-specific gameplay producers remain callbacks, but history capture,
// correction reset, state dispatch, shared integration, collision commit, and
// post-dispatch velocity correction are now executable in one frame.
class PlayerPhysicsFrame final {
public:
    [[nodiscard]] static PlayerPhysicsFrameResult step(
        PlayerState& player,
        const InputState& input,
        const PlayerPhysicsFrameHooks& hooks = {},
        std::int32_t frame_scale_q8 = 0x100);
};

} // namespace opentony::runtime
