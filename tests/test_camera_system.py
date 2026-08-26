import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest


def test_camera_system_reference_compiles_and_preserves_stage_order(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is not installed")
    source = tmp_path / "camera_system_smoke.cpp"
    source.write_text(
                textwrap.dedent(
                    """
            #include "src/camera/camera_system.hpp"

            int main() {
                using namespace opentony::camera;
                CameraStateRaw camera;
                camera.mode = 25;
                camera.anchor_update_flag = 1;
                camera.tripod_anchor_flag = 1;
                camera.anchor_target = {0x10000, 0, 0};
                camera.position = {0x10000, 0, 0};
                camera.target_transform = {0, 0, 0, 0x1000};
                camera.viewport_parameter_raw = 10;
                camera.viewport_parameter_delta_raw = 3;
                camera.viewport_timer_raw = 2;

                const CameraTargetRaw target{
                    {0x20000, 0, 0}, {0x111, 0x222, 0x333}, {}, 1, 0, false};
                const CameraFollowInput follow{1, 1, true, false, false};
                const auto expected_follow = build_follow_target_transform_q12(
                    {}, {0, -0x1000, 0}, 0);
                const auto committed = update_camera(
                    camera, target, follow, {}, {});
                if (camera.mirrored_anchor.x != 0x10000
                    || camera.anchor_target.x != 0x20000
                    || camera.update_tick != 1
                    || camera.current_transform.x != expected_follow.x
                    || camera.current_transform.y != expected_follow.y
                    || camera.current_transform.z != expected_follow.z
                    || camera.current_transform.w != expected_follow.w
                    || camera.viewport_parameter_raw != 13
                    || static_cast<unsigned>(camera.viewport_timer_raw & 0xffff) != 1
                    || committed.rendered_position.x != 16) {
                    return 1;
                }

                // Mode 2 has its own handler.  This fixture chooses an
                // anchor direction orthogonal to the supplied offset so the
                // retail dot-band/view-state seed branch is exercised.
                CameraStateRaw mode2;
                mode2.mode = 2;
                mode2.mirrored_anchor = {};
                mode2.anchor_target = {0, 0, 0x10000};
                const CameraTargetRaw mode2_target{
                    {}, {0x1000, 0, 0}, {}, 0, 0, false};
                CameraFollowInput mode2_input{};
                mode2_input.mode2_view_state = 5;
                const auto mode2_snapshot = prepare_mode2_target(
                    mode2, mode2_target, mode2_input);
                const auto mode2_expected = build_follow_target_transform_q12(
                    mode2.mode_vector, mode2_target.follow_offset, 0);
                if (!mode2_snapshot.seeded_direction
                    || mode2_snapshot.follow_offset.x != 0x1000
                    || mode2.mode_vector.z
                        != arithmetic_shift_right(mode2_snapshot.direction_raw.z, 1)
                    || mode2.history_b.z != 0
                    || mode2.history_a.z != mode2.mode_vector.z
                    || mode2.follow_transition_active != 0
                    || mode2.target_transform.x != mode2_expected.x
                    || mode2.target_transform.y != mode2_expected.y
                    || mode2.target_transform.z != mode2_expected.z
                    || mode2.target_transform.w != mode2_expected.w) {
                    return 15;
                }

                CameraStateRaw mode2_fallback;
                mode2_fallback.mode = 2;
                mode2_fallback.anchor_target = {};
                mode2_fallback.mirrored_anchor = {};
                CameraFollowInput mode2_fallback_input{};
                mode2_fallback_input.mode2_tripod_present = false;
                const auto fallback_snapshot = prepare_mode2_target(
                    mode2_fallback, {}, mode2_fallback_input);
                if (fallback_snapshot.follow_offset.x != 0
                    || fallback_snapshot.follow_offset.y != -0x1000
                    || fallback_snapshot.follow_offset.z != 0
                    || fallback_snapshot.seeded_direction) {
                    return 16;
                }

                camera.history_a = {0x200, 0, 0};
                camera.mode_vector = {0x100, 0, 0};
                update_camera_history(camera, {}, true);
                if (camera.history_b.x != 0x200 || camera.history_a.x != 0x200
                    || camera.mode_vector.x != 0x200) {
                    return 2;
                }

                CameraStateRaw shake;
                shake.shake_x = 0x1000;
                shake.shake_angle_raw = 0x00000400;
                const auto expected_shake = rotation_x_q12(0x0fff);
                apply_camera_shake(shake, 1);
                if (shake.current_transform.x != expected_shake.x
                    || shake.current_transform.y != expected_shake.y
                    || shake.current_transform.z != expected_shake.z
                    || shake.current_transform.w != expected_shake.w
                    || shake.shake_x != 0x1000) {
                    return 3;
                }
                return 0;
            }
            """
        ),
        encoding="utf-8",
    )
    result = subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-I",
            str(Path(__file__).resolve().parents[1]),
            str(source),
            "-o",
            str(tmp_path / "camera_system_smoke"),
        ],
        cwd=Path(__file__).resolve().parents[1],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run([str(tmp_path / "camera_system_smoke")], capture_output=True, text=True, check=False)
    assert run.returncode == 0, run.stderr


