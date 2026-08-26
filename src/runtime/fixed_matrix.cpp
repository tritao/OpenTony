#include "fixed_matrix.hpp"

#include <cmath>
#include <cstdint>

namespace opentony::runtime {
namespace {

constexpr double kRetailAngleToRadians =
    static_cast<double>(0.000244140625F) * 6.283185482025146484375;
constexpr double kRetailQ12Scale = 4096.0;

[[nodiscard]] std::int32_t arithmetic_shift_q12(std::int64_t value) noexcept {
    // This is the x86 SAR behavior used by FUN_004e3130. Express it without
    // relying on implementation-defined signed right shift in C++.
    if (value >= 0) {
        return static_cast<std::int32_t>(value / 4096);
    }
    return static_cast<std::int32_t>(-(((-value) + 4095) / 4096));
}

[[nodiscard]] std::int16_t angle_table_value(double value) noexcept {
    // FUN_005004f4 changes the x87 control word to round toward zero before
    // storing the trig result as an integer.
    return static_cast<std::int16_t>(static_cast<std::int32_t>(value));
}

[[nodiscard]] std::int16_t retail_signed_angle(std::int32_t angle12) noexcept {
    const std::uint16_t masked = static_cast<std::uint16_t>(angle12) & 0x0fffU;
    return static_cast<std::int16_t>(masked);
}

} // namespace

Q12Matrix3 q12_identity_matrix() noexcept {
    Q12Matrix3 result{};
    result.at(0, 0) = 0x1000;
    result.at(1, 1) = 0x1000;
    result.at(2, 2) = 0x1000;
    return result;
}

Q12Matrix3 q12_matrix_multiply(
    const Q12Matrix3& left,
    const Q12Matrix3& right) noexcept {
    Q12Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            std::int64_t sum = 0;
            for (std::size_t inner = 0; inner < 3; ++inner) {
                sum += static_cast<std::int64_t>(left.at(row, inner))
                    * right.at(inner, column);
            }
            result.at(row, column) = static_cast<std::int16_t>(
                arithmetic_shift_q12(sum));
        }
    }
    return result;
}

Q12Matrix3 q12_yaw_matrix(std::int32_t angle12) noexcept {
    const std::int16_t signed_angle = retail_signed_angle(angle12);
    const double radians = static_cast<double>(signed_angle)
        * kRetailAngleToRadians;
    const auto cosine = angle_table_value(std::cos(radians) * kRetailQ12Scale);
    const auto sine = angle_table_value(std::sin(radians) * kRetailQ12Scale);

    // This is FUN_004e7de0's transform. Its signs are important: the retail
    // path writes [cos,0,-sin; 0,1,0; sin,0,cos].
    Q12Matrix3 result{};
    result.at(0, 0) = cosine;
    result.at(0, 2) = static_cast<std::int16_t>(-sine);
    result.at(1, 1) = 0x1000;
    result.at(2, 0) = sine;
    result.at(2, 2) = cosine;
    return result;
}

Q12Matrix3 q12_apply_yaw(
    const Q12Matrix3& current,
    std::int32_t angle12) noexcept {
    // FUN_004e3130 is called with the newly-created yaw transform as the
    // left operand and the current player matrix as the right operand.
    return q12_matrix_multiply(q12_yaw_matrix(angle12), current);
}

FixedPosition q12_transform_vector(
    const Q12Matrix3& matrix,
    const FixedPosition& vector) noexcept {
    FixedPosition result{};
    for (std::size_t row = 0; row < 3; ++row) {
        const std::int64_t sum =
            static_cast<std::int64_t>(matrix.at(row, 0)) * vector[0]
            + static_cast<std::int64_t>(matrix.at(row, 1)) * vector[1]
            + static_cast<std::int64_t>(matrix.at(row, 2)) * vector[2];
        result[row] = arithmetic_shift_q12(sum);
    }
    return result;
}

RetailBasis retail_basis_from_matrix(const Q12Matrix3& matrix) noexcept {
    return RetailBasis{
        FixedPosition{
            matrix.at(0, 2),
            matrix.at(1, 2),
            matrix.at(2, 2),
        },
        FixedPosition{
            matrix.at(0, 0),
            matrix.at(1, 0),
            matrix.at(2, 0),
        },
        FixedPosition{
            matrix.at(0, 1),
            matrix.at(1, 1),
            matrix.at(2, 1),
        },
    };
}

FixedPosition q12_cross(
    const FixedPosition& left,
    const FixedPosition& right) noexcept {
    const auto sar_q12 = [](std::int64_t value) noexcept {
        if (value >= 0) {
            return static_cast<std::int32_t>(value / 0x1000);
        }
        return static_cast<std::int32_t>(-(((-value) + 0xfff) / 0x1000));
    };
    return FixedPosition{
        sar_q12(
            static_cast<std::int64_t>(left[1]) * right[2]
            - static_cast<std::int64_t>(left[2]) * right[1]),
        sar_q12(
            static_cast<std::int64_t>(left[2]) * right[0]
            - static_cast<std::int64_t>(left[0]) * right[2]),
        sar_q12(
            static_cast<std::int64_t>(left[0]) * right[1]
            - static_cast<std::int64_t>(left[1]) * right[0]),
    };
}

FixedPosition q12_normalize(const FixedPosition& vector) noexcept {
    const std::int64_t squared =
        static_cast<std::int64_t>(vector[0]) * vector[0]
        + static_cast<std::int64_t>(vector[1]) * vector[1]
        + static_cast<std::int64_t>(vector[2]) * vector[2];
    if (squared <= 0) {
        // FUN_00465f60's zero-length fallback is the positive X axis.
        return FixedPosition{0x1000, 0, 0};
    }

    std::int64_t low = 0;
    std::int64_t high = squared < 0x100000000LL
        ? 0x100000000LL
        : squared;
    while (low + 1 < high) {
        const std::int64_t middle = low + (high - low) / 2;
        if (middle <= squared / middle) {
            low = middle;
        } else {
            high = middle;
        }
    }
    const std::int64_t length = low;
    return FixedPosition{
        static_cast<std::int32_t>(
            (static_cast<std::int64_t>(vector[0]) * 0x1000) / length),
        static_cast<std::int32_t>(
            (static_cast<std::int64_t>(vector[1]) * 0x1000) / length),
        static_cast<std::int32_t>(
            (static_cast<std::int64_t>(vector[2]) * 0x1000) / length),
    };
}

} // namespace opentony::runtime
