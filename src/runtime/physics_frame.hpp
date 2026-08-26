#pragma once

#include "physics_dispatch.hpp"
#include "action_profile.hpp"

#include <functional>

namespace opentony::runtime {

// Hooks for the parts of the retail frame whose stat tables or collision
// classifiers are not yet fully recovered. A stage hook runs at stage entry,
// before the confirmed shared position integration and commit for movement
// stages. This lets a caller supply gravity, steering, or surface response
// while retaining the retail frame order.
struct PlayerPhysicsFrameHooks {
    std::function<void(PlayerState&, const InputState&)> on_prephysics;
    // Retail FUN_00493370 drains the queued local-motion words before the
    // later FUN_004be450 action stream can seed the next frame. The native
    // callback owns the still-unverified FUN_004e85a0 basis transform and may
    // apply the returned local delta to a presentation/world position.
    std::function<void(PlayerState&, const QueuedMotionDrainResult&)>
        on_queued_motion;
    // FUN_00492ea0 invokes FUN_004be450 after the frame's action update and
    // after the queued-motion drain. This preserves the producer/consumer
    // ordering for callers that decode the recovered action stream.
    std::function<void(PlayerState&)> on_action_stream;
    // FUN_0049b010 runs after the grounded turn handoff and before the later
    // action bookkeeping. The callback receives the verified action-profile
    // slots; the frame fills the recovered animation, turn-gate, cooldown,
    // and speed fields before invoking the producer.
    std::function<std::optional<GroundMotionInput>(
        const PlayerState&,
        const InputState&,
        const ActionProfileState&)> ground_motion_input;
    // State/frame portion of FUN_00492f20. The default derives its verified
    // lean/profile inputs from the current action profile and PlayerState.
    std::function<GroundAnimationInput(
        const PlayerState&,
        const InputState&,
        const ActionProfileState&)> ground_animation_input;
    // Post-dispatch +0x2dc8 update. The callback supplies the retail random
    // roll and the unresolved +0x2dd8 special-state predicate.
    std::function<std::optional<GroundMotionThresholdInput>(
        const PlayerState&, const InputState&)> ground_motion_threshold_input;
    // Supplies the recovered random/stat seam for KICK charge/release. The
    // hook is absent by default because the shared retail RNG and animation
    // eligibility are not yet owned by this renderer-independent layer.
    std::function<OlliePrePhysicsInput(
        const PlayerState&, const InputState&)> ollie_input;
    // Optional surface/stat seam for the confirmed grounded threshold and
    // braking branch at retail 0x0049df00.
    std::function<std::optional<GroundBrakeInput>(
        const PlayerState&, const InputState&)> ground_brake_input;
    // Optional config seam for the confirmed scalar in-air gravity producer
    // at FUN_00497df0. The candidate air-motion vector lives separately from
    // the +4c collision/platform response until the remaining transform and
    // movement handoff are recovered.
    std::function<std::optional<AirGravityConfig>(
        const PlayerState&, const InputState&)> air_gravity_input;
    // Optional explicit +0x2dac seam for the confirmed in-air Up/Down
    // contribution to the temporary +58 correction.
    std::function<std::optional<AirDirectionInputConfig>(
        const PlayerState&, const InputState&)> air_direction_input;
    // Optional retail +0x2dac stat/mode/RNG seam. When present without an
    // explicit air_direction_input hook, the frame computes the scalar and
    // applies the confirmed Up/Down producer with its default 150% scale.
    std::function<std::optional<AirSpeedConfig>(
        const PlayerState&, const InputState&)> air_speed_input;
    // Optional completion of FUN_00497df0's basis/orientation handoff after
    // air_gravity_input has applied its scalar update.
    bool apply_air_motion_basis{false};
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
    std::function<void(PlayerState&, const PhysicsDispatchResult&)>
        on_postphysics;
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
    bool integrate_position{true};
    bool integrate_motion_correction{true};
    bool bypass_collision{false};
    bool apply_in_air_jump_hold_effect{true};
    // FUN_00490610 is confirmed in the ground/air hit path. It is enabled for
    // metadata-producing queries; callers can disable it while recovering a
    // state whose response policy is known to differ.
    bool remove_hit_normal_component{true};
};

struct PlayerPhysicsFrameResult {
    ActionProfileState action_profile{};
    PhysicsDispatchResult dispatch{};
    PositionCommitResult position_commit{};
    bool position_integrated{};
    bool motion_correction_integrated{};
    std::optional<GroundTurnResult> ground_turn;
    std::optional<PositionCollisionHit> collision_hit;
    bool hit_normal_removed{};
    std::optional<CollisionResponseResult> collision_response;
    std::optional<CollisionOrientationResult> collision_orientation;
    bool in_air_jump_hold_applied{};
    bool landed{};
    bool velocity_damped{};
    VelocityDampingResult velocity_damping{};
    std::optional<GroundBrakeResult> ground_brake;
    std::optional<AirGravityResult> air_gravity;
    std::optional<AirMotionBasisResult> air_motion_basis;
    std::optional<AirDirectionInputResult> air_direction_input;
    std::optional<OlliePrePhysicsResult> ollie;
    std::optional<GroundMotionResult> ground_motion;
    std::optional<GroundAnimationResult> ground_animation;
    std::optional<GroundMotionThresholdResult> ground_motion_threshold;
    QueuedMotionDrainResult queued_motion{};
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
