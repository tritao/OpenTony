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
                const auto second = update_camera(camera, target, follow, {}, {});
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
