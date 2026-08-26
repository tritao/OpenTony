#include "ground_movement.hpp"

#include <cstdint>
#include <cstdlib>

using namespace opentony::ground;

namespace {

void check(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void test_x86_integer_primitives() {
    check(arithmetic_shift_right(-1, 8) == -1);
    check(arithmetic_shift_right(-257, 8) == -2);
    check(trunc_div32(-3, 2) == -1);
    check(trunc_div32(3, -2) == -1);
    check(wrap32(0x1'0000'0000LL) == 0);
    check(wrap16(0x1'0000) == 0);
}

void test_ground_position_add() {
    const Vec3i32 position{1000, -1000, 3000};
    const Vec3i32 velocity{256, -512, 1024};
    const Vec3i32 correction{512, -1024, 2048};

    // dt=1.0 in Q8, so dt2=1.0.  The recovered order is:
    // position + velocity + correction/2.
    const auto result = integrate_position_q16(position, velocity, correction, 0x100);
    const Vec3i32 expected{1512, -2024, 5048};
    check(result == expected);
}

void test_ground_turn_sign_and_cap() {
    TurnState left{};
    TurnState right{};
    const TurnConfig config{};

    update_grounded_turn(left, kLeftAction, config, 0x100);
    update_grounded_turn(right, kRightAction, config, 0x100);
    check(left.accumulator == -0x3c00);
    check(right.accumulator == 0x3c00);
    check(left.mirror == left.accumulator);
    check(right.mirror == right.accumulator);

    for (int i = 0; i < 32; ++i) {
        update_grounded_turn(left, kLeftAction, config, 0x100);
    }
    check(left.accumulator == -0x2d000);

    // Left has priority if both controller records are simultaneously held.
    TurnState both{};
    update_grounded_turn(both, kLeftAction | kRightAction, config, 0x100);
    check(both.accumulator == -0x3c00);
}

void test_q12_rotation_and_basis_mapping() {
    const auto quarter_turn = rotation_y_q12(0x400);
    check(quarter_turn.at(0, 0) == 0);
    check(quarter_turn.at(0, 2) == 0x1000);
    check(quarter_turn.at(1, 1) == 0x1000);
    check(quarter_turn.at(2, 0) == static_cast<std::int16_t>(-0x1000));
    check(quarter_turn.at(2, 2) == 0);

    const auto basis = basis_from_matrix(quarter_turn);
    const Vec3i32 expected_axis0{0x1000, 0, 0};
    const Vec3i32 expected_axis1{0, 0, -0x1000};
    const Vec3i32 expected_axis2{0, 0x1000, 0};
    check(basis.axis0 == expected_axis0);
    check(basis.axis1 == expected_axis1);
    check(basis.axis2 == expected_axis2);
}

void test_ground_projection() {
    const BasisQ12 basis{
        {0, 0, 0x1000},
        {0x1000, 0, 0},
        {0, 0x1000, 0},
    };
    Vec3i32 velocity{512, 256, 1024};
    Vec3i32 correction{0, 0, 0};

    project_ground_motion(velocity, correction, basis, 0x100, true, false);
    // Lateral (X) is removed.  The remaining forward (Z) component feeds
    // back as -((8 * forward) / 4096) at dt=1.0.
    check(velocity == Vec3i32{0, 256, 1024});
    check(correction == Vec3i32{0, 0, -2});
}

void test_ground_velocity_turn_phase() {
    Vec3i32 velocity{4096, 0, 0};
    rotate_velocity_for_ground_turn(velocity, MatrixQ12::identity(), 0x400);
    check(velocity == Vec3i32{0, 0, -4096});
}

void test_warehouse_velocity_turn_sample() {
    // The first Left frame in ground-orient11 enters 0x0049b500 with this
    // saved matrix and velocity.  This is the result of its param_3 phase,
    // before the later surface/collision tail modifies velocity again.
    MatrixQ12 saved{};
    saved.words = {-4096, 0, -6,
                    0, -4096, 0,
                    6, 0, -4096};
    Vec3i32 velocity{282, 0, 192408};
    rotate_velocity_for_ground_turn(velocity, saved, -8);
    check(velocity == Vec3i32{-2066, 0, 192364});
}

void test_warehouse_left_matrix_sample() {
    // Idle-adjacent matrix from ground-orient11, followed by the first
    // Warehouse Left sample (turn=-0x7800, dt=0x100 => angle=-8).
    MatrixQ12 prior{};
    prior.words = {-4096, 0, -6,
                    0, -4096, 0,
                    6, 0, -4096};
    const auto result = multiply_q12(prior, rotation_y_q12(-8));
    const MatrixQ12 expected{{-4096, 0, 44,
                               0, -4096, 0,
                               -45, 0, -4096}};
    check(result.words == expected.words);
}

void test_recovered_frame_order_and_commit_boundary() {
    GroundState state{};
    state.position_q16 = {1000, 2000, 3000};
    state.velocity_q16 = {256, 0, 0};

    GroundFrameInput input{};
    input.action_mask = kLeftAction;
    input.turn_config.tuning = 1; // Warehouse trace uses the 0x78 branch.
    input.prephysics_correction_q16 = {512, 0, 0};
    input.rotate_velocity = false;

    bool resolver_called = false;
    bool resolver_saw_old_position = false;
    const Vec3i32 expected_old_position{1000, 2000, 3000};
    const auto result = step_grounded(
        state,
        input,
        [&resolver_called, &resolver_saw_old_position, &expected_old_position](
            const GroundState& callback_state, const Vec3i32& candidate) {
            resolver_called = true;
            resolver_saw_old_position = callback_state.position_q16 == expected_old_position;
            return Vec3i32{candidate.x + 7, candidate.y, candidate.z - 9};
        });

    const Vec3i32 expected_old_axis0{0, 0, 0x1000};
    const Vec3i32 expected_integrated{1512, 2000, 3000};
    const Vec3i32 expected_committed{1519, 2000, 2991};
    check(result.old_basis.axis0 == expected_old_axis0);
    check(result.integrated_position_q16 == expected_integrated);
    check(result.correction_after_projection_q16 == input.prephysics_correction_q16);
    check(result.candidate_before_commit_q16 == result.integrated_position_q16);
    check(result.committed_position_q16 == expected_committed);
    check(state.position_q16 == result.committed_position_q16);
    check(state.turn.accumulator == -0x7800);
    check(result.angle_q12 == -8);
    check(resolver_called);
    check(resolver_saw_old_position);
}

} // namespace

int main() {
    test_x86_integer_primitives();
    test_ground_position_add();
    test_ground_turn_sign_and_cap();
    test_q12_rotation_and_basis_mapping();
    test_ground_projection();
    test_ground_velocity_turn_phase();
    test_warehouse_velocity_turn_sample();
    test_warehouse_left_matrix_sample();
    test_recovered_frame_order_and_commit_boundary();
}
