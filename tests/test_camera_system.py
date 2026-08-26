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
                const auto committed = update_camera(
                    camera, target, follow, {}, {});
                if (camera.mirrored_anchor.x != 0x10000
                    || camera.anchor_target.x != 0x20000
                    || camera.update_tick != 1
                    || camera.current_transform.w != 0x1000
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
