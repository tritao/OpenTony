#pragma once

// Replayable native camera contract. This intentionally models the raw state
// and the recovered stage ordering; it is not a gameplay replacement yet.

#include "src/camera/camera_math.hpp"

#include <cstdint>

namespace opentony::camera {

struct Q4Vec3 {
    Raw x{};
    Raw y{};
    Raw z{};
};

struct CameraStateRaw {
    Q16Vec3 position{};
    Q16Vec3 mirrored_anchor{};
    Q16Vec3 anchor_target{};
    Q16Vec3 secondary_anchor{};
    Q16Vec3 look_target{};
    LookAngles look_angles{};

    TransformQ12 current_transform{0, 0, 0, kQ12One};
    TransformQ12 target_transform{0, 0, 0, kQ12One};
    TransformQ12 previous_transform{0, 0, 0, kQ12One};
    TransformQ12 saved_transform{0, 0, 0, kQ12One};
    TransformQ12 cached_render_transform{0, 0, 0, kQ12One};

    // These fields retain the original integer words. Their producer-side
    // units are not all identical until the effect path is fully classified.
    Q16Vec3 screen_effect_offset{};
    // The names below retain the historical reference API. They correspond
    // to the three raw vector blocks used by both follow handlers:
    // +0x5b8, +0x5c4, and +0x610 respectively.
    Q16Vec3 history_a{};   // smoothing vector at +0x5b8
    Q16Vec3 history_b{};   // previous smoothing vector at +0x5c4
    Q16Vec3 mode_vector{}; // camera direction/history vector at +0x610
    Q16Vec3 screen_delta{};

    Raw viewport_parameter_raw{};
    Raw viewport_timer_raw{};
    Raw viewport_parameter_delta_raw{};
    std::uint32_t mode{};
    std::uint32_t update_tick{};
    std::uint32_t death_camera_tick{}; // camera +0x570
    Q16Vec3 death_target_position{};    // camera +0x574..+0x57c
    Q16Vec3 death_start_position{};     // camera +0x594..+0x59c
    std::uint32_t point_camera_tick{};  // camera +0x55c
    Q16Vec3 point_start_position{};     // camera +0x564..+0x56c
    std::uint8_t point_acceleration_flag{}; // camera +0x560
    std::uint32_t follow_distance_counter{};
    std::uint32_t follow_preparation_counter{};
    // Camera +0x5d8. This is an effect-ramp counter, not the follow
    // preparation counter at +0x60c.
    Raw effect_ramp_counter_a{};
    Raw effect_ramp_counter_b{}; // camera +0x5dc
    Raw effect_ramp_counter_c{}; // camera +0x5e0
    Raw effect_ramp_counter_d{}; // camera +0x5e4
    Raw distance_q4{};            // camera +0x5d0
    Raw distance_step_q4{};       // camera +0x61c
    std::array<Raw, 6> distance_history{}; // camera +0x620..+0x634
    // Signed short read at camera +0x5b4 before the mode-1 target transform
    // is composed with the recovered basis.
    std::int16_t follow_rotation_raw{};
    std::uint8_t follow_state_flag{};
    std::uint8_t follow_transition_active{};
    std::uint8_t transform_fallback{};
    std::uint8_t anchor_update_flag{};
    std::uint8_t tripod_anchor_flag{};
    std::int16_t shake_x{};
    std::int16_t shake_y{};
    std::int16_t shake_z{};
    std::uint8_t shake_rate_x{};
    std::uint8_t shake_rate_y{};
    std::uint8_t shake_rate_z{};
    std::uint32_t shake_angle_raw{};
    std::uint16_t shake_phase_raw{};
};

struct CameraTargetRaw {
    Q16Vec3 position{};
    Q16Vec3 follow_offset{};
    Q16Vec3 secondary_position{};
    std::uint32_t tripod_state{};
    std::uint32_t secondary_state{};
    bool has_secondary{};

