#pragma once

// Replayable native camera contract. This intentionally models the raw state
// and the recovered stage ordering; it is not a gameplay replacement yet.

#include "camera_math.hpp"
#include "camera_timing.hpp"

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
    // DAT_00524a40/44/48 are shared framing words consumed by the camera
    // update.  They are kept as a raw vector here; their final display
    // meaning is still not promoted to FOV/clip terminology.
    Q16Vec3 framing_globals_raw{};
    std::uint32_t mode{};
    std::uint32_t update_tick{};
    std::uint32_t death_camera_tick{}; // camera +0x570
    Q16Vec3 death_target_position{};    // camera +0x574..+0x57c
    Q16Vec3 death_start_position{};     // camera +0x594..+0x59c
    std::uint32_t point_camera_tick{};  // camera +0x55c
    Q16Vec3 point_start_position{};     // camera +0x564..+0x56c
    std::uint8_t point_acceleration_flag{}; // camera +0x560
    Raw follow_effect_counter_raw{}; // camera +0x5e8
    std::uint32_t follow_preparation_counter{};
    // Camera +0x5d8. This is an effect-ramp counter, not the follow
    // preparation counter at +0x60c.
    Raw effect_ramp_counter_a{};
    Raw effect_ramp_counter_b{}; // camera +0x5dc
    Raw effect_ramp_counter_c{}; // camera +0x5e0
    Raw effect_ramp_counter_d{}; // camera +0x5e4
    // DAT_0055F94C is a shared effect word rather than a field proven to be
    // embedded in the camera object.  Keep a replay mirror here so the
    // default native smoothing adapter can carry it from one update to the
    // next without pretending to own the retail global.
    Raw shared_vertical_effect_q16{};
    // Raw mode-25 alternate-path state.  These are kept separate from the
    // normal effect ramp: the dispatcher updates +0x5ec/+0x5f0 from the
    // linked tripod scalar and may feed +0x5ec into anchor Y.
    Raw alternate_integrator_raw{}; // camera +0x5ec
    Raw alternate_counter_raw{};    // camera +0x5f0
    std::int16_t alternate_phase_a_raw{}; // camera +0x434
    std::int16_t alternate_phase_b_raw{}; // camera +0x436
    Raw distance_q4{};            // camera +0x5d0
    Raw distance_step_q4{};       // camera +0x61c
    std::array<Raw, 6> distance_history{}; // camera +0x620..+0x634
    // PE32 links/control words written by the gameplay camera-point
    // producer. Keep pointers as raw 32-bit values in the replay contract.
    std::uint32_t primary_tripod_link_raw{}; // camera +0x3a4
    std::uint32_t secondary_target_link_raw{}; // camera +0x3dc
    std::uint8_t target_valid_raw{}; // camera +0x3e0
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
    // Tripod +0x4c, consumed by Camera_SmoothAndValidate's distance-history
    // producer. This is intentionally separate from the follow offset at
    // +0x310c; the two vectors serve different camera stages.
    Q16Vec3 distance_sample_offset{};
    bool distance_sample_valid{};
};

