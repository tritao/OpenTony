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
                if (sizeof(RasterVertexRecordRaw) != 4) {
                    return 30;
                }
                if (sizeof(TransformedVertexWorkingRecordRaw) != 28) {
                    return 31;
                }
                CommonVertexTransformProducerInputRaw producer_input;
                producer_input.object_basis_q12 = {
                    0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000,
                };
                producer_input.view_basis_q12 = {
                    0x1000, 0x0200, 0, 0, 0x1000, 0x0400, 0, 0, 0x1000,
                };
                producer_input.relative_translation = {10, -20, 30};
                producer_input.state_flags = 0x40;
                const auto produced_transform = build_common_vertex_transform(
                    producer_input);
                if (std::fabs(
                        f32_from_bits(produced_transform.transform.bias_bits[0])
                        - 7.5f) > 0.0001f
                    || std::fabs(
                        f32_from_bits(produced_transform.transform.bias_bits[1])
                        - (-12.5f)) > 0.0001f
                    || std::fabs(
                        f32_from_bits(produced_transform.transform.bias_bits[2])
                        - 30.0f) > 0.0001f
                    || std::fabs(
                        f32_from_bits(produced_transform.perspective_factor_bits[0])
                        - (-0.125f)) > 0.0001f
                    || std::fabs(
                        f32_from_bits(produced_transform.perspective_factor_bits[1])
                        - (-1.0f)) > 0.0001f
                    || std::fabs(
                        f32_from_bits(produced_transform.perspective_factor_bits[2])
                        - 0.0f) > 0.0001f) {
                    return 34;
                }
                CommonVertexTransformRaw common_transform;
                common_transform.linear_bits = {
                    f32_to_bits(1.0f), f32_to_bits(0.000244140625f),
                    f32_to_bits(-0.001708984375f),
                    f32_to_bits(-0.000732421875f), f32_to_bits(0.98388671875f),
                    f32_to_bits(-0.1796875f),
                    f32_to_bits(0.001220703125f), f32_to_bits(0.1796875f),
                    f32_to_bits(0.98388671875f),
                };
                common_transform.bias_bits = {
                    f32_to_bits(-641.56201171875f),
                    f32_to_bits(-1580.8857421875f),
                    f32_to_bits(2868.30078125f),
                };
                common_transform.center_x_bits = f32_to_bits(320.0f);
                common_transform.center_y_bits = f32_to_bits(240.0f);
                common_transform.depth_scale_bits = f32_to_bits(384.0f);
                const CommonVertexViewportEdgesRaw common_viewport{0, 640, 0, 480};
                const auto common_projection = project_common_vertex(
                    {0, 1328, 92, 0}, common_transform, common_viewport, 0x800);
                if (std::fabs(
                        f32_from_bits(common_projection.record.words[0])
                        - 242.971054f) > 0.001f
                    || std::fabs(
                        f32_from_bits(common_projection.record.words[1])
                        - 205.074249f) > 0.001f
                    || std::fabs(
                        f32_from_bits(common_projection.record.words[2])
                        - 3197.443359f) > 0.01f
                    || std::fabs(
                        f32_from_bits(common_projection.record.words[3])
                        - 0.120095953f) > 0.000001f
                    || common_projection.record.words[5] != 0) {
                    return 32;
                }
                const auto common_near_clip = project_common_vertex(
                    {0, 0, -3000, 0}, common_transform, common_viewport, 0x800);
                if ((common_near_clip.record.words[5] & 0x10U) == 0) {
                    return 33;
                }
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
                const auto camera_tail_transform = camera_transform_matrix_q12_trunc(
                    {1, 2, 3, 4, 5, 6, 7, 8, 9},
                    {0x1000, 0x2000, 0x3000});
                if (camera_tail_transform.x != 32
                    || camera_tail_transform.y != 50
                    || camera_tail_transform.z != 14) {
                    return 21;
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
                const auto saturated_cross = cross_product_s16(
                    {0x7fff, 0, 0}, {0, 0x7fff, 0});
                const auto negative_cross = cross_product_s16(
                    {-0x7fff, 0, 0}, {0, 0x7fff, 0});
                if (saturated_cross.z != 0x7fff || negative_cross.z != -0x8000) {
                    return 20;
                }
                if (sin_angle_q12(0x400) != 0x0fff) {
                    return 6;
                }
                if (dot_q12_x87({0, 0x1000, 0}, {0, 0x1000, 0}) != 0x1000
                    || dot_q12_x87({0x0800, 0, 0}, {0x1000, 0, 0}) != 0x0800) {
                    return 16;
                }
                const TransformQ12 identity{0, 0, 0, 0x1000};
                const TransformQ12 sample{0x0200, -0x0300, 0x0400, 0x0e00};
                const auto left_identity = multiply_transform_q12(sample, identity);
                const auto right_identity = multiply_transform_q12(identity, sample);
                if (left_identity.x != sample.x || left_identity.y != sample.y
                    || left_identity.z != sample.z || left_identity.w != sample.w
                    || right_identity.x != sample.x || right_identity.y != sample.y
                    || right_identity.z != sample.z || right_identity.w != sample.w) {
                    return 10;
                }
                const auto normalized = normalize_transform_q12({0, 0, 0, 0x0800});
                if (normalized.x != 0 || normalized.y != 0
                    || normalized.z != 0 || normalized.w != 0x1000) {
                    return 14;
                }
                const auto x_rotation = rotation_x_q12(0x800);
                const auto slerp_start = slerp_transform_q12(identity, x_rotation, 0);
                const auto slerp_end = slerp_transform_q12(identity, x_rotation, 0x1000);
                const auto normalized_rotation = normalize_transform_q12(x_rotation);
                if (slerp_start.x != identity.x || slerp_start.y != identity.y
                    || slerp_start.z != identity.z || slerp_start.w != identity.w
                    || slerp_end.x != normalized_rotation.x || slerp_end.y != normalized_rotation.y
                    || slerp_end.z != normalized_rotation.z || slerp_end.w != normalized_rotation.w) {
                    return 15;
                }
                if (x_rotation.x != 0x0fff || x_rotation.y != 0
                    || x_rotation.z != 0 || x_rotation.w != 0) {
                    return 11;
                }
                const auto identity_matrix = transform_to_matrix_q12(identity);
                if (identity_matrix != MatrixQ12{0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000}) {
                    return 12;
                }
                const auto matrix_identity = matrix_to_transform_q12(identity_matrix);
                if (matrix_identity.x != identity.x || matrix_identity.y != identity.y
                    || matrix_identity.z != identity.z || matrix_identity.w != identity.w) {
                    return 18;
                }
                const auto half_turn_matrix = transform_to_matrix_q12(x_rotation);
                if (half_turn_matrix[0] != 0x1000 || half_turn_matrix[4] >= 0
                    || half_turn_matrix[8] >= 0) {
                    return 13;
                }
                const auto recovered_half_turn = matrix_to_transform_q12(half_turn_matrix);
                if (recovered_half_turn.x != 0x0ffe
                    || recovered_half_turn.y != x_rotation.y
                    || recovered_half_turn.z != x_rotation.z
                    || recovered_half_turn.w != x_rotation.w) {
                    return 19;
                }
                const MatrixQ12 ordered_matrix{1, 2, 3, 4, 5, 6, 7, 8, 9};
                if (transpose_matrix_q12(ordered_matrix) != MatrixQ12{1, 4, 7, 2, 5, 8, 3, 6, 9}) {
                    return 17;
                }
                const auto view_record_matrix = camera_view_record_matrix_q12(x_rotation);
                if (view_record_matrix != transpose_matrix_q12(half_turn_matrix)) {
                    return 24;
                }
                ViewportProjectionRaw projection;
                const ViewportInputRaw viewport{{640, 480, 0, 0, 0x10, 0, 0x100, 0, 0, 0,
                                                  0x111, 0x222, 0x333, 0x444}};
                if (!build_viewport_projection(viewport, 0x33, 0x1000, 0x1000, projection)) {
                    return 7;
                }
                if (projection.viewport.words[5] != 0x33
                    || projection.viewport.words[7] != 0x1400
                    || projection.viewport.words[8] != 320
                    || projection.viewport.words[9] != 240
                    || projection.viewport.words[10] != 0x111
                    || projection.viewport.words[13] != 0x444) {
                    return 8;
                }
                if (projection.basis.blocks[0][2] != -0x1000
                    || projection.basis.blocks[0][3] != 0x33
                    || projection.basis.blocks[0][7] != -0x10) {
                    return 9;
                }
                ViewportProjectionRaw odd_projection;
                const ViewportInputRaw odd_viewport{{641, 480, 0, 0, 0x10, 0, 0x100, 0, 0, 0}};
                if (!build_viewport_projection(odd_viewport, 0x33, 1, 0x1000, odd_projection)) {
                    return 22;
                }
                // Retail uses extent * 0x800 for the edge numerator, while
                // the divisor uses (extent & 0x1fffff) >> 1. For odd 641,
                // that preserves the one-half-unit difference.
                if (odd_projection.basis.blocks[1][2] != 4102) {
                    return 23;
                }
                ProjectionBasisQ12 handoff_basis;
                for (std::size_t block = 0; block < 5; ++block) {
                    for (std::size_t word = 0; word < 8; ++word) {
                        handoff_basis.blocks[block][word] =
                            static_cast<std::int16_t>(block * 8 + word + 1);
                    }
                }
                const MatrixQ12 identity_view{
                    0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000};
                const auto handoff = prepare_view_records_q12(
                    identity_view, handoff_basis);
                if (handoff.record_005620e8
                        != std::array<std::int16_t, 15>{
                            1, 2, 3, 5, 6, 7, 9, 10, 11,
                            13, 14, 15, 17, 18, 19}
                    || handoff.record_005620c0
                        != std::array<std::int16_t, 15>{
                            13, 14, 15, 17, 18, 19, 21, 22, 23,
                            25, 26, 27, 29, 30, 31}) {
                    return 25;
                }
                CameraRenderPreparationInputRaw render_input;
                render_input.camera_transform = identity;
                render_input.viewport = viewport;
                render_input.state_selector = 0x33;
                render_input.scale_x = 0x1000;
                render_input.scale_y = 0x1000;
                    CameraRenderPreparationRaw render_state;
                    const auto expected_render_records = prepare_view_records_q12(
                        identity_matrix, projection.basis);
                    if (!prepare_camera_render_state_q12(render_input, render_state)
                    || render_state.row_ordered_matrix != identity_matrix
                    || render_state.backend_view_matrix != identity_matrix
                    || render_state.viewport_projection.viewport.words
                           != projection.viewport.words
                        || render_state.prepared_records.record_005620e8
                               != expected_render_records.record_005620e8
                        || render_state.prepared_records.record_005620c0
                               != expected_render_records.record_005620c0) {
                    return 30;
                }
                render_input.scale_y = 0;
                if (prepare_camera_render_state_q12(render_input, render_state)) {
                    return 31;
                }
                NormalizedViewportRecordRaw display_record;
                const DisplayViewportNormalizationInputRaw display_config{
                    true, 640, 480, 0, 0};
                if (!normalize_viewport_record(
                        viewport, display_config, display_record)) {
                    return 26;
                }
                if (display_record.header != 0xe3000000U
                    || display_record.origin_x != 0
                    || display_record.origin_y != 0
                    || display_record.extent_x != 0x200
                    || display_record.extent_y != 0xf0) {
                    return 27;
                }
                NormalizedViewportRecordRaw default_display_record;
                const DisplayViewportNormalizationInputRaw default_display_config{
                    false, 640, 480, 640, 480};
                if (!normalize_viewport_record(
                        viewport, default_display_config, default_display_record)
                    || default_display_record.origin_x != 0
                    || default_display_record.origin_y != 0
                    || default_display_record.extent_x != 0x200
                    || default_display_record.extent_y != 0xf0) {
                    return 28;
                }
                const ViewportInputRaw tiny_viewport{{1, 1, 0, 0, 0, 0, 1, 0, 0, 0,
                                                       0, 0, 0, 0}};
                NormalizedViewportRecordRaw clamped_display_record;
                if (!normalize_viewport_record(
                        tiny_viewport, display_config, clamped_display_record)
                    || clamped_display_record.extent_x != 1
                    || clamped_display_record.extent_y != 1) {
                    return 29;
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