def test_follow_basis_fixture_covers_raw_history_and_s16_saturation(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is not installed")
    source = tmp_path / "camera_follow_fixture.cpp"
    source.write_text(
        textwrap.dedent(
            """
            #include "src/camera/camera_system.hpp"

            void prepare_position(
                opentony::camera::CameraStateRaw&,
                const opentony::camera::CameraTargetRaw&,
                opentony::camera::CameraPositionStageInput& input) {
                input.local_offset = {0x1000, 0x2000, 0x3000};
                input.effect_vector = {0x4000, 0x5000, 0x6000};
                input.valid = true;
            }

            int main() {
                using namespace opentony::camera;
                const Q16Vec3 follow_offset{0, -0x1000, 0};

                // History is shifted with x86 SAR before it is narrowed to
                // the signed-short basis.  The first case is the unscaled
                // canonical follow basis and is intentionally checked
                // independently of matrix_to_transform_q12.
                const auto canonical = build_follow_basis_matrix_q12(
                    {0x1000, 0, 0}, follow_offset);
                const MatrixQ12 expected_canonical{
                    0, 0, 0x7fff,
                    0, 0x1000, 0,
                    0x1000, 0, 0};
                if (canonical != expected_canonical) {
                    return 1;
                }

                // A longer history exercises the original saturating-short
                // boundary on the second cross product.
                const auto saturated = build_follow_basis_matrix_q12(
                    {0, 0, 0x2000}, follow_offset);
                const MatrixQ12 expected_saturated{
                    -0x2000, 0, 0,
                    0, 0x1000, 0,
                    0, 0, 0x7fff};
                if (saturated != expected_saturated) {
                    return 2;
                }

                // Negative Q16 history must use arithmetic SAR, not C++'s
                // truncating division, before the signed-short conversion.
                const auto negative = build_follow_basis_matrix_q12(
                    {-0x1001, 0, 0}, follow_offset);
                const MatrixQ12 expected_negative{
                    0, 0, static_cast<std::int16_t>(-0x8000),
                    0, 0x1000, 0,
                    -0x2000, 0, 0};
                if (negative != expected_negative) {
                    return 3;
                }

                // The multi-frame contract prepares the target from the
                // updated history before the startup copy consumes it.
                CameraStateRaw camera;
                camera.mode = 1;
                camera.anchor_update_flag = 1;
                camera.tripod_anchor_flag = 1;
                camera.anchor_target = {0x1000, 0, 0};
                camera.position = camera.anchor_target;
                const CameraTargetRaw target{
                    {0x1000, 0, 0}, follow_offset, {}, 0, 0, false};
                const CameraFollowInput follow{};
                const auto first = update_camera(camera, target, follow, {}, {});
                const auto first_expected = build_follow_target_transform_q12(
                    {0, 0, 0}, follow_offset, 0);
                if (camera.current_transform.x != first_expected.x
                    || camera.current_transform.y != first_expected.y
                    || camera.current_transform.z != first_expected.z
                    || camera.current_transform.w != first_expected.w
                    || first.current_transform.w != first_expected.w) {
                    return 4;
                }

                camera.mode_vector = {0, 0, 0x2000};
                const CameraTargetRaw moved_target{
                    {0x2000, 0, 0}, follow_offset, {}, 0, 0, false};
                const auto second = update_camera(
                    camera, moved_target, follow, {}, {});
                const auto second_expected = build_follow_target_transform_q12(
                    {0, 0, 0x1000}, follow_offset, 0);
                if (camera.history_b.z != 0
                    || camera.history_a.z != 0x1000
                    || second.current_transform.x != second_expected.x
                    || second.current_transform.y != second_expected.y
                    || second.current_transform.z != second_expected.z
                    || second.current_transform.w != second_expected.w) {
                    return 5;
                }

                CameraStateRaw positioned;
                positioned.anchor_target = {0x10000, 0x20000, 0x30000};
                CameraUpdateHooks stage_hooks{};
                stage_hooks.prepare_position_stage = prepare_position;
                update_camera(positioned, {}, {}, {}, stage_hooks);
                if (positioned.position.x != 0x12000
                    || positioned.position.y != 0x23000
                    || positioned.position.z != 0x31000
                    || positioned.screen_effect_offset.x != 0x5000
                    || positioned.screen_effect_offset.y != 0x6000
                    || positioned.screen_effect_offset.z != 0x4000) {
                    return 6;
                }

                const auto base_position = build_base_position_stage_input(
                    {197, -140});
                if (base_position.local_offset.x != 0
                    || base_position.local_offset.y != 0
                    || base_position.local_offset.z != -197 * 0x1000
                    || base_position.effect_vector.x != 0
                    || base_position.effect_vector.y != -140 * 0x1000
                    || base_position.effect_vector.z != 0
                    || !base_position.valid) {
                    return 7;
                }

                const std::array<Raw, 6> history = {
                    0x100000, 0x100000, 0x100000,
                    0x100000, 0x100000, 0x100000};
                const Raw step = camera_distance_smoothing_step_q4(history);
                if (step != -45) {
                    return 8;
                }
                if (advance_camera_distance_q4({history, 197, -3})
                    != 7) {
                    return 9;
                }

                if (camera_transform_smoothing_weight_q12(0x100) != 222) {
                    return 10;
                }
                const TransformQ12 prior_transform{0, 0, 0, 0x1000};
                const TransformQ12 next_transform{0, 0x1000, 0, 0};
                const auto expected_smoothed = smooth_camera_transform_q12(
                    prior_transform, next_transform, 0x100);
                CameraStateRaw smoothed;
                smoothed.mode = 1;
                smoothed.update_tick = 12;
                smoothed.current_transform = prior_transform;
                smoothed.target_transform = next_transform;
                CameraUpdateHooks smoothing_hooks{};
                smoothing_hooks.smoothing_delta_q8 = 0x100;
                smoothing_hooks.apply_follow_transform =
                    [](CameraStateRaw&, const CameraFollowSnapshot&) {};
                update_camera(smoothed, {}, {}, {}, smoothing_hooks);
                if (smoothed.current_transform.x != expected_smoothed.x
                    || smoothed.current_transform.y != expected_smoothed.y
                    || smoothed.current_transform.z != expected_smoothed.z
                    || smoothed.current_transform.w != expected_smoothed.w
                    || smoothed.previous_transform.x != prior_transform.x
                    || smoothed.previous_transform.w != prior_transform.w) {
                    return 11;
                }

                CameraEffectRampStateRaw ramp{
                    false, true, true, false, 0, 3,
                    1, 0, 3, 0, 100};
                const auto ramp_result = advance_camera_effect_ramp(ramp);
                if (ramp_result.special_branch
                    || ramp.counter_c != 3
                    || ramp.vertical_effect_q16 != 900) {
                    return 12;
                }
                ramp.counter_a = 7;
                ramp.counter_b = 0;
                const auto max_result = advance_camera_effect_ramp(ramp);
                if (max_result.special_branch
                    || ramp.counter_b != 0
                    || ramp.counter_d != 1
                    || ramp.vertical_effect_q16
                        != add_s32(900, divide_toward_zero(
                            multiply_s32(3, 0x5fb40), 0x1e))) {
                    return 13;
                }
                CameraEffectRampStateRaw special_ramp{
                    false, false, false, false, 0, 3,
                    0, 0, 0, 0, 100};
                if (!advance_camera_effect_ramp(special_ramp).special_branch) {
                    return 14;
                }

                // Camera_FollowTarget's transition branch uses the raw
                // vertical-offset threshold and tripod state, then advances
                // the separate +0x60c preparation counter.  The first three
                // entries seed the direction; the fourth uses the retail
                // fallback vector.
                CameraStateRaw follow_state;
                follow_state.mode = 1;
                const CameraTargetRaw transition_target{
                    {}, {0, -0x100, 0}, {}, 2, 0, false, 0, 1};
                CameraFollowInput transition_input{};
                const auto transition = prepare_follow_target(
                    follow_state, transition_target, transition_input);
                if (!transition.transition_branch
                    || !transition.tripod_effect_notification
                    || follow_state.follow_state_flag != 1
                    || follow_state.follow_transition_active != 1
                    || follow_state.follow_preparation_counter != 1
                    || follow_state.follow_distance_counter != 0
                    || follow_state.mode_vector.x
                        != transition.direction_raw.x
                    || follow_state.mode_vector.y
                        != transition.direction_raw.y
                    || follow_state.mode_vector.z
                        != transition.direction_raw.z) {
                    return 15;
                }
                follow_state.follow_preparation_counter = 3;
                follow_state.follow_transition_active = 0;
                const CameraTargetRaw fourth_transition{
                    {}, {0, -0x100, 0}, {}, 2, 0, false, 0, 0};
                const auto fourth = prepare_follow_target(
                    follow_state, fourth_transition, transition_input);
                if (!fourth.transition_branch
                    || follow_state.follow_preparation_counter != 4
                    || follow_state.mode_vector.x != 0
                    || follow_state.mode_vector.y != 0x1000000
                    || follow_state.mode_vector.z != 0) {
                    return 16;
                }

                // A producer reset clears +0x5d8 and transition state, while
                // the normal state-2 branch may immediately re-enter the
                // transition path.
                follow_state.effect_ramp_counter_a = 77;
                const CameraTargetRaw reset_target{
                    {}, {0, -0x100, 0}, {}, 2, 0, false, 1, 0};
                prepare_follow_target(
                    follow_state, reset_target, transition_input);
                if (follow_state.effect_ramp_counter_a != 0
                    || follow_state.follow_transition_active != 1) {
                    return 17;
                }

                // The alternate direction-seed branch is gated by the
                // renderer-backed scalar result, not by a guessed enum.
                CameraStateRaw seeded_state;
                seeded_state.mode = 1;
                CameraFollowInput seeded_input{};
                seeded_input.direction_state_result = 5;
                const auto seeded = prepare_follow_target(
                    seeded_state, {}, seeded_input);
                if (!seeded.direction_seed_branch
                    || seeded_state.follow_transition_active != 0
                    || seeded_state.mode_vector.x != seeded.direction_raw.x
                    || seeded_state.mode_vector.y != seeded.direction_raw.y
                    || seeded_state.mode_vector.z != seeded.direction_raw.z) {
                    return 18;
                }
                return 0;
            }
            """
        ),
        encoding="utf-8",
    )
    result = subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-I",
            str(Path(__file__).resolve().parents[1]),
            str(source),
            "-o",
            str(tmp_path / "camera_follow_fixture"),
        ],
        cwd=Path(__file__).resolve().parents[1],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run(
        [str(tmp_path / "camera_follow_fixture")],
        capture_output=True,
        text=True,
        check=False,
    )
    assert run.returncode == 0, run.stderr