struct CameraViewportCommitRaw {
    Q4Vec3 rendered_position{};
    Q16Vec3 screen_delta{};
    Q4Vec3 rendered_look_target{};
    TransformQ12 current_transform{};
    // Camera_Update writes the low 16 bits of +0x40c to the active viewport
    // record at +0x0e before mode dispatch. Preserve that render-visible
    // handoff even though the viewport memory lives outside CameraStateRaw.
    std::uint16_t viewport_parameter_low_raw{};
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

struct CameraDistanceSampleResultRaw {
    Q16Vec3 bounded_offset{};
    Raw quantized_length_q4{};
    Raw sample_raw{};
    bool clamped{};
};

// 0x004ca8f0 quantizes the supplied Q16 vector to signed Q4 words, measures
// that quantized vector, and replaces vectors longer than 100 with the Q16
// vector (100, 0, 0).  The following 0x004f5f90 call then computes the raw
// sample as sqrt((x*x + y*y + z*z) / 0x1000) using the original Q16 words.
// Keeping the two scales distinct is important: the bounded vector is later
// shifted by 0x40 when inserted into camera +0x620.
inline CameraDistanceSampleResultRaw camera_distance_sample_from_q16(
    const Q16Vec3& source_offset) {
    const Raw q4_x = sar12_world(source_offset.x);
    const Raw q4_y = sar12_world(source_offset.y);
    const Raw q4_z = sar12_world(source_offset.z);
    const std::uint64_t q4_square =
        static_cast<std::uint64_t>(static_cast<std::int64_t>(q4_x) * q4_x)
        + static_cast<std::uint64_t>(static_cast<std::int64_t>(q4_y) * q4_y)
        + static_cast<std::uint64_t>(static_cast<std::int64_t>(q4_z) * q4_z);
    const Raw length = static_cast<Raw>(isqrt_u32_x87(
        static_cast<std::uint32_t>(q4_square)));

    const bool clamped = length > 100;
    const Q16Vec3 bounded = clamped
        ? Q16Vec3{100 << 12, 0, 0}
        : source_offset;
    const std::int64_t q16_square =
        static_cast<std::int64_t>(bounded.x) * bounded.x
        + static_cast<std::int64_t>(bounded.y) * bounded.y
        + static_cast<std::int64_t>(bounded.z) * bounded.z;
    const Raw dot_scaled = static_cast<Raw>(q16_square / 0x1000);
    const Raw sample = static_cast<Raw>(isqrt_u32_x87(
        static_cast<std::uint32_t>(dot_scaled)));
    return {bounded, length, sample, clamped};
}

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
    Raw history_sample_raw,
    bool history_sample_valid = true) {
    auto history = smoothing.history;
    const bool refreshed = history_sample_valid
        && (tripod_physics_state == 0 || tripod_physics_state == 4);
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

inline CameraDistanceAdvanceResultRaw
advance_camera_distance_from_tripod_offset(
    CameraStateRaw& camera,
    Raw tripod_physics_state,
    const Q16Vec3& tripod_distance_sample_offset,
    Raw bias_q4,
    CameraDistanceSampleResultRaw* sample_result = nullptr) {
    const auto sample = camera_distance_sample_from_q16(
        tripod_distance_sample_offset);
    if (sample_result != nullptr) {
        *sample_result = sample;
    }
    return advance_camera_distance_for_camera(
        camera, tripod_physics_state, sample.sample_raw, bias_q4);
}

// Ordered value-level boundary for the normal tripod portion of
// Camera_SmoothAndValidate. The distance history and effect envelope are
// camera-owned state machines; the collision/effect transform producer remains
// an explicit input/result boundary when the special branch is reported.
struct CameraSmoothingStageInputRaw {
    CameraDistanceSmoothingRaw distance{};
    Raw tripod_physics_state{};
    Raw history_sample_raw{};
    // Direct callers that do not have a valid tripod sample must not refresh
    // the six-entry history with an invented zero.  Existing value fixtures
    // retain the retail-default behavior by leaving this true.
    bool history_sample_valid{true};
    CameraEffectRampStateRaw effect{};
    Raw vertical_effect_q4{};
};

struct CameraSmoothingStageOutputRaw {
    CameraDistanceAdvanceResultRaw distance{};
    CameraEffectRampStateRaw effect{};
    CameraEffectRampResultRaw effect_result{};
    CameraPositionStageInput base_position{};
};

// Raw producer boundary for the camera-owned portion of
// Camera_SmoothAndValidate 0x0040e090.  The gameplay/world side supplies the
// scalar sample, effect gates, and shared vertical-effect value; the native
// camera owns the exact history/ramp recurrence after those values arrive.
// This intentionally stops before the collision-dependent transform branch
// at 0x004e85a0.
struct CameraSmoothingProducerInputRaw {
    bool valid{};
    bool history_sample_valid{};
    Raw history_sample_raw{};       // result after 0x004f53b0
    Raw distance_bias_q4{};         // DAT_00524A98
    bool global_override{};         // DAT_0055FA30 != 0
    bool tripod_effect_gate{};      // tripod +0x2C68 != 0
    bool vertical_effect_valid{};
    Raw vertical_effect_q16{};      // DAT_0055F94C
};

inline bool build_camera_smoothing_stage_input(
    const CameraStateRaw& camera,
    const CameraTargetRaw& target,
    const CameraSmoothingProducerInputRaw& producer,
    CameraSmoothingStageInputRaw& output) {
    if (!producer.valid) {
        return false;
    }

    output.distance = {
        camera.distance_history,
        camera.distance_q4,
        producer.distance_bias_q4};
    output.tripod_physics_state = static_cast<Raw>(target.tripod_state);
    output.history_sample_raw = producer.history_sample_raw;
    output.history_sample_valid = producer.history_sample_valid;
    output.effect = {
        producer.global_override,
        camera.follow_transition_active != 0,
        producer.tripod_effect_gate,
        camera.follow_state_flag != 0,
        static_cast<Raw>(target.tripod_state),
        camera.distance_step_q4,
        camera.effect_ramp_counter_a,
        camera.effect_ramp_counter_b,
        camera.effect_ramp_counter_c,
        camera.effect_ramp_counter_d,
        producer.vertical_effect_valid
            ? producer.vertical_effect_q16
            : camera.shared_vertical_effect_q16};
    output.vertical_effect_q4 = arithmetic_shift_right(
        output.effect.vertical_effect_q16, 12);
    return true;
}

inline CameraPositionStageInput build_base_position_stage_input(
    const CameraPositionProducerRaw& producer);

inline CameraSmoothingStageOutputRaw advance_camera_smoothing_stage(
    const CameraSmoothingStageInputRaw& input) {
    CameraSmoothingStageOutputRaw output;
    output.distance = advance_camera_distance_smoothing(
        input.distance,
        input.tripod_physics_state,
        input.history_sample_raw,
        input.history_sample_valid);
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

// The mode handlers use the same quaternion interpolator as the normal
// smoothing path, but their weights are produced by unsigned PE32 reciprocal
// multiplies rather than by the normal simulation delta. The point handler
// uses `mul(tick << 12, 0xfc0fc0fd).edx >> 7` and the death handler uses
// `mul(tick << 12, 0x88888889).edx >> 4`.
inline Raw camera_mode_reciprocal_weight_q12(
    std::uint32_t tick, std::uint32_t reciprocal, unsigned post_shift) {
    const auto scaled_tick = static_cast<std::uint32_t>(tick << 12);
    const auto high_word = static_cast<std::uint32_t>(
        (static_cast<std::uint64_t>(scaled_tick) * reciprocal) >> 32);
    return static_cast<Raw>(high_word >> post_shift);
}

inline Raw camera_death_transform_weight_q12(std::uint32_t tick) {
    return camera_mode_reciprocal_weight_q12(tick, 0x88888889U, 4);
}

inline Raw camera_point_transform_weight_q12(std::uint32_t tick) {
    return camera_mode_reciprocal_weight_q12(tick, 0xfc0fc0fdU, 7);
}

// 0x00410c90 and 0x00410f70 both feed 0x004a9bf0, the retail quaternion
// interpolator. Keep the source/target objects explicit: their producers are
// mode-owned and are not interchangeable with normal follow's target basis.
inline TransformQ12 advance_camera_death_transform(
    CameraStateRaw& camera,
    const TransformQ12& source,
    const TransformQ12& target) {
    camera.current_transform = slerp_transform_q12(
        source, target,
        camera_death_transform_weight_q12(camera.death_camera_tick));
    return camera.current_transform;
}

inline TransformQ12 advance_camera_point_transform(
    CameraStateRaw& camera,
    const TransformQ12& source,
    const TransformQ12& target) {
    camera.current_transform = slerp_transform_q12(
        source, target,
        camera_point_transform_weight_q12(camera.point_camera_tick));
    return camera.current_transform;
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

// Inputs owned by the mode/level systems rather than by Camera_Update itself.
// The retail dispatcher sends modes 23 and 24 directly to 0x0040be70 after
// their handlers; they do not pass through the normal smoothing tail. A
// native caller must therefore provide the selected point/death destination
// and, when available, the mode-specific quaternion producer explicitly.
struct CameraModeInputRaw {
    bool tripod_present{};
    Q16Vec3 tripod_position{};
    bool point_target_valid{};
    Q16Vec3 point_target_position{};
    bool point_start_valid{};
    Q16Vec3 point_start_position{};
    bool point_late_acceleration_enabled{};
    bool point_transform_valid{};
    TransformQ12 point_transform_target{};
    bool death_transform_valid{};
    TransformQ12 death_transform_source{};
    TransformQ12 death_transform_target{};
};

// Output of Camera_PointSelect 0x00411fc0. The point-table lookup itself is
// owned by gameplay; this boundary is the exact camera-state handoff after a
// point has been selected. Registry bit 0x800 has precedence over 0x400, as
// in the retail branch structure.
struct CameraPointSelectionInputRaw {
    bool selected{};
    std::uint32_t registry_flags{}; // DAT_00524cb8[selected_index]
    Q16Vec3 selected_point{};
    std::uint32_t player_link_raw{};
    std::int32_t camera_action_variant_raw{};
    // Used only by the 0x400/0x100 distance-scaled viewport branch. The
    // distance producer is gameplay-owned; callers may provide its exact raw
    // result without making the point selector own player geometry.
    bool point_distance_valid{};
    Raw point_distance_raw{};
};

// Exact selector mapping recovered from the byte table at 0x00410390 and
// the target table at 0x00410378.  Values not listed here enter the retail
// diagnostic/default path at 0x00410027; they are not aliases for normal
// follow.  Keep this separate from the semantic names used by callers so a
// future runtime trace can promote a mode without changing dispatch.
enum class CameraDispatchKind : std::uint8_t {
    default_path,
    normal_follow,       // mode 1 -> 0x0040ff2b
    mode2,               // mode 2 -> 0x004113f0
    point,               // mode 23 -> 0x00410f70
    death,               // mode 24 -> 0x00410c90
    alternate_follow,    // mode 25 -> 0x0040feef
};

struct CameraPointSelectionResultRaw {
    bool applied{};
    CameraDispatchKind kind{};
    std::int32_t camera_action_variant_raw{};
    bool viewport_word6_valid{};
    std::uint16_t viewport_word6_raw{};
    bool framing_globals_valid{};
    Q16Vec3 framing_globals_raw{}; // DAT_00524a40/44/48
    bool follow_rotation_updated{};
    std::int16_t follow_rotation_raw{}; // camera +0x5b4
    Raw clamped_point_distance_raw{};
};

constexpr CameraDispatchKind camera_dispatch_kind(std::uint32_t mode) {
    switch (mode) {
    case 1:
        return CameraDispatchKind::normal_follow;
    case 2:
        return CameraDispatchKind::mode2;
    case 23:
        return CameraDispatchKind::point;
    case 24:
        return CameraDispatchKind::death;
    case 25:
        return CameraDispatchKind::alternate_follow;
    default:
        return CameraDispatchKind::default_path;
    }
}

// Exact camera-side writes made by Camera_PointSelect after its gameplay
// point search succeeds. This does not search asset/runtime point tables; it
// makes that ownership explicit while preserving the mode/anchor/link
// contract needed by the subsequent Camera_Update call.
inline CameraPointSelectionResultRaw apply_camera_point_selection(
    CameraStateRaw& camera,
    const CameraPointSelectionInputRaw& input) {
    CameraPointSelectionResultRaw result{
        false, CameraDispatchKind::default_path, 0, false, 0, false,
        {}, false, 0, 0};
    if (!input.selected) {
        return result;
    }

    if ((input.registry_flags & 0x800U) != 0) {
        camera.target_valid_raw = 1;
        camera.secondary_target_link_raw = input.player_link_raw;
        camera.tripod_anchor_flag = 1;
        camera.mode = 1;
        camera.anchor_update_flag = 1;
        camera.primary_tripod_link_raw = input.player_link_raw;
        result.camera_action_variant_raw = input.camera_action_variant_raw;
        result.kind = CameraDispatchKind::normal_follow;
        result.applied = true;

        // The post-selection tail of 0x00411fc0 selects both the vertical
        // viewport input and the follow rotation/framing tuple. Values not
        // listed by the retail switch leave the viewport/framing outputs
        // unchanged after the unconditional rotation reset.
        result.follow_rotation_updated = true;
        result.follow_rotation_raw = 0;
        switch (input.camera_action_variant_raw) {
        case 0:
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = 0x96a;
            result.framing_globals_valid = true;
            result.framing_globals_raw = {0xf5, 0xd7, -0x46};
            break;
        case 1:
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = 0xb72;
            result.framing_globals_valid = true;
            result.framing_globals_raw = {0xaf, 0x32, -0x11};
            break;
        case 2:
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = 0xd52;
            result.framing_globals_valid = true;
            result.framing_globals_raw = {0x122, 0x96, -0x6c};
            break;
        case 4:
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = 0x96a;
            result.framing_globals_valid = true;
            result.framing_globals_raw = {0xf5, 0xd7, -0x46};
            result.follow_rotation_updated = true;
            result.follow_rotation_raw = 0x800;
            break;
        case 5:
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = 0xb72;
            result.framing_globals_valid = true;
            result.framing_globals_raw = {0xc3, 0x32, -0x11};
            result.follow_rotation_updated = true;
            result.follow_rotation_raw = 0x800;
            break;
        case 6:
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = 0xd52;
            result.framing_globals_valid = true;
            result.framing_globals_raw = {0x122, 0x96, -0x6c};
            result.follow_rotation_updated = true;
            result.follow_rotation_raw = 0x800;
            break;
        default:
            break;
        }
        if (result.follow_rotation_updated) {
            camera.follow_rotation_raw = result.follow_rotation_raw;
        }
        if (result.framing_globals_valid) {
            camera.framing_globals_raw = result.framing_globals_raw;
        }
        return result;
    }
    if ((input.registry_flags & 0x400U) != 0) {
        camera.target_valid_raw = 1;
        camera.secondary_target_link_raw = input.player_link_raw;
        camera.tripod_anchor_flag = 1;
        camera.mode = 2;
        camera.anchor_update_flag = 0;
        camera.primary_tripod_link_raw = 0;
        camera.anchor_target = input.selected_point;
        result.kind = CameraDispatchKind::mode2;
        result.applied = true;

        // 0x00411fc0's 0x400 tail has two independent viewport branches.
        // Preserve the registry low byte and the distance clamp exactly;
        // only the distance-scaled branch requires a gameplay-provided
        // distance result.
        if ((input.registry_flags & 0x200U) != 0) {
            const Raw low_byte = static_cast<Raw>(input.registry_flags & 0xffU);
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = static_cast<std::uint16_t>(
                add_s32(0xb2c, multiply_s32(low_byte, -10)));
        } else if ((input.registry_flags & 0x100U) != 0
                   && input.point_distance_valid) {
            Raw distance = input.point_distance_raw;
            if (distance < 0x1c2) {
                distance = 0x1c2;
            } else if (distance > 0x960) {
                distance = 0x960;
            }
            const Raw low_byte = static_cast<Raw>(input.registry_flags & 0xffU);
            result.viewport_word6_valid = true;
            result.viewport_word6_raw = static_cast<std::uint16_t>(
                divide_toward_zero(
                    multiply_s32(low_byte, 0x8552), distance));
            result.clamped_point_distance_raw = distance;
        }
    }
    return result;
}

// The selector's candidate arithmetic is integer Q16 world arithmetic, not a
// normalized offset or a float position.  0x00411fc0 multiplies each raw
// +0x310c word by 0x6e and adds it to the player position with PE32 wrap.
inline Q16Vec3 build_camera_point_candidate_q16(
    const Q16Vec3& player_position,
    const Q16Vec3& camera_offset) {
    return {
        add_s32(player_position.x, multiply_s32(camera_offset.x, 0x6e)),
        add_s32(player_position.y, multiply_s32(camera_offset.y, 0x6e)),
        add_s32(player_position.z, multiply_s32(camera_offset.z, 0x6e)),
    };
}

// 0x004c9590: point-view distance used by the 0x400/0x100 registry branch.
// Differences are reduced from Q16 world words to integer world units before
// the squared length is passed to the retail x87 integer-square-root helper.
inline Raw camera_point_distance_q4(
    const Q16Vec3& left,
    const Q16Vec3& right) {
    const Raw dx = arithmetic_shift_right(subtract_s32(left.x, right.x), 12);
    const Raw dy = arithmetic_shift_right(subtract_s32(left.y, right.y), 12);
    const Raw dz = arithmetic_shift_right(subtract_s32(left.z, right.z), 12);
    const Raw square = add_s32(
        add_s32(multiply_s32(dx, dx), multiply_s32(dy, dy)),
        multiply_s32(dz, dz));
    return static_cast<Raw>(isqrt_u32_x87(static_cast<std::uint32_t>(square)));
}

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

    camera.follow_effect_counter_raw = add_s32(
        camera.follow_effect_counter_raw, 1);
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
            camera.follow_effect_counter_raw = 0;
        } else {
            camera.mode_vector = {0, 0x1000000, 0};
            camera.follow_transition_active = 1;
            camera.follow_effect_counter_raw = 0;
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
                camera.follow_effect_counter_raw = 0;
            }
        } else {
            camera.follow_preparation_counter = 0;
            if (near_vertical_offset && prior_transition) {
                camera.follow_effect_counter_raw = 0;
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
    result.viewport_parameter_low_raw = static_cast<std::uint16_t>(
        camera.viewport_parameter_raw);
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

// The normal-follow continuation at 0x0040ff2b promotes the camera to mode
// 25 after its first smoothing pass when the linked tripod has a positive
// follow-offset Y word above 0x64 and physics state 1.  Keep the producer
// inputs separate from CameraTargetRaw until the runtime ownership of the
// tripod block is fully promoted.
struct CameraMode25ProducerInputRaw {
    bool tripod_present{};
    Raw tripod_follow_offset_y_raw{}; // tripod +0x3110
    Raw tripod_physics_state{};       // tripod +0x30b8
};

inline bool camera_mode25_condition(
    const CameraMode25ProducerInputRaw& input) {
    return input.tripod_present
        && input.tripod_follow_offset_y_raw > 0x64
        && input.tripod_physics_state == 1;
}

// Mode 25 is selected by the normal-follow continuation, but its next
// dispatch enters 0x0040feef rather than 0x0040ff2b.  That alternate path
// performs a normal follow/smoothing pass, applies an optional transformed
// tripod-local offset, then calls Camera_FollowTarget once more before the
// common commit tail.  The matrix transform producing the optional vector is
// 0x004e85a0 over the linked tripod's +0x2e58 short basis and the local
// vector (0, 0, -0x1000).  Keep that producer result injectable until the
// tripod/runtime ownership is independently promoted.
struct CameraAlternateFollowInputRaw {
    bool tripod_present{};
    Raw tripod_physics_state{};       // tripod +0x30b8
    Raw tripod_behavior_flag{};       // tripod +0x2f64
    Raw tripod_follow_offset_y_raw{}; // tripod +0x3110
    bool tripod_vector_effect_enabled{}; // tripod +0x31ec != 0
    Raw tripod_scalar_raw{};             // tripod +0x50
    bool transformed_offset_valid{};
    Q16Vec3 transformed_offset{};     // result of 0x004e85a0
};

struct CameraAlternateFollowResultRaw {
    bool reset_to_normal{};
    bool retained_mode25{};
    bool transformed_offset_applied{};
};

struct CameraAlternateFollowStateResultRaw {
    Raw shared_angle_raw{};
    bool anchor_y_adjusted{};
};

// Exact scalar side effects in 0x0040fdc4..0x0040fe74.  The shared angle is
// DAT_00524a94, so it is deliberately passed by value and returned rather
// than being misrepresented as a camera field.  The two camera phase words
// remain raw signed shorts at +0x434/+0x436.
inline CameraAlternateFollowStateResultRaw advance_camera_mode25_state(
    CameraStateRaw& camera,
    const CameraAlternateFollowInputRaw& input,
    Raw shared_angle_raw) {
    CameraAlternateFollowStateResultRaw result{shared_angle_raw, false};
    if (!input.tripod_present) {
        return result;
    }

    if (input.tripod_vector_effect_enabled
        && camera.alternate_phase_b_raw != 0) {
        if (input.tripod_scalar_raw >= 0) {
            camera.alternate_counter_raw = add_s32(
                camera.alternate_counter_raw, 1);
        } else {
            camera.alternate_counter_raw = subtract_s32(
                camera.alternate_counter_raw, 1);
        }
        const Raw negated_scalar = subtract_s32(0, input.tripod_scalar_raw);
        const Raw scalar_q4 = arithmetic_shift_right(negated_scalar, 12);
        camera.alternate_integrator_raw = add_s32(
            camera.alternate_integrator_raw,
            multiply_s32(scalar_q4, camera.alternate_phase_b_raw));
        if (camera.alternate_counter_raw > 0) {
            camera.anchor_target.y = add_s32(
                camera.anchor_target.y, camera.alternate_integrator_raw);
            result.anchor_y_adjusted = true;
        }
    }

    if (camera.alternate_phase_a_raw != 0) {
        if (input.tripod_scalar_raw <= 0) {
            result.shared_angle_raw = wrap_s32(
                static_cast<std::int64_t>(result.shared_angle_raw)
                - camera.alternate_phase_a_raw);
        } else {
            result.shared_angle_raw = subtract_s32(
                result.shared_angle_raw,
                arithmetic_shift_right(camera.alternate_phase_a_raw, 1));
        }
        result.shared_angle_raw &= 0x0fff;
    }
    return result;
}

inline CameraAlternateFollowResultRaw apply_camera_mode25_alternate(
    CameraStateRaw& camera,
    const CameraAlternateFollowInputRaw& input) {
    CameraAlternateFollowResultRaw result;

    // 0x0040ff08..0x0040ff25: only a linked tripod with physics state other
    // than 1, no behavior flag, and a negative +0x3110 resets mode to 1.
    if (input.tripod_present
        && input.tripod_physics_state != 1
        && input.tripod_behavior_flag == 0
        && input.tripod_follow_offset_y_raw < 0) {
        camera.mode = 1;
        result.reset_to_normal = true;
    }

    // 0x0040ff39..0x0040ff4b is the camera-side mode-25 producer.  It is
    // deliberately evaluated after the reset condition, matching the
    // original branch order.
    if (input.tripod_present
        && input.tripod_follow_offset_y_raw > 0x64
        && input.tripod_physics_state == 1) {
        camera.mode = 25;
        result.retained_mode25 = true;
    }

    // The retail +0x4a9 guard controls this vector path.  The caller supplies
    // the already-transformed vector so the camera contract does not invent a
    // matrix scale or a tripod object layout at this boundary.
    if (camera.transform_fallback != 0 && input.transformed_offset_valid) {
        camera.mode_vector = input.transformed_offset;
        camera.history_b = {
            shift_left_12(input.transformed_offset.x),
            shift_left_12(input.transformed_offset.y),
            shift_left_12(input.transformed_offset.z),
        };
        camera.history_a = input.transformed_offset;
        result.transformed_offset_applied = true;
    }
    return result;
}

// The 0x0040f850 viewport-parameter branch is controlled by globals owned by
// the camera/effect system.  This value-level adapter preserves the observed
// operation order without pretending those globals are camera fields:
// restore from DAT_00524aa4, decrement, increment, reset to 0x100, then apply
// the signed delta while the low timer is nonzero.
struct CameraViewportParameterControlRaw {
    bool restore_from_global{};
    Raw global_parameter_raw{};
    bool decrement{};
    bool increment{};
    bool reset_to_default{};
};

// Raw input/framing controls in Camera_Update 0x0040f850.  The retail code
// executes this block after the viewport-parameter controls and before the
// mode table.  The names describe observed operations and addresses, not
// assumed gameplay meanings.
struct CameraFramingInputControlRaw {
    // DAT_0055FA30 != 0 enables the control block.  The retail branch at
    // 0x0040fa94 jumps over all viewport/framing controls only when this
    // value is zero.
    bool global_override{};
    bool restore_axes_from_globals{}; // DAT_0056B244
    Q16Vec3 restored_axes_raw{}; // x=0055F9A4, y=0055F910, z=0055F978
    bool rotation_decrement{}; // DAT_0056B174, camera +0x5B4 -= 10
    bool rotation_increment{}; // DAT_0056B184, camera +0x5B4 += 10
    std::uint8_t directional_input_raw{}; // DAT_0056B254
    bool x_decrement{}; // DAT_0056B1E4
    bool x_increment{}; // DAT_0056B1F4
    bool y_decrement{}; // DAT_0056B204
    bool y_increment{}; // DAT_0056B214
    bool z_decrement{}; // DAT_0056B1A4
    bool z_increment{}; // DAT_0056B1B4
};

// `neg byte; sbb reg,reg; and reg,0x18; add reg,8` yields 8 when the
// directional byte is zero and 0x20 for every nonzero byte.  It is not a
// signed-byte test despite the input's apparent directional role.
inline Raw camera_framing_step_raw(std::uint8_t directional_input_raw) {
    return directional_input_raw == 0 ? 8 : 0x20;
}

inline void apply_camera_framing_input_control(
    CameraStateRaw& camera,
    const CameraFramingInputControlRaw& control) {
    if (!control.global_override) {
        return;
    }
    if (control.restore_axes_from_globals) {
        camera.framing_globals_raw = control.restored_axes_raw;
    }
    if (control.rotation_decrement) {
        camera.follow_rotation_raw = static_cast<std::int16_t>(
            add_s32(camera.follow_rotation_raw, -10));
    }
    if (control.rotation_increment) {
        camera.follow_rotation_raw = static_cast<std::int16_t>(
            add_s32(camera.follow_rotation_raw, 10));
    }
    const Raw step = camera_framing_step_raw(control.directional_input_raw);
    if (control.x_decrement) {
        camera.framing_globals_raw.x = subtract_s32(
            camera.framing_globals_raw.x, step);
    }
    if (control.x_increment) {
        camera.framing_globals_raw.x = add_s32(
            camera.framing_globals_raw.x, step);
    }
    if (control.y_decrement) {
        camera.framing_globals_raw.y = subtract_s32(
            camera.framing_globals_raw.y, step);
    }
    if (control.y_increment) {
        camera.framing_globals_raw.y = add_s32(
            camera.framing_globals_raw.y, step);
    }
    // 0x0040fbb8 masks the Y framing word after both Y operations.
    camera.framing_globals_raw.y &= 0xfff;
    if (control.z_decrement) {
        camera.framing_globals_raw.z = subtract_s32(
            camera.framing_globals_raw.z, step);
    }
    if (control.z_increment) {
        camera.framing_globals_raw.z = add_s32(
            camera.framing_globals_raw.z, step);
    }
}

inline void apply_viewport_parameter_control(
    CameraStateRaw& camera,
    const CameraViewportParameterControlRaw& control) {
    if (control.restore_from_global) {
        camera.viewport_parameter_raw = control.global_parameter_raw;
    }
    if (control.decrement) {
        camera.viewport_parameter_raw = subtract_s32(
            camera.viewport_parameter_raw, 1);
    }
    if (control.increment) {
        camera.viewport_parameter_raw = add_s32(
            camera.viewport_parameter_raw, 1);
    }
    if (control.reset_to_default) {
        camera.viewport_parameter_raw = 0x100;
    }
    advance_viewport_parameter(camera);
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
    const CameraUpdateHooks& hooks = {},
    const CameraModeInputRaw& mode_input = {},
    const CameraMode25ProducerInputRaw& mode25_input = {},
    const CameraAlternateFollowInputRaw& alternate_follow_input = {},
    const CameraViewportParameterControlRaw& viewport_control = {},
    const CameraFramingInputControlRaw& framing_control = {},
    const CameraSmoothingProducerInputRaw& smoothing_producer = {}) {
    // 0x0040f881..0x0040fc00 runs before the mode table is dispatched. In
    // particular, the restore/decrement/increment/reset operations must also
    // occur for modes 2, 23, and 24; placing this at the old common-tail
    // location silently skipped those paths.
    apply_viewport_parameter_control(camera, viewport_control);
    if (framing_control.global_override) {
        apply_camera_framing_input_control(camera, framing_control);
    }

    const CameraDispatchKind dispatch = camera_dispatch_kind(camera.mode);
    const bool alternate_follow =
        dispatch == CameraDispatchKind::alternate_follow;
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
    if (dispatch == CameraDispatchKind::normal_follow || alternate_follow) {
        snapshot = prepare_follow_target(camera, target, follow_input);
    } else if (dispatch == CameraDispatchKind::mode2) {
        (void)prepare_mode2_target(camera, target, follow_input);
    }

    // Modes 23 and 24 jump from their mode handler directly to
    // Camera_CommitViewportEffects 0x0040be70. Do not run normal follow's
    // history, smoothing, shake, or position producer after them. The
    // destination/transform producers are supplied explicitly because the
    // point table and death-camera setup live outside Camera_Update.
    if (dispatch == CameraDispatchKind::point
        && mode_input.point_target_valid) {
        const Q16Vec3 point_start = mode_input.point_start_valid
            ? mode_input.point_start_position
            : camera.point_start_position;
        (void)advance_camera_point_position(
            camera, mode_input.point_target_position, point_start,
            mode_input.point_late_acceleration_enabled);
        if (mode_input.point_transform_valid) {
            (void)advance_camera_point_transform(
                camera, camera.current_transform,
                mode_input.point_transform_target);
        }
        const auto committed = commit_viewport_effects(
            camera, look_target_offset, target.tripod_state);
        ++camera.update_tick;
        return committed;
    }
    if (dispatch == CameraDispatchKind::death
        && mode_input.tripod_present) {
        (void)advance_camera_death_position(
            camera, camera.death_target_position,
            mode_input.tripod_position, true);
        if (mode_input.death_transform_valid) {
            (void)advance_camera_death_transform(
                camera, mode_input.death_transform_source,
                mode_input.death_transform_target);
        }
        const auto committed = commit_viewport_effects(
            camera, look_target_offset, target.tripod_state);
        ++camera.update_tick;
        return committed;
    }

    // Camera_FollowTarget updates its history and target transform before
    // Camera_SmoothAndValidate is entered. Keeping this boundary explicit is
    // important: the startup path then consumes the newly prepared target,
    // rather than smoothing one frame behind it.
    if (dispatch == CameraDispatchKind::normal_follow || alternate_follow) {
        const bool anchors_equal = camera.anchor_target.x
                == camera.mirrored_anchor.x
            && camera.anchor_target.y == camera.mirrored_anchor.y
            && camera.anchor_target.z == camera.mirrored_anchor.z;
        update_camera_history(
            camera, snapshot.anchor_delta,
            anchors_equal || follow_input.external_history_override
                || follow_input.collision_distance_valid);
    }
    if (dispatch == CameraDispatchKind::normal_follow || alternate_follow) {
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
    } else if (smoothing_producer.valid) {
        // The normal distance/effect state machine is camera-owned once its
        // gameplay producer values have crossed this explicit boundary. Do
        // not require a replacement hook for the recovered common path.
        CameraSmoothingStageInputRaw smoothing_input;
        if (build_camera_smoothing_stage_input(
                camera, target, smoothing_producer, smoothing_input)) {
            const auto smoothing_output = advance_camera_smoothing_stage(
                smoothing_input);
            camera.distance_history = smoothing_output.distance.history;
            camera.distance_step_q4 = smoothing_output.distance.distance_step_q4;
            camera.distance_q4 = smoothing_output.distance.distance_q4;
            camera.effect_ramp_counter_a = smoothing_output.effect.counter_a;
            camera.effect_ramp_counter_b = smoothing_output.effect.counter_b;
            camera.effect_ramp_counter_c = smoothing_output.effect.counter_c;
            camera.effect_ramp_counter_d = smoothing_output.effect.counter_d;
            camera.shared_vertical_effect_q16 =
                smoothing_output.effect.vertical_effect_q16;
            smoothing_position_input = smoothing_output.base_position;
            have_smoothing_position =
                !smoothing_output.effect_result.special_branch;
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

    // The mode-25 handler at 0x0040feef performs the alternate producer
    // between its first smoothing pass and a second Camera_FollowTarget call.
    // There is no second smoothing pass here: the common tail consumes the
    // second target on the next update, matching the retail call order.
    if (alternate_follow) {
        (void)apply_camera_mode25_alternate(
            camera, alternate_follow_input);
        const auto second_snapshot = prepare_follow_target(
            camera, target, follow_input);
        const bool anchors_equal = camera.anchor_target.x
                == camera.mirrored_anchor.x
            && camera.anchor_target.y == camera.mirrored_anchor.y
            && camera.anchor_target.z == camera.mirrored_anchor.z;
        update_camera_history(
            camera, second_snapshot.anchor_delta,
            anchors_equal || follow_input.external_history_override
                || follow_input.collision_distance_valid);
        camera.target_transform = build_follow_target_transform_q12(
            camera.history_a,
            second_snapshot.follow_offset,
            camera.follow_rotation_raw);
    }

    camera.look_angles = build_look_angles(camera.look_target, camera.position);
    apply_camera_shake(camera, hooks.shake_phase_multiplier);
    // 0x0040ff2b..0x0040ff4b promotes the normal-follow camera to mode 25
    // after the mode-1 preparation/smoothing work above. The next update
    // therefore consumes the mode-25 follow offset; do not feed this result
    // back into the current frame's mode dispatch.
    if (camera.mode == 1 && camera_mode25_condition(mode25_input)) {
        camera.mode = 25;
    }
    const auto committed = commit_viewport_effects(camera, look_target_offset, target.tripod_state);
    ++camera.update_tick;
    return committed;
}

} // namespace opentony::camera
