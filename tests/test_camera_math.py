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
                if (sin_angle_q12(0x400) != 0x0fff) {
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
