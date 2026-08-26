#include "camera_runtime.hpp"

#include "tests/test_check.hpp"

int main() {
    using namespace opentony::camera;

    CameraRuntime runtime;
    bool rejected = false;
    try {
        (void)runtime.update({}, {});
    } catch (const CameraRuntimeError&) {
        rejected = true;
    }
    CHECK(rejected);

    CameraTargetRaw target{};
    target.position = {0x10000, 0, 0};
    target.follow_offset = {0, 0, -0x4000};
    target.tripod_state = 1;
    runtime.reset(target);
    CHECK(runtime.configured());
    CHECK(runtime.state().mode == 1);
    CHECK(runtime.state().anchor_target.x == 0x10000);

    const auto first = runtime.update(target, {});
    CHECK(runtime.state().update_tick == 1);
    CHECK(runtime.last_commit().current_transform.x == first.current_transform.x);
    CHECK(runtime.last_commit().current_transform.y == first.current_transform.y);
    CHECK(runtime.last_commit().current_transform.z == first.current_transform.z);
    CHECK(runtime.last_commit().current_transform.w == first.current_transform.w);
    CHECK(runtime.state().look_angles.third == 0);
    CHECK(runtime.last_commit().rendered_position.y == 0);
    ViewportInputRaw viewport{};
    viewport.words = {640, 480, 0, 0, 10, 0, 3413, 12, 320, 240, 0, 0, 320, 480};
    CHECK(runtime.prepare_viewport_projection(viewport, 20512, 3413, 3413));
    CHECK(runtime.viewport_projection().viewport.words[5] == 20512);
    CHECK(runtime.viewport_projection().viewport.words[8] == 320);
    CHECK(runtime.viewport_projection().viewport.words[9] == 240);

    CameraMode25ProducerInputRaw mode25{};
    mode25.tripod_present = true;
    mode25.tripod_follow_offset_y_raw = 0x65;
    mode25.tripod_physics_state = 1;
    const auto second = runtime.update(target, {}, {}, {}, {}, mode25);
    CHECK(runtime.state().mode == 25);
    CHECK(second.current_transform.w != 0);

    CameraStateRaw alternate_basis;
    alternate_basis.mode = 25;
    alternate_basis.transform_fallback = 1;
    CameraAlternateFollowInputRaw alternate_basis_input{};
    alternate_basis_input.tripod_present = true;
    alternate_basis_input.tripod_physics_state = 1;
    alternate_basis_input.tripod_follow_offset_y_raw = 0x65;
    alternate_basis_input.tripod_basis_valid = true;
    alternate_basis_input.tripod_basis_q12 = {
        0x1000, 0, 0,
        0, 0, 0x1000,
        0, 0x1000, 0,
    };
    const auto alternate_basis_result = apply_camera_mode25_alternate(
        alternate_basis, alternate_basis_input);
    CHECK(alternate_basis_result.transformed_offset_applied);
    CHECK(alternate_basis.mode_vector.x == -0x1000);
    CHECK(alternate_basis.mode_vector.y == 0);
    CHECK(alternate_basis.mode_vector.z == 0);
    CHECK(alternate_basis.history_b.x == -0x1000 * 0x1000);

    CameraStateRaw alternate_scalar;
    alternate_scalar.mode = 25;
    alternate_scalar.alternate_counter_raw = 1;
    alternate_scalar.alternate_phase_a_raw = 5;
    alternate_scalar.alternate_phase_b_raw = 3;
    alternate_scalar.alternate_shared_angle_raw = 0x1200;
    alternate_scalar.anchor_target.y = 1000;
    CameraAlternateFollowInputRaw alternate_scalar_input{};
    alternate_scalar_input.tripod_present = true;
    alternate_scalar_input.tripod_physics_state = 1;
    alternate_scalar_input.tripod_follow_offset_y_raw = 0x65;
    alternate_scalar_input.tripod_vector_effect_enabled = true;
    alternate_scalar_input.tripod_scalar_raw = 0x2000;
    update_camera(
        alternate_scalar, {}, {}, {}, {}, {}, {}, alternate_scalar_input);
    CHECK(alternate_scalar.alternate_counter_raw == 2);
    CHECK(alternate_scalar.alternate_integrator_raw == -6);
    CHECK(alternate_scalar.alternate_shared_angle_raw == 0x1fe);
    CHECK(alternate_scalar.anchor_target.y == 994);

    CameraRuntime point_runtime;
    point_runtime.reset(target, 23);
    CameraModeInputRaw point{};
    point.point_target_valid = true;
    point.point_target_position = {0x20000, 0, 0};
    point.point_start_valid = true;
    point.point_start_position = target.position;
    const auto point_commit = point_runtime.update(target, {}, {}, {}, point);
    CHECK(point_runtime.state().point_camera_tick == 1);
    CHECK(point_commit.rendered_position.x >= 0);
    return 0;
}