    // Raw producer flags read by Camera_FollowTarget from the linked tripod.
    // They remain here as inputs rather than being assigned camera semantics:
    // +0x2f64 resets follow state/history counters, while +0x2ddc controls
    // the transition-side producer notification.
    std::uint32_t tripod_behavior_flag{};
    std::uint32_t tripod_effect_gate{};
};

struct CameraViewportCommitRaw {
    Q4Vec3 rendered_position{};
    Q16Vec3 screen_delta{};
    Q4Vec3 rendered_look_target{};
    TransformQ12 current_transform{};
};

struct CameraFollowSnapshot {
    Q16Vec3 anchor_delta{};
    Q16Vec3 follow_offset{};
    LookAngles look_angles{};
    Q12Vec3 direction_raw{};
    Raw offset_vertical_metric_raw{};
    Raw axis_dot_raw{};
    Raw offset_dot_raw{};
    bool transition_branch{};
    bool direction_seed_branch{};
    bool tripod_effect_notification{};
};

// The final tail of Camera_SmoothAndValidate receives these vectors from
// gameplay/effect state, not from the follow-basis builder. They remain raw
// Q16 world words until 0x004e85a0 applies the current Q12 matrix.
struct CameraPositionStageInput {
    Q16Vec3 local_offset{};
    Q16Vec3 effect_vector{};
    bool valid{};
};

struct CameraPositionStageOutput {
    Q16Vec3 position{};
    Q16Vec3 effect_vector{};
};

// State consumed by the normal vertical-effect envelope in
// Camera_SmoothAndValidate. The names follow the recovered offsets; the
// gameplay/effect producer that decides when the special branch is entered is
// represented explicitly rather than folded into these counters.
struct CameraEffectRampStateRaw {
    bool global_override{};       // DAT_0055FA30 != 0
    bool transition_active{};     // camera +0x5d4
    bool tripod_effect_gate{};    // tripod +0x2c68 != 0
    bool follow_state_flag{};     // camera +0x418
    Raw tripod_physics_state{};   // tripod +0x30b8
    Raw distance_step_q4{};       // camera +0x61c
    Raw counter_a{};               // camera +0x5d8
    Raw counter_b{};               // camera +0x5dc
    Raw counter_c{};               // camera +0x5e0
    Raw counter_d{};               // camera +0x5e4
    Raw vertical_effect_q16{};     // DAT_0055F94C
};

struct CameraEffectRampResultRaw {
    bool special_branch{};
    Raw vertical_effect_q16{};
};

inline Raw camera_effect_ramp_delta_q16(Raw strength_q4, Raw phase) {
    const Raw square = multiply_s32(phase, phase);
    const Raw scaled = multiply_s32(strength_q4, square);
    return divide_toward_zero(multiply_s32(scaled, 8000), 0x1e);
}

// Models the non-special branch at 0x0040e803..0x0040e9f4. The special path
// at 0x0040ea53 composes a separate effect transform and needs the unresolved
// gameplay producer, so this function reports it to the caller and leaves the
// vertical value untouched in that case.
inline CameraEffectRampResultRaw advance_camera_effect_ramp(
    CameraEffectRampStateRaw& state) {
    const bool special = state.global_override
        || ((!state.transition_active || !state.tripod_effect_gate)
            && state.counter_a < 1);
    if (special) {
        return {true, state.vertical_effect_q16};
    }

    if (state.counter_a == 0) {
        state.counter_c = state.distance_step_q4;
    }
    if (state.counter_c > 0x1e) {
        state.counter_c = 0x1e;
    }

    const bool ramp_down = !state.transition_active
        || (state.follow_state_flag
            && state.tripod_physics_state != 1
            && state.tripod_physics_state != 2);
    if (ramp_down) {
        Raw phase = state.counter_a;
        if (phase < 1) {
            phase = 0;
            state.counter_b = 1;
            state.counter_a = -1;
            state.counter_d = 0;
        } else {
            if (state.counter_b == 0) {
                if (phase > 7) {
                    phase = 7;
                }
                state.counter_b = add_s32(phase, 1);
            } else {
                phase = subtract_s32(state.counter_b, 1);
            }
            if (phase == 0) {
                state.counter_b = 1;
                state.counter_a = -1;
                state.counter_d = 0;
                return {false, state.vertical_effect_q16};
            }
        }
        state.counter_b = subtract_s32(state.counter_b, 1);
        state.vertical_effect_q16 = add_s32(
            state.vertical_effect_q16,
            camera_effect_ramp_delta_q16(state.counter_c, phase));
    } else {
        state.counter_b = 0;
        if (state.counter_a < 7) {
            if (state.counter_a >= 0) {
                state.counter_d = 0;
                state.vertical_effect_q16 = add_s32(
                    state.vertical_effect_q16,
                    camera_effect_ramp_delta_q16(
                        state.counter_c, state.counter_a));
            }
        } else {
            state.vertical_effect_q16 = add_s32(
                state.vertical_effect_q16,
                divide_toward_zero(
                    multiply_s32(state.counter_c, 0x5fb40), 0x1e));
            state.counter_d = add_s32(state.counter_d, 1);
        }
    }
    return {false, state.vertical_effect_q16};
}

// Base inputs assembled by Camera_SmoothAndValidate before its
// collision/effect branches. The distance is the camera +0x5d0 word in Q4
// world units; the vertical effect is the camera-update DAT_00524a48 word in
// the same source scale. Both are shifted into the Q16 words consumed by
// 0x004e85a0. Branches later in 0x0040e090 may add transition terms to the
// local Z word or replace the effect magnitude.
struct CameraPositionProducerRaw {
    Raw distance_q4{};
    Raw vertical_effect_q4{};
};

// The normal path keeps six raw history words at camera +0x620..+0x634.
// 0x0040e090 turns their sum into the +0x61c step with a compiler-optimized
// integer expression equivalent to:
//
//   ((((sum / 0xcc) * 0x3c) / 2) * 0xe10) / 0x149e >> 12
//
// Keep every multiply at PE32 width and keep the final shift as SAR. The
// history producer is still gameplay/tripod-owned, so this stage accepts the
// six words rather than guessing how they are refreshed.
struct CameraDistanceSmoothingRaw {
    std::array<Raw, 6> history{};
    Raw distance_q4{};
    Raw bias_q4{};
};

struct CameraDistanceAdvanceResultRaw {
    std::array<Raw, 6> history{};
    Raw distance_step_q4{};
    Raw distance_q4{};
    bool history_refreshed{};
};

inline Raw camera_distance_smoothing_step_q4(
    const std::array<Raw, 6>& history) {
    Raw sum = 0;
    for (const Raw value : history) {
        sum = add_s32(sum, value);
    }
    Raw scaled = divide_toward_zero(sum, 0xcc);
    scaled = divide_toward_zero(multiply_s32(scaled, 0x3c), 2);
    scaled = divide_toward_zero(multiply_s32(scaled, 0xe10), 0x149e);
    return arithmetic_shift_right(scaled, 12);
}

inline Raw advance_camera_distance_q4(
    const CameraDistanceSmoothingRaw& smoothing) {
    const Raw step = camera_distance_smoothing_step_q4(smoothing.history);
    return arithmetic_shift_right(
        add_s32(add_s32(smoothing.distance_q4, multiply_s32(step, 4)),
                smoothing.bias_q4),
        1);
}

// The retail refresh at 0x0040e1a0..0x0040e1cf shifts the six-word history
// only for tripod physics states 0 and 4. The opaque vector/collision chain
// supplies the scalar consumed by FUN_004f53b0; its result is accepted here
// as a raw producer value and is shifted left by six exactly as the binary
// does before entering the common distance recurrence.
inline CameraDistanceAdvanceResultRaw advance_camera_distance_smoothing(
    const CameraDistanceSmoothingRaw& smoothing,
    Raw tripod_physics_state,
    Raw history_sample_raw) {
    auto history = smoothing.history;
    const bool refreshed = tripod_physics_state == 0
        || tripod_physics_state == 4;
    if (refreshed) {
        for (std::size_t index = history.size() - 1; index > 0; --index) {
            history[index] = history[index - 1];
        }
        history[0] = wrap_s32(
            static_cast<std::int64_t>(history_sample_raw) * 0x40);
    }
    const Raw step = camera_distance_smoothing_step_q4(history);
    const Raw distance = arithmetic_shift_right(
        add_s32(add_s32(smoothing.distance_q4, multiply_s32(step, 4)),
                smoothing.bias_q4),
        1);
    return {history, step, distance, refreshed};
}

inline CameraDistanceAdvanceResultRaw advance_camera_distance_for_camera(
    CameraStateRaw& camera,
    Raw tripod_physics_state,
    Raw history_sample_raw,
    Raw bias_q4) {
    const auto result = advance_camera_distance_smoothing(
        {camera.distance_history, camera.distance_q4, bias_q4},
        tripod_physics_state,
        history_sample_raw);
    camera.distance_history = result.history;
    camera.distance_step_q4 = result.distance_step_q4;
    camera.distance_q4 = result.distance_q4;
    return result;
}

// Ordered value-level boundary for the normal tripod portion of
// Camera_SmoothAndValidate. The distance history and effect envelope are
// camera-owned state machines; the collision/effect transform producer remains
// an explicit input/result boundary when the special branch is reported.
struct CameraSmoothingStageInputRaw {
    CameraDistanceSmoothingRaw distance{};
    Raw tripod_physics_state{};
    Raw history_sample_raw{};
    CameraEffectRampStateRaw effect{};
    Raw vertical_effect_q4{};
};

struct CameraSmoothingStageOutputRaw {
    CameraDistanceAdvanceResultRaw distance{};
    CameraEffectRampStateRaw effect{};
    CameraEffectRampResultRaw effect_result{};
    CameraPositionStageInput base_position{};
};

inline CameraPositionStageInput build_base_position_stage_input(
    const CameraPositionProducerRaw& producer);

inline CameraSmoothingStageOutputRaw advance_camera_smoothing_stage(
    const CameraSmoothingStageInputRaw& input) {
    CameraSmoothingStageOutputRaw output;
    output.distance = advance_camera_distance_smoothing(
        input.distance, input.tripod_physics_state, input.history_sample_raw);
    output.effect = input.effect;
    output.effect.tripod_physics_state = input.tripod_physics_state;
    output.effect.distance_step_q4 = output.distance.distance_step_q4;
    output.effect_result = advance_camera_effect_ramp(output.effect);
    output.base_position = build_base_position_stage_input({
        output.distance.distance_q4, input.vertical_effect_q4});
    return output;
}

struct CameraDeathPositionResultRaw {
    Q16Vec3 position{};
    std::uint32_t tick{};
    bool initialized{};
    bool completed{};
    bool missing_tripod{};
};

// Camera_DeathMode 0x00410c90 latches the tripod position on tick zero, then
// advances toward the preselected death-camera target for ticks 0..30. The
// retail sequence is component-wise `(start - target) / 30`, multiplied by
// the tick, and subtracted from start; preserve truncation and PE32 wrapping.
inline Q16Vec3 subtract_q16(const Q16Vec3& left, const Q16Vec3& right);

inline CameraDeathPositionResultRaw advance_camera_death_position(
    CameraStateRaw& camera,
    const Q16Vec3& death_target_position,
    const Q16Vec3& tripod_position,
    bool tripod_present) {
    if (!tripod_present) {
        return {camera.position, camera.death_camera_tick, false, false, true};
    }
    if (camera.death_camera_tick == 0) {
        camera.death_target_position = death_target_position;
        camera.death_start_position = tripod_position;
        camera.position = tripod_position;
    }
    if (camera.death_camera_tick < 0x1f) {
        const Q16Vec3 delta = subtract_q16(
            camera.death_start_position, camera.death_target_position);
        const Q16Vec3 per_tick{
            divide_toward_zero(delta.x, 0x1e),
            divide_toward_zero(delta.y, 0x1e),
            divide_toward_zero(delta.z, 0x1e),
        };
        const Q16Vec3 offset{
            multiply_s32(per_tick.x,
                         static_cast<Raw>(camera.death_camera_tick)),
            multiply_s32(per_tick.y,
                         static_cast<Raw>(camera.death_camera_tick)),
            multiply_s32(per_tick.z,
                         static_cast<Raw>(camera.death_camera_tick)),
        };
        camera.position = subtract_q16(camera.death_start_position, offset);
        ++camera.death_camera_tick;
        return {
            camera.position, camera.death_camera_tick,
            camera.death_camera_tick == 1, false, false};
    }
    camera.mode = 1;
    return {
        camera.position, camera.death_camera_tick,
        false, true, false};
}

struct CameraPointPositionResultRaw {
    Q16Vec3 position{};
    std::uint32_t tick{};
    bool completed{};
};

// Camera_PointMode 0x00410f70 uses the same raw vector filter family for its
// position sequence. The recovered path divides the start-target delta by
// 0x82, multiplies by the current +0x55c tick, subtracts from the start
// position, and advances the tick by one (or six after the late acceleration
// flag is set). The point-table producer remains outside this value contract.
inline CameraPointPositionResultRaw advance_camera_point_position(
    CameraStateRaw& camera,
    const Q16Vec3& point_target_position,
    const Q16Vec3& point_start_position,
    bool late_acceleration_enabled,
    Raw duration = 0x82) {
    if (camera.point_camera_tick > static_cast<std::uint32_t>(duration)) {
        camera.mode = 1;
        return {camera.position, camera.point_camera_tick, true};
    }
    if (camera.point_camera_tick == 0) {
        camera.point_start_position = point_start_position;
    }
    const Q16Vec3 delta = subtract_q16(
        camera.point_start_position, point_target_position);
    const Q16Vec3 per_tick{
        divide_toward_zero(delta.x, duration),
        divide_toward_zero(delta.y, duration),
        divide_toward_zero(delta.z, duration),
    };
    const Q16Vec3 offset{
        multiply_s32(per_tick.x,
                     static_cast<Raw>(camera.point_camera_tick)),
        multiply_s32(per_tick.y,
                     static_cast<Raw>(camera.point_camera_tick)),
        multiply_s32(per_tick.z,
                     static_cast<Raw>(camera.point_camera_tick)),
    };
    camera.position = subtract_q16(camera.point_start_position, offset);
    if (camera.point_camera_tick > 10 && late_acceleration_enabled) {
        camera.point_acceleration_flag = 1;
    }
    const Raw increment = camera.point_acceleration_flag != 0 ? 6 : 1;
    camera.point_camera_tick = static_cast<std::uint32_t>(add_s32(
        static_cast<Raw>(camera.point_camera_tick), increment));
    return {camera.position, camera.point_camera_tick, false};
}

// Stateful adapter for the four camera-owned effect counters at
// +0x5d8..+0x5e4. The vertical effect value is a shared producer/global word
// in retail, so it is passed by reference rather than mislocated in the
// camera object.
inline CameraEffectRampResultRaw advance_camera_effects_for_camera(
    CameraStateRaw& camera,
    bool global_override,
    bool tripod_effect_gate,
    Raw tripod_physics_state,
    Raw& vertical_effect_q16) {
    CameraEffectRampStateRaw state{
        global_override,
        camera.follow_transition_active != 0,
        tripod_effect_gate,
        camera.follow_state_flag != 0,
        tripod_physics_state,
        camera.distance_step_q4,
        camera.effect_ramp_counter_a,
        camera.effect_ramp_counter_b,
        camera.effect_ramp_counter_c,
        camera.effect_ramp_counter_d,
        vertical_effect_q16,
    };
    const auto result = advance_camera_effect_ramp(state);
    camera.effect_ramp_counter_a = state.counter_a;
    camera.effect_ramp_counter_b = state.counter_b;
    camera.effect_ramp_counter_c = state.counter_c;
    camera.effect_ramp_counter_d = state.counter_d;
    vertical_effect_q16 = state.vertical_effect_q16;
    return result;
}

inline CameraPositionStageInput build_base_position_stage_input(
    const CameraPositionProducerRaw& producer) {
    return {
        {0, 0, shift_left_12(subtract_s32(0, producer.distance_q4))},
        {0, shift_left_12(producer.vertical_effect_q4), 0},
        true,
    };
}

// The fields below correspond to the operations that are visible in
// Camera_FollowTarget 0x00410610.  Collision/action-state producers are passed
// in by the caller because they belong to gameplay, not camera semantics.
struct CameraFollowInput {
    std::uint32_t tripod_state{};
    std::uint32_t action_state{};
    bool follow_transition_requested{};
    bool transform_fallback{};
    bool collision_distance_valid{};

