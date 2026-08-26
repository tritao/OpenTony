#pragma once

#include "position_commit.hpp"

#include <array>
#include <cstdint>

namespace opentony::runtime {

// The retail skater orientation is nine signed shorts in Q12. The matrix is
// logically row-major, although the original stores/loads its rows through
// interleaved short offsets.
struct Q12Matrix3 final {
    std::array<std::int16_t, 9> values{};

    friend bool operator==(
        const Q12Matrix3&,
        const Q12Matrix3&) = default;

    [[nodiscard]] std::int16_t& at(std::size_t row, std::size_t column) noexcept {
        return values[row * 3 + column];
    }
    [[nodiscard]] const std::int16_t& at(
        std::size_t row,
        std::size_t column) const noexcept {
        return values[row * 3 + column];
    }
};

// These names deliberately retain the retail field offsets. FUN_0049c7d0
// copies matrix columns into +30f4, +3100, and +310c; their gameplay meaning
// is left open until a movement producer consumes them.
struct RetailBasis final {
    FixedPosition at_30f4{};
    FixedPosition at_3100{};
    FixedPosition at_310c{};

    friend bool operator==(
        const RetailBasis&,
        const RetailBasis&) = default;
};

[[nodiscard]] Q12Matrix3 q12_identity_matrix() noexcept;

// Matches FUN_004e3130: matrix products use signed products followed by an
// arithmetic right shift of twelve bits, not truncation toward zero.
[[nodiscard]] Q12Matrix3 q12_matrix_multiply(
    const Q12Matrix3& left,
    const Q12Matrix3& right) noexcept;

// FUN_004e80e0 composes Y, X, then Z rotations. The grounded steering path
// calls it with only the Y word populated, so this public boundary exposes the
// exact one-axis operation first.
[[nodiscard]] Q12Matrix3 q12_yaw_matrix(std::int32_t angle12) noexcept;

// FUN_004c4d10 rebuilds the retail restart basis from the high word of the
// restart auxiliary field.  The restart matrix is a yawed, negative identity
// basis: forward/right/down are stored as the three matrix columns.
[[nodiscard]] std::int32_t retail_restart_angle12(
    std::uint32_t auxiliary) noexcept;

[[nodiscard]] Q12Matrix3 q12_restart_matrix(
    std::uint32_t auxiliary) noexcept;

[[nodiscard]] Q12Matrix3 q12_apply_yaw(
    const Q12Matrix3& current,
    std::int32_t angle12) noexcept;

// Grounded FUN_0049b500 uses the saved player matrix as the left operand and
// the Y rotation as the right operand. Keep this convention separate from
// q12_apply_yaw(), whose older generic callers use the opposite composition.
[[nodiscard]] Q12Matrix3 q12_ground_yaw_matrix(
    std::int32_t angle12) noexcept;

[[nodiscard]] Q12Matrix3 q12_apply_ground_yaw(
    const Q12Matrix3& current,
    std::int32_t angle12) noexcept;

// The param_3 != 0 phase of grounded FUN_0049b500. It transforms the live
// response vector through the saved pre-frame matrix and restores its integer
// speed metric after the Q12 matrix chain.
[[nodiscard]] FixedPosition q12_rotate_ground_velocity(
    const FixedPosition& velocity,
    const Q12Matrix3& saved_old_matrix,
    std::int32_t angle12,
    std::int16_t offset12 = 0) noexcept;

// Matches retail FUN_004e85a0: a row-major 3x3 signed-short matrix multiplied
// by a three-component fixed-point vector through x87, with truncation toward
// zero after the Q12 scale.
[[nodiscard]] FixedPosition q12_transform_vector(
    const Q12Matrix3& matrix,
    const FixedPosition& vector) noexcept;

[[nodiscard]] RetailBasis retail_basis_from_matrix(
    const Q12Matrix3& matrix) noexcept;

// FUN_004e2ff0's three-component Q12 cross product and FUN_00465f60's
// normalized-vector boundary. These are shared by the grounded orientation
// code and the in-air basis handoff.
[[nodiscard]] FixedPosition q12_cross(
    const FixedPosition& left,
    const FixedPosition& right) noexcept;

[[nodiscard]] FixedPosition q12_normalize(
    const FixedPosition& vector) noexcept;

} // namespace opentony::runtime
