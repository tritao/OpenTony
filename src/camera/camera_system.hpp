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
    Q16Vec3 history_a{};
    Q16Vec3 history_b{};
    Q16Vec3 mode_vector{};
    Q16Vec3 screen_delta{};

    Raw viewport_parameter_raw{};
    Raw viewport_timer_raw{};
    Raw viewport_parameter_delta_raw{};
    std::uint32_t mode{};
    std::uint32_t update_tick{};
    std::uint32_t follow_distance_counter{};
    std::uint32_t follow_preparation_counter{};
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
    Raw axis_dot_raw{};
    Raw offset_dot_raw{};
};

// The fields below correspond to the operations that are visible in
// Camera_FollowTarget 0x00410610.  Collision/action-state producers are passed
// in by the caller because they belong to gameplay, not camera semantics.
struct CameraFollowInput {
    std::uint32_t tripod_state{};
    std::uint32_t action_state{};
    bool follow_transition_requested{};
    bool transform_fallback{};
    bool collision_distance_valid{};
};

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

    // Global DAT_0056a8d4 in the retail update. Keep it configurable until
    // its producer is promoted from the runtime global set.
    std::int32_t shake_phase_multiplier{1};
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
    snapshot.axis_dot_raw = dot_q12_x87({0, kQ12One, 0},
                                        {snapshot.follow_offset.x,
                                         snapshot.follow_offset.y,
                                         snapshot.follow_offset.z});
    snapshot.offset_dot_raw = dot_q12_x87(snapshot.direction_raw,
                                          {snapshot.follow_offset.x,
                                           snapshot.follow_offset.y,
                                           snapshot.follow_offset.z});

    ++camera.follow_distance_counter;
    if ((input.action_state != 1 && input.action_state != 2 && input.action_state != 3)
        && !camera.follow_transition_active) {
        camera.follow_state_flag = 0;
    }
    if (input.follow_transition_requested) {
        camera.follow_state_flag = 1;
        ++camera.follow_preparation_counter;
        if (camera.follow_preparation_counter < 4) {
            camera.follow_transition_active = 1;
            camera.follow_distance_counter = 0;
        } else {
            camera.mode_vector = {0, 0x1000000, 0};
            camera.follow_transition_active = 1;
            camera.follow_distance_counter = 0;
        }
    } else {
        camera.follow_preparation_counter = 0;
        camera.follow_transition_active = 0;
    }
    camera.transform_fallback = input.transform_fallback ? 1 : 0;
    return snapshot;
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
    bool use_external_history) {
    camera.history_b = camera.history_a;
    if (use_external_history || camera.transform_fallback != 0) {
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
    }

    // Camera_FollowTarget updates its history and target transform before
    // Camera_SmoothAndValidate is entered. Keeping this boundary explicit is
    // important: the startup path then consumes the newly prepared target,
    // rather than smoothing one frame behind it.
    update_camera_history(camera, snapshot.anchor_delta, follow_input.collision_distance_valid);
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
    if (hooks.smooth_transform != nullptr) {
        hooks.smooth_transform(camera);
    } else if (camera.update_tick < 12) {
        camera.current_transform = camera.target_transform;
    }
    camera.look_angles = build_look_angles(camera.look_target, camera.position);
    apply_camera_shake(camera, hooks.shake_phase_multiplier);
    const auto committed = commit_viewport_effects(camera, look_target_offset, target.tripod_state);
    advance_viewport_parameter(camera);
    ++camera.update_tick;
    return committed;
}

} // namespace opentony::camera