    // The old field above is retained for source compatibility with the
    // early reference fixtures.  The recovered routine's actual history gate
    // is anchor equality; this explicit override is only for replay inputs
    // that have already applied an opaque collision/history producer.
    bool external_history_override{};

    // Result of 0x004ca8f0. That helper also updates renderer scratch state,
    // so the caller supplies its scalar result instead of hiding it in a
    // guessed camera enum.
    std::int32_t direction_state_result{};

    // Mode 2 has a separate target-preparation handler. These two values
    // expose the raw facts that its static branch consumes without treating
    // the renderer-dependent 0x004ca8f0 result as a camera enum.
    bool mode2_tripod_present{true};
    std::int32_t mode2_view_state{};
};

struct CameraMode2Snapshot {
    Q16Vec3 anchor_delta{};
    Q16Vec3 follow_offset{};
    LookAngles look_angles{};
    Q12Vec3 direction_raw{};
    Raw direction_offset_dot_raw{};
    bool seeded_direction{};
    bool anchors_equal{};
};

struct CameraSmoothingStageInputRaw;
struct CameraSmoothingStageOutputRaw;

struct CameraUpdateHooks {
    // Rebuild the anchor when the original calls the opaque vector/collision
    // chain instead of directly copying the tripod position.
    void (*rebuild_anchor)(CameraStateRaw&, const CameraTargetRaw&){};

