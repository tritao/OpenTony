import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest


def test_camera_math_reference_compiles_and_preserves_fixed_contract(tmp_path):
    compiler = shutil.which("g++")
    if compiler is None:
        pytest.skip("g++ is not installed")
    source = tmp_path / "camera_math_smoke.cpp"
    source.write_text(
        textwrap.dedent(
            """
            #include <array>
            #include "src/camera/camera_math.hpp"

            int main() {
                using namespace opentony::camera;
                const auto angles = build_look_angles({0, 0, 0x10000}, {0, 0, 0});
                if (angles.first != 0 || angles.second != 0x800 || angles.third != 0) {
                    return 1;
                }
                const auto transformed = multiply_matrix_q12(
                    {0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000},
                    {0x1000, -0x1000, 0x2000});
                if (transformed.x != 0x1000 || transformed.y != -0x1000 || transformed.z != 0x2000) {
                    return 2;
                }
                const auto sar_fraction = multiply_matrix_q12(
                    {0x0800, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000},
                    {-1, 0, 0});
                const auto x87_fraction = transform_matrix_q12_trunc(
                    {0x0800, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000},
                    {-1, 0, 0});
                if (sar_fraction.x != -1 || x87_fraction.x != 0) {
                    return 3;
                }
                if (angle_delta12(0x0000, 0x0fff) != -1
                    || angle_delta12(0x0fff, 0x0000) != 1
                    || angle_delta12(0x0000, 0x0800) != 0x0800) {
                    return 4;
                }
                if (decay_shake_axis(-3, 2) != -1
                    || decay_shake_axis(3, 2) != 1
                    || zero_shake_on_sign_crossing(-1, 1) != 0
                    || zero_shake_on_sign_crossing(1, 2) != 2) {
                    return 5;
                }
                if (sin_angle_q12(0x400) != 0x0fff) {
                    return 6;
                }
                ViewportProjectionRaw projection;
                const ViewportInputRaw viewport{{640, 480, 0, 0, 0x10, 0, 0x100, 0, 0, 0}};
                if (!build_viewport_projection(viewport, 0x33, 0x1000, 0x1000, projection)) {
                    return 7;
                }
                if (projection.viewport.words[5] != 0x33
                    || projection.viewport.words[7] != 0x1400
                    || projection.viewport.words[8] != 320
                    || projection.viewport.words[9] != 240) {
                    return 8;
                }
                if (projection.basis.blocks[0][2] != -0x1000
                    || projection.basis.blocks[0][3] != 0x33
                    || projection.basis.blocks[0][7] != -0x10) {
                    return 9;
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
            str(tmp_path / "camera_math_smoke"),
        ],
        cwd=Path(__file__).resolve().parents[1],
        capture_output=True,
        text=True,
        check=False,
    )
    assert result.returncode == 0, result.stderr
    run = subprocess.run([str(tmp_path / "camera_math_smoke")], capture_output=True, text=True, check=False)
    assert run.returncode == 0, run.stderr
