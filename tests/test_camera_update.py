import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest


def test_camera_update_model_preserves_dispatch_and_state_boundaries(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is not installed")

    source = tmp_path / "camera_update_smoke.cpp"
    source.write_text(
        textwrap.dedent(
            """
            #include "src/camera/camera_update.hpp"

            using namespace opentony::camera;

            bool same(Q16Vec3 left, Q16Vec3 right) {
                return left.x == right.x && left.y == right.y && left.z == right.z;
            }

            int main() {
                if (dispatch_camera_mode(1).entry != CameraUpdateEntry::entry_0040ff2b) return 1;
                if (dispatch_camera_mode(2).callee != 0x004113f0) return 2;
                if (dispatch_camera_mode(3).entry != CameraUpdateEntry::entry_00410027) return 3;
                if (dispatch_camera_mode(22).entry != CameraUpdateEntry::entry_00410027) return 4;
                if (dispatch_camera_mode(23).callee != 0x00410f70) return 5;
                if (dispatch_camera_mode(24).callee != 0x00410c90) return 6;
                if (dispatch_camera_mode(25).entry != CameraUpdateEntry::entry_0040feef) return 7;
                if (dispatch_camera_mode(0).bounds_check_passed) return 8;
                if (dispatch_camera_mode(26).bounds_check_passed) return 9;

                CameraUpdateState camera{};
                camera.anchor_update_flag = true;
                camera.tripod_anchor_flag = true;
                camera.secondary_target_valid = true;
                camera.anchor_target = {1, 2, 3};
                camera.look_target = {4, 5, 6};
                copy_observed_tripod_targets(camera, {10, 20, 30}, true, {40, 50, 60}, true);
                if (!same(camera.mirrored_anchor, {1, 2, 3})) return 10;
                if (!same(camera.anchor_target, {10, 20, 30})) return 11;
                if (!same(camera.secondary_anchor, {4, 5, 6})) return 12;
                if (!same(camera.look_target, {40, 50, 60})) return 13;

                camera.viewport_parameter_raw = 0x12340056;
                if (viewport_parameter_low_word(camera) != 0x0056) return 14;
                camera.update_tick = 239;
                advance_observed_update_tick(camera);
                if (camera.update_tick != 240) return 15;
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
            str(tmp_path / "camera_update_smoke"),
        ],
        cwd=Path(__file__).resolve().parents[1],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run([str(tmp_path / "camera_update_smoke")], capture_output=True, text=True, check=False)
    assert run.returncode == 0, run.stderr