    // Apply the remaining game-owned mode-1 target-transform producer after
    // the snapshot has been prepared. A null hook leaves the target transform
    // unchanged rather than substituting a guessed camera behavior.
    void (*apply_follow_transform)(CameraStateRaw&, const CameraFollowSnapshot&){};

    // Camera_SmoothAndValidate contains gameplay-state-dependent branches. A
    // hook can replace the conservative startup/history implementation below.
    void (*smooth_transform)(CameraStateRaw&){};

    // Build the local camera/effect vectors consumed by the post-smoothing
    // 0x004e85a0 tail. Their producers depend on gameplay collision/effect
    // state, so the native camera owns the transform and anchor composition
    // while the caller owns this input boundary.
    void (*prepare_position_stage)(
        CameraStateRaw&, const CameraTargetRaw&, CameraPositionStageInput&){};

    // Build the inputs consumed inside Camera_SmoothAndValidate before the
    // orientation interpolation. The native update applies the returned
    // base position/effect vector after the current transform is available.
    void (*prepare_smoothing_stage)(
        CameraStateRaw&, const CameraTargetRaw&, CameraSmoothingStageInputRaw&){};

    // The special effect branch has an unresolved gameplay/collision producer.
    // This callback observes the exact state-machine result and may commit
    // its producer-owned transform/effect output without hiding it in the
    // camera implementation.
    void (*commit_smoothing_stage)(
        CameraStateRaw&, const CameraSmoothingStageOutputRaw&){};

