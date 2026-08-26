import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest


def test_camera_frame_contract_keeps_present_explicit(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is not installed")
    source = tmp_path / "camera_frame_contract.cpp"
    source.write_text(
        textwrap.dedent(
            """
            #include "src/camera/camera_frame.hpp"

            int main() {
                using namespace opentony::camera;
                CameraFrameStateRaw state;
                CameraFrameInputRaw input;
                input.timestamp_ms = 1000;
                    input.render_input.viewport.words = {
                        640, 480, 0, 0, 0, 20512, 3410, 12,
                    320, 240, 0, 0, 320, 480};
                input.render_input.scale_x = 0x1000;
                input.render_input.scale_y = 0x1000;
                input.target.tripod_state = 2;
                input.target.follow_offset = {0, -0x1000, 0};

                auto first = advance_camera_frame(state, input);
                if (!first.camera_updated || !first.render_prepared
                    || first.presented
                    || state.camera_timing.simulation_delta_q8 != 42) {
                    return 1;
                }
                if (!present_camera_frame(state, first)
                    || first.presented_frame_serial != 1
                    || present_camera_frame(state, first)) {
                    return 2;
                }

                input.timestamp_ms = 1017;
                auto second = advance_camera_frame(state, input);
                if (second.simulation_clock.tick_delta != 1
                    || !present_camera_frame(state, second)
                    || state.presented_frame_serial != 2) {
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
            str(tmp_path / "camera_frame_contract"),
        ],
        cwd=Path(__file__).resolve().parents[1],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run(
        [str(tmp_path / "camera_frame_contract")],
        capture_output=True,
        text=True,
        check=False,
    )
    assert run.returncode == 0, run.stderr


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
                    camera, target, follow, {}, {}, {}, {}, {}, {}, {true});
                if (camera.mirrored_anchor.x != 0x10000
                    || camera.anchor_target.x != 0x20000
                    || camera.update_tick != 1
                    || camera.current_transform.x != expected_follow.x
                    || camera.current_transform.y != expected_follow.y
                    || camera.current_transform.z != expected_follow.z
                    || camera.current_transform.w != expected_follow.w
                    || camera.viewport_parameter_raw != 13
                    || committed.viewport_parameter_low_raw != 13
                    || static_cast<unsigned>(camera.viewport_timer_raw & 0xffff) != 1
                    || committed.rendered_position.x != 16) {
                    return 1;
                }

                if (camera_mode25_condition({false, 0x65, 1})
                    || camera_mode25_condition({true, 0x64, 1})
                    || camera_mode25_condition({true, 0x65, 0})
                    || !camera_mode25_condition({true, 0x65, 1})) {
                    return 36;
                }

                if (camera_dispatch_kind(1)
                        != CameraDispatchKind::normal_follow
                    || camera_dispatch_kind(2)
                        != CameraDispatchKind::mode2
                    || camera_dispatch_kind(22)
                        != CameraDispatchKind::default_path
                    || camera_dispatch_kind(23)
                        != CameraDispatchKind::point
                    || camera_dispatch_kind(24)
                        != CameraDispatchKind::death
                    || camera_dispatch_kind(25)
                        != CameraDispatchKind::alternate_follow) {
                    return 40;
                }

                CameraStateRaw alternate;
                alternate.mode = 25;
                alternate.transform_fallback = 1;
                const auto alternate_result =
                    apply_camera_mode25_alternate(
                        alternate,
                        {true, 1, 0, 0x65, false, 0,
                         true, {0x1000, -0x2000, 0x3000}});
                if (!alternate_result.retained_mode25
                    || !alternate_result.transformed_offset_applied
                    || alternate.mode != 25
                    || alternate.mode_vector.x != 0x1000
                    || alternate.history_a.y != -0x2000
                    || alternate.history_b.x != 0x1000 * 0x1000
                    || alternate.history_b.y != -0x2000 * 0x1000
                    || alternate.history_b.z != 0x3000 * 0x1000) {
                    return 41;
                }

                // When the caller provides the tripod's raw short basis,
                // mode 25 owns the 0x004e85a0 transform of (0,0,-0x1000).
                // Use a non-identity basis so the helper's row order is
                // covered instead of accepting a copied seam.
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
                const auto alternate_basis_result =
                    apply_camera_mode25_alternate(
                        alternate_basis, alternate_basis_input);
                if (!alternate_basis_result.transformed_offset_applied
                    || alternate_basis.mode_vector.x != -0x1000
                    || alternate_basis.mode_vector.y != 0
                    || alternate_basis.mode_vector.z != 0
                    || alternate_basis.history_a.x != -0x1000
                    || alternate_basis.history_b.x != -0x1000 * 0x1000) {
                    return 55;
                }
                CameraStateRaw alternate_reset;
                alternate_reset.mode = 25;
                const auto alternate_reset_result =
                    apply_camera_mode25_alternate(
                        alternate_reset,
                        {true, 2, 0, -1, false, 0, false, {}});
                if (!alternate_reset_result.reset_to_normal
                    || alternate_reset.mode != 1) {
                    return 42;
                }

                CameraStateRaw alternate_state;
                alternate_state.alternate_phase_a_raw = 5;
                alternate_state.alternate_phase_b_raw = 3;
                alternate_state.alternate_counter_raw = 0;
                alternate_state.anchor_target.y = 1000;
                const auto alternate_state_result =
                    advance_camera_mode25_state(
                        alternate_state,
                        {true, 1, 0, 0, true, -0x2000, false, {}},
                        0x1200);
                if (alternate_state.alternate_counter_raw != -1
                    || alternate_state.alternate_integrator_raw != 6
                    || alternate_state.anchor_target.y != 1000
                    || alternate_state_result.shared_angle_raw != 0x1fb
                    || alternate_state_result.anchor_y_adjusted) {
                    return 43;
                }
                alternate_state.alternate_counter_raw = 1;
                const auto alternate_state_positive =
                    advance_camera_mode25_state(
                        alternate_state,
                        {true, 1, 0, 0, true, 0x2000, false, {}},
                        0x1200);
                if (alternate_state.alternate_counter_raw != 2
                    || alternate_state.alternate_integrator_raw != 0
                    || alternate_state.anchor_target.y != 1000
                    || alternate_state_positive.shared_angle_raw != 0x1fe
                    || !alternate_state_positive.anchor_y_adjusted) {
                    return 44;
                }

                // Camera_Update must run the scalar producer after the first
                // smoothing pass and before the alternate path's second
                // follow call. This mirrors camera +0x5ec/+0x5f0 and the
                // shared DAT_00524a94 angle without turning the latter into a
                // camera-object field.
                CameraStateRaw alternate_update;
                alternate_update.mode = 25;
                alternate_update.alternate_counter_raw = 1;
                alternate_update.alternate_phase_a_raw = 5;
                alternate_update.alternate_phase_b_raw = 3;
                alternate_update.alternate_shared_angle_raw = 0x1200;
                alternate_update.anchor_target.y = 1000;
                const CameraTargetRaw alternate_target{
                    {}, {0, -0x1000, 0}, {}, 1, 0, false};
                const CameraAlternateFollowInputRaw alternate_input{
                    true, 1, 0, 0x65, true, 0x2000, false, {}};
                update_camera(
                    alternate_update, alternate_target, {}, {}, {}, {}, {},
                    alternate_input);
                if (alternate_update.alternate_counter_raw != 2
                    || alternate_update.alternate_integrator_raw != -6
                    || alternate_update.alternate_shared_angle_raw != 0x1fe
                    || alternate_update.anchor_target.y != 994) {
                    return 54;
                }

                // The retail producer changes mode only after the current
                // normal-follow update has completed. It must not cause the
                // same call to use the mode-25 offset during preparation.
                CameraStateRaw promoted;
                promoted.mode = 1;
                const CameraTargetRaw promoted_target{
                    {}, {0, -0x1000, 0}, {}, 1, 0, false};
                const auto promoted_expected =
                    build_follow_target_transform_q12(
                        {}, promoted_target.follow_offset, 0);
                update_camera(
                    promoted, promoted_target, {}, {}, {}, {},
                    {true, 0x65, 1});
                if (promoted.mode != 25
                    || promoted.current_transform.x != promoted_expected.x
                    || promoted.current_transform.y != promoted_expected.y
                    || promoted.current_transform.z != promoted_expected.z
                    || promoted.current_transform.w != promoted_expected.w) {
                    return 39;
                }

                // The viewport control block is at the beginning of
                // Camera_Update, before mode dispatch. Verify that an
                // explicit restore/decrement operation is applied even on a
                // non-default mode path.
                CameraStateRaw mode2_viewport_control;
                mode2_viewport_control.mode = 2;
                mode2_viewport_control.viewport_parameter_raw = 7;
                mode2_viewport_control.viewport_parameter_delta_raw = -2;
                mode2_viewport_control.viewport_timer_raw = 1;
                update_camera(
                    mode2_viewport_control, {}, {}, {}, {}, {}, {}, {},
                    {true, 100, true, false, false});
                if (mode2_viewport_control.viewport_parameter_raw != 97
                    || static_cast<unsigned>(
                        mode2_viewport_control.viewport_timer_raw & 0xffff)
                        != 0) {
                    return 45;
                }

                CameraStateRaw viewport_control;
                viewport_control.viewport_parameter_raw = 7;
                viewport_control.viewport_parameter_delta_raw = -2;
                viewport_control.viewport_timer_raw = 1;
                apply_viewport_parameter_control(
                    viewport_control,
                    {true, 100, true, false, false});
                if (viewport_control.viewport_parameter_raw != 97
                    || static_cast<unsigned>(
                        viewport_control.viewport_timer_raw & 0xffff) != 0) {
                    return 37;
                }
                apply_viewport_parameter_control(
                    viewport_control,
                    {false, 0, false, false, true});
                if (viewport_control.viewport_parameter_raw != 0x100) {
                    return 38;
                }

                // The raw Camera_Update framing block uses a byte carry test,
                // not a signed directional comparison: zero selects 8 and
                // every nonzero byte selects 0x20.  It also masks Y to 12
                // bits after the paired Y operations.
                CameraStateRaw framing;
                framing.follow_rotation_raw = 100;
                framing.framing_globals_raw = {100, 0x1005, 200};
                apply_camera_framing_input_control(
                    framing,
                    {true, false, {}, true, false, 0,
                     true, false, true, false, false, true});
                if (framing.follow_rotation_raw != 90
                    || framing.framing_globals_raw.x != 92
                    || framing.framing_globals_raw.y != 0xffd
                    || framing.framing_globals_raw.z != 208) {
                    return 49;
                }
                framing.framing_globals_raw = {100, 0, 200};
                apply_camera_framing_input_control(
                    framing,
                    {true, false, {}, false, false, 1,
                     false, true, false, false, false, true});
                if (framing.framing_globals_raw.x != 132
                    || framing.framing_globals_raw.y != 0
                    || framing.framing_globals_raw.z != 232) {
                    return 50;
                }
                framing.viewport_parameter_raw = 12;
                framing.framing_globals_raw = {1, 2, 3};
                apply_camera_framing_input_control(
                    framing,
                    {true, true, {10, 20, 30}, true, true, 1,
                     true, true, true, true, true, true});
                if (framing.follow_rotation_raw != 90
                    || framing.framing_globals_raw.x != 10
                    || framing.framing_globals_raw.y != 20
                    || framing.framing_globals_raw.z != 30) {
                    return 51;
                }
                framing.framing_globals_raw = {1, 2, 3};
                apply_camera_framing_input_control(
                    framing,
                    {false, true, {10, 20, 30}, true, true, 1,
                     true, true, true, true, true, true});
                if (framing.follow_rotation_raw != 90
                    || framing.framing_globals_raw.x != 1
                    || framing.framing_globals_raw.y != 2
                    || framing.framing_globals_raw.z != 3) {
                    return 52;
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

                CameraStateRaw point_selected;
                const CameraPointSelectionInputRaw mode2_point{
                    true, 0x400, {0x10000, -0x2000, 0x30000}, 0x12345678, 5};
                const auto mode2_point_result = apply_camera_point_selection(
                    point_selected, mode2_point);
                if (!mode2_point_result.applied
                    || mode2_point_result.kind != CameraDispatchKind::mode2
                    || point_selected.mode != 2
                    || point_selected.target_valid_raw != 1
                    || point_selected.secondary_target_link_raw != 0x12345678
                    || point_selected.primary_tripod_link_raw != 0
                    || point_selected.anchor_update_flag != 0
                    || point_selected.tripod_anchor_flag != 1
                    || point_selected.anchor_target.y != -0x2000
                    // The selector's mode-2 branch does not copy the
                    // action variant; that state is only written by the
                    // 0x800/mode-1 branch.
                    || mode2_point_result.camera_action_variant_raw != 0) {
                    return 30;
                }
                CameraStateRaw normal_point;
                const CameraPointSelectionInputRaw normal_point_input{
                    true, 0xc00, {1, 2, 3}, 0x87654321, 4};
                const auto normal_point_result = apply_camera_point_selection(
                    normal_point, normal_point_input);
                if (!normal_point_result.applied
                    || normal_point_result.kind != CameraDispatchKind::normal_follow
                    || normal_point.mode != 1
                    || normal_point.primary_tripod_link_raw != 0x87654321
                    || normal_point.anchor_update_flag != 1) {
                    return 31;
                }
                CameraStateRaw action_point;
                const CameraPointSelectionInputRaw action_point_input{
                    true, 0x800, {1, 2, 3}, 0x12345678, 5};
                const auto action_point_result = apply_camera_point_selection(
                    action_point, action_point_input);
                if (!action_point_result.viewport_word6_valid
                    || action_point_result.viewport_word6_raw != 0xb72
                    || !action_point_result.framing_globals_valid
                    || action_point_result.framing_globals_raw.x != 0xc3
                    || action_point_result.framing_globals_raw.y != 0x32
                    || action_point_result.framing_globals_raw.z != -0x11
                    || !action_point_result.follow_rotation_updated
                    || action_point.follow_rotation_raw != 0x800) {
                    return 46;
                }
                CameraStateRaw viewport_point;
                const CameraPointSelectionInputRaw viewport_point_input{
                    true, 0x6a5, {1, 2, 3}, 0x12345678, 0,
                    false, 0};
                const auto viewport_point_result = apply_camera_point_selection(
                    viewport_point, viewport_point_input);
                if (!viewport_point_result.viewport_word6_valid
                    || viewport_point_result.viewport_word6_raw
                        != static_cast<unsigned short>(0xb2c - 0xa5 * 10)) {
                    return 47;
                }
                if (camera_point_distance_q4(
                        {0x10000, 0x20000, 0x30000}, {}) != 59) {
                    return 48;
                }
                const auto candidate = build_camera_point_candidate_q16(
                    {0x10000, 0x20000, 0x30000}, {0x400, -0x800, 0x120});
                if (candidate.x != 0x2b800
                    || candidate.y != -0x17000
                    || candidate.z != 0x37bc0) {
                    return 32;
                }

                CameraStateRaw death;
                death.mode = 24;
                const Q16Vec3 death_start{0x2e000, 0, 0};
                const Q16Vec3 death_target{0x10000, 0, 0};
                const auto death_first = advance_camera_death_position(
                    death, death_target, death_start, true);
                if (!death_first.initialized
                    || death_first.tick != 1
                    || death.position.x != death_start.x
                    || death.death_start_position.x != death_start.x
                    || death.death_target_position.x != death_target.x) {
                    return 24;
                }
                const auto death_second = advance_camera_death_position(
                    death, death_target, death_start, true);
                if (death_second.initialized
                    || death_second.tick != 2
                    || death.position.x != 0x2d000) {
                    return 25;
                }
                death.death_camera_tick = 30;
                const auto death_last = advance_camera_death_position(
                    death, death_target, death_start, true);
                if (death_last.completed
                    || death_last.tick != 31
                    || death.position.x != death_target.x
                    || death.mode != 24) {
                    return 26;
                }
                const auto death_done = advance_camera_death_position(
                    death, death_target, death_start, true);
                if (!death_done.completed || death.mode != 1
                    || death.position.x != death_target.x) {
                    return 27;
                }
                CameraStateRaw no_death_tripod;
                if (!advance_camera_death_position(
                        no_death_tripod, death_target, death_start, false)
                         .missing_tripod) {
                    return 28;
                }

                CameraStateRaw point;
                point.mode = 23;
                const Q16Vec3 point_start{0x82000, 0, 0};
                const Q16Vec3 point_target{0, 0, 0};
                const auto point_first = advance_camera_point_position(
                    point, point_target, point_start, false);
                if (point_first.completed
                    || point_first.tick != 1
                    || point.position.x != point_start.x
                    || point.point_start_position.x != point_start.x) {
                    return 29;
                }
                point.point_camera_tick = 10;
                const auto point_regular = advance_camera_point_position(
                    point, point_target, point_start, false);
                if (point_regular.tick != 11
                    || point.position.x != 0x78000
                    || point.point_acceleration_flag != 0) {
                    return 30;
                }
                const auto point_accelerated = advance_camera_point_position(
                    point, point_target, point_start, true);
                if (point_accelerated.tick != 17
                    || point.position.x != 0x77000
                    || point.point_acceleration_flag != 1) {
                    return 31;
                }
                point.point_camera_tick = 0x83;
                const auto point_done = advance_camera_point_position(
                    point, point_target, point_start, true);
                if (!point_done.completed || point.mode != 1) {
                    return 32;
                }

                // The mode handlers use unsigned reciprocal-multiply
                // weights, not the normal frame delta.  Check the exact
                // retail values at the endpoints and a nontrivial sample.
                if (camera_death_transform_weight_q12(1) != 136
                    || camera_death_transform_weight_q12(10) != 1365
                    || camera_death_transform_weight_q12(30) != 4096
                    || camera_point_transform_weight_q12(1) != 31
                    || camera_point_transform_weight_q12(10) != 315
                    || camera_point_transform_weight_q12(130) != 4096) {
                    return 33;
                }

                // Point/death dispatches bypass normal smoothing and finish
                // at the viewport commit boundary.
                CameraStateRaw point_dispatch;
                point_dispatch.mode = 23;
                CameraModeInputRaw point_mode_input{};
                point_mode_input.point_target_valid = true;
                point_mode_input.point_start_valid = true;
                point_mode_input.point_start_position = point_start;
                point_mode_input.point_target_position = point_target;
                point_mode_input.point_transform_valid = true;
                point_mode_input.point_transform_target = {0, 0, 0, 0x1000};
                const auto point_dispatch_result = update_camera(
                    point_dispatch, {}, {}, {}, {}, point_mode_input);
                if (point_dispatch_result.rendered_position.x != 0x82
                    || point_dispatch.point_camera_tick != 1
                    || point_dispatch.update_tick != 1
                    || point_dispatch.mode != 23) {
                    return 34;
                }

                CameraStateRaw death_dispatch;
                death_dispatch.mode = 24;
                death_dispatch.death_target_position = death_target;
                CameraModeInputRaw death_mode_input{};
                death_mode_input.tripod_present = true;
                death_mode_input.tripod_position = death_start;
                death_mode_input.death_transform_valid = true;
                death_mode_input.death_transform_source = {0, 0, 0, 0x1000};
                death_mode_input.death_transform_target = {0, 0, 0, 0x1000};
                const auto death_dispatch_result = update_camera(
                    death_dispatch, {}, {}, {}, {}, death_mode_input);
                if (death_dispatch_result.rendered_position.x != 46
                    || death_dispatch.death_camera_tick != 1
                    || death_dispatch.update_tick != 1
                    || death_dispatch.mode != 24) {
                    return 35;
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


def test_update_camera_uses_recovered_default_smoothing_stage(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is not installed")
    source = tmp_path / "camera_default_smoothing.cpp"
    source.write_text(
        textwrap.dedent(
            """
            #include "src/camera/camera_system.hpp"

            int main() {
                using namespace opentony::camera;

                CameraStateRaw camera;
                camera.mode = 1;
                camera.update_tick = 12;
                camera.current_transform = {0, 0, 0, kQ12One};
                camera.anchor_target = {0x10000, 0x20000, 0x30000};
                camera.distance_history = {
                    0x100000, 0x100000, 0x100000,
                    0x100000, 0x100000, 0x100000};
                camera.distance_q4 = 197;
                camera.follow_transition_active = 1;
                camera.effect_ramp_counter_a = 1;
                camera.effect_ramp_counter_c = 3;

                CameraTargetRaw target;
                target.tripod_state = 2;
                target.follow_offset = {0, -0x1000, 0};

                CameraUpdateHooks hooks{};
                hooks.apply_follow_transform =
                    [](CameraStateRaw&, const CameraFollowSnapshot&) {};

                CameraSmoothingProducerInputRaw producer;
                producer.valid = true;
                producer.history_sample_valid = true;
                producer.history_sample_raw = 0x2000;
                producer.distance_bias_q4 = -3;
                producer.tripod_effect_gate = true;
                producer.vertical_effect_valid = true;
                producer.vertical_effect_q16 = -140 * kQ12One;

                update_camera(
                    camera, target, {}, {}, hooks, {}, {}, {}, {}, {}, producer);

                // The recovered distance recurrence gives step -45 and
                // distance 7 for this fixture.  The common effect ramp adds
                // 800 Q16 units to the supplied -140-world-unit effect.
                if (camera.distance_step_q4 != -45
                    || camera.distance_q4 != 7
                    || camera.position.x != 0x10000
                    // 0x004e85a0 emits matrix rows 1, 2, 0, so the local
                    // camera-Z offset lands in the native Y output slot.
                    || camera.position.y != 0x19000
                    || camera.position.z != 0x30000
                    || camera.screen_effect_offset.x != -140 * kQ12One
                    || camera.screen_effect_offset.y != 0
                    || camera.shared_vertical_effect_q16
                        != -140 * kQ12One + 800) {
                    return 1;
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
            str(tmp_path / "camera_default_smoothing"),
        ],
        cwd=Path(__file__).resolve().parents[1],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run(
        [str(tmp_path / "camera_default_smoothing")],
        capture_output=True,
        text=True,
        check=False,
    )
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

                void prepare_smoothing(
                    opentony::camera::CameraStateRaw&,
                    const opentony::camera::CameraTargetRaw&,
                    opentony::camera::CameraSmoothingStageInputRaw& input) {
                    using namespace opentony::camera;
                    input.distance.distance_q4 = 7;
                    input.tripod_physics_state = 2;
                    input.effect = {
                        false, true, true, false, 2, 3,
                        1, 0, 3, 0, 100};
                    input.vertical_effect_q4 = -140;
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

                    CameraStateRaw staged;
                    staged.mode = 1;
                    staged.update_tick = 12;
                    staged.current_transform = {0, 0, 0, 0x1000};
                    staged.anchor_target = {0x10000, 0x20000, 0x30000};
                    CameraUpdateHooks staged_hooks{};
                    staged_hooks.apply_follow_transform =
                        [](CameraStateRaw&, const CameraFollowSnapshot&) {};
                    staged_hooks.prepare_smoothing_stage = prepare_smoothing;
                    update_camera(staged, {}, {}, {}, staged_hooks);
                    if (staged.distance_step_q4 != 0
                        || staged.distance_q4 != 3
                        || staged.position.x != 0x10000
                        || staged.position.y != 0x1d000
                        || staged.position.z != 0x30000
                        || staged.screen_effect_offset.x != -140 * 0x1000
                        || staged.screen_effect_offset.y != 0
                        || staged.screen_effect_offset.z != 0) {
                        return 23;
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

                const auto distance_sample =
                    camera_distance_sample_from_q16({0x1000, 0, 0});
                if (distance_sample.clamped
                    || distance_sample.quantized_length_q4 != 1
                    || distance_sample.sample_raw != 64
                    || distance_sample.bounded_offset.x != 0x1000) {
                    return 52;
                }
                const auto bounded_distance_sample =
                    camera_distance_sample_from_q16({0x100000, 0, 0});
                if (!bounded_distance_sample.clamped
                    || bounded_distance_sample.quantized_length_q4 != 256
                    || bounded_distance_sample.sample_raw != 6400
                    || bounded_distance_sample.bounded_offset.x != 0x64000
                    || bounded_distance_sample.bounded_offset.y != 0
                    || bounded_distance_sample.bounded_offset.z != 0) {
                    return 53;
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

                const auto refreshed_distance =
                    advance_camera_distance_smoothing(
                        {history, 197, -3}, 0, 0x2000);
                if (!refreshed_distance.history_refreshed
                    || refreshed_distance.history[0] != 0x2000 * 0x40
                    || refreshed_distance.history[1] != 0x100000
                    || refreshed_distance.distance_step_q4 != -58
                    || refreshed_distance.distance_q4 != -19) {
                    return 19;
                }
                const auto retained_distance =
                    advance_camera_distance_smoothing(
                        {history, 197, -3}, 2, 0x2000);
                if (retained_distance.history_refreshed
                    || retained_distance.history != history
                    || retained_distance.distance_step_q4 != -45
                    || retained_distance.distance_q4 != 7) {
                    return 20;
                }

                CameraSmoothingStageInputRaw smoothing_stage_input{};
                smoothing_stage_input.distance = {history, 197, -3};
                smoothing_stage_input.tripod_physics_state = 2;
                smoothing_stage_input.history_sample_raw = 0x2000;
                smoothing_stage_input.effect = {
                    false, true, true, false, 2, 3,
                    1, 0, 3, 0, 100};
                smoothing_stage_input.vertical_effect_q4 = -140;
                const auto smoothing_stage =
                    advance_camera_smoothing_stage(smoothing_stage_input);
                if (smoothing_stage.distance.distance_step_q4 != -45
                    || smoothing_stage.distance.distance_q4 != 7
                    || smoothing_stage.effect.distance_step_q4 != -45
                    || smoothing_stage.effect_result.special_branch
                    || smoothing_stage.effect.vertical_effect_q16 != 900
                    || smoothing_stage.base_position.local_offset.z
                        != -7 * 0x1000
                    || smoothing_stage.base_position.effect_vector.y
                        != -140 * 0x1000) {
                    return 21;
                }

                CameraStateRaw effect_camera;
                effect_camera.follow_transition_active = 1;
                effect_camera.distance_step_q4 = 3;
                effect_camera.effect_ramp_counter_a = 1;
                effect_camera.effect_ramp_counter_c = 3;
                Raw vertical_effect = 100;
                const auto effect_result = advance_camera_effects_for_camera(
                    effect_camera, false, true, 2, vertical_effect);
                if (effect_result.special_branch
                    || vertical_effect != 900
                    || effect_camera.effect_ramp_counter_b != 0
                    || effect_camera.effect_ramp_counter_d != 0) {
                    return 22;
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
                    || follow_state.follow_effect_counter_raw != 0
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

                // The level timer is a signed millisecond-to-60-Hz
                // accumulator.  Public time still advances while the
                // simulation accumulator is paused by the second guard.
                SimulationClockStateRaw clock;
                const auto clock_first = advance_simulation_clock_ms(
                    clock, 1000, false, false);
                const auto clock_second = advance_simulation_clock_ms(
                    clock, 1017, false, false);
                const auto clock_paused = advance_simulation_clock_ms(
                    clock, 1051, false, true);
                if (clock_first.advanced
                    || clock_second.tick_delta != 1
                    || clock.public_tick != 3
                    || clock.simulation_time != 1
                    || clock_paused.tick_delta != 2) {
                    return 49;
                }

                // 0x00468b30 smooths the timing delta over three samples;
                // its result is the next Camera_Update smoothing input.
                CameraTimingStateRaw timing;
                const auto timing_first = advance_camera_timing(
                    timing, 0, false, false, false, false);
                const auto timing_second = advance_camera_timing(
                    timing, 3, false, false, false, false);
                const auto timing_third = advance_camera_timing(
                    timing, 6, false, false, false, false);
                if (!timing_first.updated
                    || timing_first.sample_sum != 1
                    || timing_second.sample_sum != 3
                    || timing_third.sample_sum != 6
                    || timing.simulation_delta_q8 != 0x100
                    || timing.progress_integer != 3) {
                    return 50;
                }
                CameraTimingStateRaw scaled_timing;
                const auto scaled = advance_camera_timing(
                    scaled_timing, 6, false, true, true, false);
                if (!scaled.quarter_rate_applied
                    || !scaled.slow_rate_applied
                    || scaled_timing.simulation_delta_q8 != 80) {
                    return 51;
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
