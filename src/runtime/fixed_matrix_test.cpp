#include "fixed_matrix.hpp"

#include <cassert>
#include <iostream>

int main() {
    using opentony::runtime::q12_apply_yaw;
    using opentony::runtime::q12_apply_ground_yaw;
    using opentony::runtime::q12_cross;
    using opentony::runtime::q12_identity_matrix;
    using opentony::runtime::q12_matrix_multiply;
    using opentony::runtime::q12_normalize;
    using opentony::runtime::q12_rotate_ground_velocity;
    using opentony::runtime::Q12Matrix3;
    using opentony::runtime::q12_transform_vector;
    using opentony::runtime::q12_yaw_matrix;

    const auto identity = q12_identity_matrix();
    assert(q12_matrix_multiply(identity, identity).values == identity.values);

    // 0x400 is a quarter turn in the retail 12-bit angle table.
    const auto quarter = q12_yaw_matrix(0x400);
    assert(quarter.at(0, 0) == 0);
    // The retail 2*pi constant is a float, so its quarter-turn sine is just
    // below one and the truncating x87 conversion produces 4095.
    assert(quarter.at(0, 2) == -4095);
    assert(quarter.at(1, 1) == 0x1000);
    assert(quarter.at(2, 0) == 4095);
    assert(quarter.at(2, 2) == 0);

    // 0xffc is sign-extended by FUN_004e80e0 and therefore means -4, not a
    // large positive rotation.
    const auto tiny = q12_yaw_matrix(0xffc);
    assert(tiny.at(0, 0) == 4095);
    assert(tiny.at(0, 2) == 25);
    assert(tiny.at(2, 0) == -25);

    const auto turned = q12_apply_yaw(identity, 0x400);
    assert(turned.values == quarter.values);
    assert(q12_transform_vector(
        quarter,
        {0, 0, 0x1000})
        == opentony::runtime::FixedPosition({-4095, 0, 0}));

    const auto basis = opentony::runtime::retail_basis_from_matrix(quarter);
    assert(basis.at_30f4 == opentony::runtime::FixedPosition({-4095, 0, 0}));
    assert(basis.at_3100 == opentony::runtime::FixedPosition({0, 0, 4095}));
    assert(basis.at_310c == opentony::runtime::FixedPosition({0, 0x1000, 0}));

    assert(q12_cross(
        {0x1000, 0, 0},
        {0, 0x1000, 0})
        == opentony::runtime::FixedPosition({0, 0, 0x1000}));
    assert(q12_cross(
        {0, 0x1000, 0},
        {0, 0, 0x1000})
        == opentony::runtime::FixedPosition({0x1000, 0, 0}));
    assert(q12_normalize({0, 0, 0})
        == opentony::runtime::FixedPosition({0x1000, 0, 0}));

    // Warehouse ground-motion-final3 captured the ordinary state-0 turn
    // writer with this saved pre-frame matrix, angle, and response vector.
    // FUN_0049b500's param_3 response phase must reproduce the rotated vector
    // before the next frame consumes it.
    Q12Matrix3 saved_ground_matrix{};
    saved_ground_matrix.at(0, 0) = -4096;
    saved_ground_matrix.at(0, 2) = -6;
    saved_ground_matrix.at(1, 1) = -4096;
    saved_ground_matrix.at(2, 0) = 6;
    saved_ground_matrix.at(2, 2) = -4096;
    assert(q12_apply_ground_yaw(saved_ground_matrix, -8).at(0, 2) == 44);
    assert(q12_apply_ground_yaw(saved_ground_matrix, -8).at(2, 0) == -45);
    const auto rotated_ground_velocity = q12_rotate_ground_velocity(
        {282, 0, 192408},
        saved_ground_matrix,
        -8);
    assert(rotated_ground_velocity
        == opentony::runtime::FixedPosition({-2066, 0, 192364}));

    std::cout << "Fixed matrix tests passed\n";
}