    // Global DAT_0056a8d4 in the retail update. Keep it configurable until
    // its producer is promoted from the runtime global set.
    std::int32_t shake_phase_multiplier{1};

    // Retail DAT_0056865C is the Q8 simulation delta used by the transform
    // smoothing tail. The normal game initializes it to 0x100; callers can
    // provide a replay/runtime value without hiding the fixed-point contract.
    Raw smoothing_delta_q8{0x100};
};

inline Q16Vec3 subtract_q16(const Q16Vec3& left, const Q16Vec3& right) {
    return {
        subtract_s32(left.x, right.x),
        subtract_s32(left.y, right.y),
        subtract_s32(left.z, right.z),
    };
}

inline Q16Vec3 add_q16(const Q16Vec3& left, const Q16Vec3& right) {
    return {
        add_s32(left.x, right.x),
        add_s32(left.y, right.y),
        add_s32(left.z, right.z),
    };
}

inline Q16Vec3 shift_right_q16(const Q16Vec3& value, unsigned bits) {
    return {
        arithmetic_shift_right(value.x, bits),
        arithmetic_shift_right(value.y, bits),
        arithmetic_shift_right(value.z, bits),
    };
}

inline Q4Vec3 world_to_q4(const Q16Vec3& value) {
    return {sar12_world(value.x), sar12_world(value.y), sar12_world(value.z)};
}

inline Q16Vec3 q4_to_world_delta(const Q4Vec3& value, const Q16Vec3& anchor) {
    return {
        subtract_s32(shift_left_12(value.x), anchor.x),
        subtract_s32(shift_left_12(value.y), anchor.y),
        subtract_s32(shift_left_12(value.z), anchor.z),
    };
}

// Exact value-level form of 0x004c9500. The original stores a raw direction
// record whose Y product is intentionally not shifted after the multiply;
// callers use the same integer scale in their subsequent dot/threshold path.
inline Q12Vec3 direction_from_angles_raw(const LookAngles& angles, Raw scalar) {
    const Raw first_sin = sin_angle_q12(angles.first);
    const Raw first_cos = cos_angle_q12(angles.first);
    const Raw second_sin = sin_angle_q12(angles.second);
    const Raw second_cos = cos_angle_q12(angles.second);
    const Raw horizontal_sin = sar12(multiply_s32(first_sin, scalar));
    const Raw horizontal_cos = sar12(multiply_s32(first_cos, scalar));
    return {
        wrap_s32(-static_cast<std::int64_t>(multiply_s32(horizontal_sin, second_sin))),
        multiply_s32(first_sin, scalar),
        wrap_s32(-static_cast<std::int64_t>(multiply_s32(horizontal_cos, second_cos))),
    };
}

inline Raw absolute_s32(Raw value);

inline Q16Vec3 direction_to_q16(const Q12Vec3& value) {
    return {value.x, value.y, value.z};
}

inline CameraFollowSnapshot prepare_follow_target(
    CameraStateRaw& camera,
    const CameraTargetRaw& target,
    const CameraFollowInput& input) {
    CameraFollowSnapshot snapshot;
    snapshot.anchor_delta = subtract_q16(camera.anchor_target, camera.mirrored_anchor);
    snapshot.follow_offset = target.follow_offset;
    if (camera.mode == 25) {
        snapshot.follow_offset = {0, -0x1000, 0};
    }

    const Q16Vec3 zero{};
    snapshot.look_angles = build_look_angles(snapshot.anchor_delta, zero);
    snapshot.direction_raw = direction_from_angles_raw(snapshot.look_angles, kQ12One);
    const Raw vertical_offset_dot_raw = dot_q12_x87(
        {0, kQ12One, 0},
        {snapshot.follow_offset.x, snapshot.follow_offset.y,
         snapshot.follow_offset.z});
    snapshot.offset_vertical_metric_raw = absolute_s32(
        vertical_offset_dot_raw);
    snapshot.axis_dot_raw = vertical_offset_dot_raw;
    snapshot.offset_dot_raw = dot_q12_x87(snapshot.direction_raw,
                                          {snapshot.follow_offset.x,
                                           snapshot.follow_offset.y,
                                           snapshot.follow_offset.z});

    // 0x00410610 returns early from this branch when the linked tripod is in
    // the producer-reset state.  Preserve the writes visible before the
    // early state tests, while leaving the actual tripod object untouched.
    if (target.tripod_behavior_flag != 0) {
        camera.follow_state_flag = 0;
        camera.effect_ramp_counter_a = 0;
        camera.follow_transition_active = 0;
    }

    camera.follow_distance_counter = add_s32(
        static_cast<Raw>(camera.follow_distance_counter), 1);
    if (target.tripod_state != 1 && target.tripod_state != 2
        && target.tripod_state != 3) {
        camera.follow_state_flag = 0;
    }

    const bool prior_transition = camera.follow_transition_active != 0;
    const bool near_vertical_offset = snapshot.offset_vertical_metric_raw < 0xdf4;
    const bool follow_transition =
        (near_vertical_offset
            && (target.tripod_state == 2 || target.tripod_state == 8
                || camera.follow_state_flag != 0))
        || (prior_transition && target.tripod_state == 1)
        || input.follow_transition_requested;

    if (follow_transition) {
        snapshot.transition_branch = true;
        camera.follow_state_flag = 1;
        if (!prior_transition && target.tripod_effect_gate != 0) {
            snapshot.tripod_effect_notification = true;
        }
        camera.follow_preparation_counter = add_s32(
            static_cast<Raw>(camera.follow_preparation_counter), 1);
        if (camera.follow_preparation_counter < 4) {
            camera.mode_vector = direction_to_q16(snapshot.direction_raw);
            camera.follow_transition_active = 1;
            camera.follow_distance_counter = 0;
        } else {
            camera.mode_vector = {0, 0x1000000, 0};
            camera.follow_transition_active = 1;
            camera.follow_distance_counter = 0;
        }
    } else {
        const Raw direction_metric_band = absolute_s32(snapshot.offset_dot_raw)
            & static_cast<Raw>(0xfffff000U);
        if (direction_metric_band < 0x7d0001
            && input.direction_state_result > 4) {
            snapshot.direction_seed_branch = true;
            camera.follow_preparation_counter = 0;
            camera.mode_vector = direction_to_q16(snapshot.direction_raw);
            if (near_vertical_offset && prior_transition) {
                camera.follow_distance_counter = 0;
            }
        } else {
            camera.follow_preparation_counter = 0;
            if (near_vertical_offset && prior_transition) {
                camera.follow_distance_counter = 0;
            }
        }
        camera.follow_transition_active = 0;
    }
    camera.transform_fallback = input.transform_fallback ? 1 : 0;
    return snapshot;
}

inline Raw absolute_s32(Raw value) {
    return value < 0
        ? wrap_s32(-static_cast<std::int64_t>(value))
        : value;
}

inline TransformQ12 build_follow_target_transform_q12(
    const Q16Vec3& history,
    const Q16Vec3& follow_offset,
    std::int16_t follow_rotation_raw);

// 0x004113f0 is the mode-2 target-preparation handler. It shares the final
// short-basis/transform tail with normal follow, but its input vector is
// maintained at +0x610 and it uses the tripod's +0x310c offset (or the raw
// fallback offset when no tripod is linked). The view-state result is passed
// in because 0x004ca8f0 also updates renderer scratch state, which is outside
// this value-oriented camera contract.
inline CameraMode2Snapshot prepare_mode2_target(
    CameraStateRaw& camera,
    const CameraTargetRaw& target,
    const CameraFollowInput& input) {
    CameraMode2Snapshot snapshot;
    snapshot.anchor_delta = subtract_q16(
        camera.anchor_target, camera.mirrored_anchor);
    snapshot.follow_offset = input.mode2_tripod_present
        ? target.follow_offset
        : Q16Vec3{0, -0x1000, 0};
    snapshot.look_angles = build_look_angles(snapshot.anchor_delta, {});
    snapshot.direction_raw = direction_from_angles_raw(
        snapshot.look_angles, kQ12One);
    snapshot.direction_offset_dot_raw = dot_q12_x87(
        snapshot.direction_raw,
        {snapshot.follow_offset.x, snapshot.follow_offset.y,
         snapshot.follow_offset.z});

    // The retail condition is:
    //   (abs(dot) & 0xfffff000) < 0x7d0001 && view_state > 4
    // followed by Camera_BuildDirection into +0x610. Preserve the unusual
    // mask and strict comparison; this is not a normalized-float distance.
    const Raw dot_band = absolute_s32(snapshot.direction_offset_dot_raw)
        & static_cast<Raw>(0xfffff000U);
    if (dot_band < 0x7d0001 && input.mode2_view_state > 4) {
        camera.mode_vector = {
            snapshot.direction_raw.x,
            snapshot.direction_raw.y,
            snapshot.direction_raw.z};
        snapshot.seeded_direction = true;
    }
    camera.follow_transition_active = 0;

    camera.history_b = camera.history_a;
    snapshot.anchors_equal = camera.anchor_target.x == camera.mirrored_anchor.x
        && camera.anchor_target.y == camera.mirrored_anchor.y
        && camera.anchor_target.z == camera.mirrored_anchor.z;
    if (snapshot.anchors_equal) {
        camera.history_a = camera.history_b;
    } else {
        camera.history_a = shift_right_q16(
            add_q16(camera.history_b, camera.mode_vector), 1);
    }
    camera.mode_vector = camera.history_a;
    camera.target_transform = build_follow_target_transform_q12(
        camera.mode_vector,
        snapshot.follow_offset,
        camera.follow_rotation_raw);
    camera.follow_transition_active = 0;
    return snapshot;
}

inline CameraPositionStageOutput transform_position_stage(
    const CameraStateRaw& camera,
    const CameraPositionStageInput& input) {
    const auto matrix = transform_to_matrix_q12(camera.current_transform);
    const auto transform = [](const MatrixQ12& source, const Q16Vec3& value) {
        return camera_transform_matrix_q12_trunc(
            source, {value.x, value.y, value.z});
    };
    const auto local_offset = transform(matrix, input.local_offset);
    const auto effect_vector = transform(matrix, input.effect_vector);
    return {
        add_q16(camera.anchor_target,
                {local_offset.x, local_offset.y, local_offset.z}),
        {effect_vector.x, effect_vector.y, effect_vector.z},
    };
}

inline void apply_position_stage(
    CameraStateRaw& camera,
    const CameraPositionStageInput& input) {
    if (!input.valid) {
        return;
    }
    const auto output = transform_position_stage(camera, input);
    camera.position = output.position;
    camera.screen_effect_offset = output.effect_vector;
}

inline std::int16_t negate_s16_raw(Raw value) {
    return low_s16_raw(-static_cast<Raw>(low_s16_raw(value)));
}

// The normal-follow tail of 0x00410610 forms two cross products from the
// shifted history vector and the signed-short follow offset, lays those three
// basis vectors into a short 3x3 record, converts that record with 0x004a9a00,
// and finally composes the Y-axis transform from camera +0x5b4.
inline MatrixQ12 build_follow_basis_matrix_q12(
    const Q16Vec3& history,
    const Q16Vec3& follow_offset) {
    const Q12Vec3 history_q4{
        low_s16_raw(arithmetic_shift_right(history.x, 12)),
        low_s16_raw(arithmetic_shift_right(history.y, 12)),
        low_s16_raw(arithmetic_shift_right(history.z, 12)),
    };
    const Q12Vec3 negative_offset{
        negate_s16_raw(follow_offset.x),
        negate_s16_raw(follow_offset.y),
        negate_s16_raw(follow_offset.z),
    };
    const auto first_cross = cross_product_s16(history_q4, negative_offset);
    const auto second_cross = cross_product_s16(negative_offset, first_cross);
    return {
        low_s16_raw(first_cross.x), low_s16_raw(negative_offset.x), low_s16_raw(second_cross.x),
        low_s16_raw(first_cross.y), low_s16_raw(negative_offset.y), low_s16_raw(second_cross.y),
        low_s16_raw(first_cross.z), low_s16_raw(negative_offset.z), low_s16_raw(second_cross.z),
    };
}

inline TransformQ12 build_follow_target_transform_q12(
    const Q16Vec3& history,
    const Q16Vec3& follow_offset,
    std::int16_t follow_rotation_raw) {
    const auto basis = build_follow_basis_matrix_q12(history, follow_offset);
    const auto basis_transform = matrix_to_transform_q12(basis);
    // 0x004a9650 returns `second * first` for its two stack operands. The
    // original pushes the Y constructor as the third operand and the matrix
    // transform as the second operand, hence this argument order.
    return multiply_transform_q12(
        rotation_y_q12(follow_rotation_raw), basis_transform);
}

inline void update_camera_history(
    CameraStateRaw& camera,
    const Q16Vec3& anchor_delta,
    bool force_history_copy) {
    camera.history_b = camera.history_a;
    if (force_history_copy || camera.transform_fallback != 0) {
        camera.history_a = camera.history_b;
    } else {
        // 0x004cabb0 adds history_b and mode_vector; 0x004cacd0 shifts all
        // three words right by one. This is the visible history recurrence.
        (void)anchor_delta;
        camera.history_a = shift_right_q16(add_q16(camera.history_b, camera.mode_vector), 1);
    }
    camera.mode_vector = camera.history_a;
}

inline CameraViewportCommitRaw commit_viewport_effects(
    CameraStateRaw& camera,
    const Q16Vec3& look_target_offset,
    std::uint32_t tripod_state) {
    CameraViewportCommitRaw result;
    const bool special_tripod_path = tripod_state == 5;
    if (special_tripod_path) {
        result.rendered_position = world_to_q4(add_q16(camera.anchor_target, camera.screen_delta));
        camera.current_transform = camera.cached_render_transform;
    } else {
        result.rendered_position = world_to_q4(add_q16(camera.position, camera.screen_effect_offset));
        camera.cached_render_transform = camera.current_transform;
        camera.screen_delta = q4_to_world_delta(result.rendered_position, camera.anchor_target);
    }
    result.screen_delta = camera.screen_delta;
    result.rendered_look_target = world_to_q4(add_q16(camera.look_target, look_target_offset));
    result.current_transform = camera.current_transform;
    return result;
}

inline Raw multiply_q12_truncate(Raw left, Raw right) {
    return divide_toward_zero(multiply_s32(left, right), kQ12One);
}

inline void apply_camera_shake(
    CameraStateRaw& camera,
    std::int32_t phase_multiplier) {
    const Raw phase = wrap_s32(
        static_cast<std::int64_t>(camera.shake_phase_raw) * phase_multiplier)
        & kAngleMask;
    const Raw x_phase = wrap_s32(
        static_cast<std::int64_t>(static_cast<std::uint16_t>(camera.shake_angle_raw))
        * phase_multiplier) & kAngleMask;
    const Raw y_phase = wrap_s32(
        static_cast<std::int64_t>(static_cast<std::uint16_t>(camera.shake_angle_raw >> 16))
        * phase_multiplier) & kAngleMask;

    const auto z_rotation = rotation_z_q12(
        static_cast<std::int16_t>(multiply_q12_truncate(sin_angle_q12(phase), camera.shake_z)));
    const auto x_rotation = rotation_x_q12(
        static_cast<std::int16_t>(multiply_q12_truncate(sin_angle_q12(x_phase), camera.shake_x)));
    const auto y_rotation = rotation_y_q12(
        static_cast<std::int16_t>(multiply_q12_truncate(sin_angle_q12(y_phase), camera.shake_y)));

    // The call sequence at 0x004101ff/0x0041021c/0x00410237 composes the
    // rotations in this order using 0x004a9650.
    const auto y_current = multiply_transform_q12(camera.current_transform, y_rotation);
    const auto x_y_current = multiply_transform_q12(y_current, x_rotation);
    camera.current_transform = multiply_transform_q12(x_y_current, z_rotation);

    const auto old_x = camera.shake_x;
    const auto old_y = camera.shake_y;
    const auto old_z = camera.shake_z;
    const auto decayed_x = decay_shake_axis(old_x, camera.shake_rate_x);
    const auto decayed_y = decay_shake_axis(old_y, camera.shake_rate_y);
    const auto decayed_z = decay_shake_axis(old_z, camera.shake_rate_z);
    camera.shake_x = zero_shake_on_sign_crossing(old_x, decayed_x);
    camera.shake_y = zero_shake_on_sign_crossing(old_y, decayed_y);
    camera.shake_z = zero_shake_on_sign_crossing(old_z, decayed_z);
}

// 0x0040e090 derives its Q12 slerp weight as
// `SAR((DAT_0056865C * 222), 8)`. The multiply is a 32-bit PE32 operation and
// the right shift is an arithmetic shift; do not replace this with a float
// frame-delta or a clamped interpolation coefficient.
inline Raw camera_transform_smoothing_weight_q12(Raw smoothing_delta_q8) {
    return arithmetic_shift_right(
        multiply_s32(smoothing_delta_q8, 222), 8);
}

inline TransformQ12 smooth_camera_transform_q12(
    const TransformQ12& previous,
    const TransformQ12& target,
    Raw smoothing_delta_q8) {
    return slerp_transform_q12(
        previous,
        target,
        camera_transform_smoothing_weight_q12(smoothing_delta_q8));
}

inline void advance_viewport_parameter(CameraStateRaw& camera) {
    const auto timer_low = static_cast<std::uint16_t>(camera.viewport_timer_raw);
    if (timer_low == 0) {
        return;
    }
    camera.viewport_parameter_raw = add_s32(
        camera.viewport_parameter_raw, camera.viewport_parameter_delta_raw);
    camera.viewport_timer_raw = static_cast<Raw>(
        (static_cast<std::uint32_t>(camera.viewport_timer_raw) & 0xffff0000U)
        | static_cast<std::uint16_t>(timer_low - 1));
}

// Conservative raw update ordering. It performs every stage whose data
// contract is established and leaves the gameplay-owned branches behind
// explicit hooks. That makes replay mismatches localizable instead of hiding
// an unverified behavior inside a float-based “camera follow” implementation.
inline CameraViewportCommitRaw update_camera(
    CameraStateRaw& camera,
    const CameraTargetRaw& target,
    const CameraFollowInput& follow_input,
    const Q16Vec3& look_target_offset,
    const CameraUpdateHooks& hooks = {}) {
    if (camera.anchor_update_flag != 0) {
        camera.mirrored_anchor = camera.anchor_target;
        if (camera.tripod_anchor_flag != 0) {
            camera.anchor_target = target.position;
        } else if (hooks.rebuild_anchor != nullptr) {
            hooks.rebuild_anchor(camera, target);
        }
    }
    if (target.has_secondary) {
        camera.secondary_anchor = target.secondary_position;
    }

    CameraFollowSnapshot snapshot;
    if (camera.mode == 1 || camera.mode == 25) {
        snapshot = prepare_follow_target(camera, target, follow_input);
    } else if (camera.mode == 2) {
        (void)prepare_mode2_target(camera, target, follow_input);
    }

    // Camera_FollowTarget updates its history and target transform before
    // Camera_SmoothAndValidate is entered. Keeping this boundary explicit is
    // important: the startup path then consumes the newly prepared target,
    // rather than smoothing one frame behind it.
    if (camera.mode == 1 || camera.mode == 25) {
        const bool anchors_equal = camera.anchor_target.x
                == camera.mirrored_anchor.x
            && camera.anchor_target.y == camera.mirrored_anchor.y
            && camera.anchor_target.z == camera.mirrored_anchor.z;
        update_camera_history(
            camera, snapshot.anchor_delta,
            anchors_equal || follow_input.external_history_override
                || follow_input.collision_distance_valid);
    }
    if (camera.mode == 1 || camera.mode == 25) {
        if (hooks.apply_follow_transform != nullptr) {
            hooks.apply_follow_transform(camera, snapshot);
        } else {
            camera.target_transform = build_follow_target_transform_q12(
                camera.history_a,
                snapshot.follow_offset,
                camera.follow_rotation_raw);
        }
    }
    camera.previous_transform = camera.current_transform;
    CameraPositionStageInput smoothing_position_input;
    bool have_smoothing_position = false;
    if (hooks.prepare_smoothing_stage != nullptr) {
        CameraSmoothingStageInputRaw smoothing_input;
        hooks.prepare_smoothing_stage(camera, target, smoothing_input);
        const auto smoothing_output = advance_camera_smoothing_stage(
            smoothing_input);
        camera.distance_history = smoothing_output.distance.history;
        camera.distance_step_q4 = smoothing_output.distance.distance_step_q4;
        camera.distance_q4 = smoothing_output.distance.distance_q4;
        camera.effect_ramp_counter_a = smoothing_output.effect.counter_a;
        camera.effect_ramp_counter_b = smoothing_output.effect.counter_b;
        camera.effect_ramp_counter_c = smoothing_output.effect.counter_c;
        camera.effect_ramp_counter_d = smoothing_output.effect.counter_d;
        smoothing_position_input = smoothing_output.base_position;
        have_smoothing_position = !smoothing_output.effect_result.special_branch;
        if (hooks.commit_smoothing_stage != nullptr) {
            hooks.commit_smoothing_stage(camera, smoothing_output);
        }
    }
    const bool startup_transform = camera.update_tick < 12;
    if (hooks.smooth_transform != nullptr) {
        hooks.smooth_transform(camera);
    } else if (startup_transform) {
        camera.current_transform = camera.target_transform;
    } else {
        camera.current_transform = smooth_camera_transform_q12(
            camera.previous_transform,
            camera.target_transform,
            hooks.smoothing_delta_q8);
    }
    // Retail refreshes +0x470..+0x47c alongside the startup target copy. On
    // steady-state ticks the entry copy remains in place; the next call
    // refreshes it from the then-current transform before interpolating.
    if (startup_transform) {
        camera.previous_transform = camera.current_transform;
    }
    if (have_smoothing_position) {
        apply_position_stage(camera, smoothing_position_input);
    } else if (hooks.prepare_position_stage != nullptr) {
        CameraPositionStageInput position_input;
        hooks.prepare_position_stage(camera, target, position_input);
        apply_position_stage(camera, position_input);
    }
    camera.look_angles = build_look_angles(camera.look_target, camera.position);
    apply_camera_shake(camera, hooks.shake_phase_multiplier);
    const auto committed = commit_viewport_effects(camera, look_target_offset, target.tripod_state);
    advance_viewport_parameter(camera);
    ++camera.update_tick;
    return committed;
}

} // namespace opentony::camera
