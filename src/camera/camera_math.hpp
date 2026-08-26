#pragma once

// Evidence-backed fixed-point camera primitives recovered from THPS2 PC.
//
// This is deliberately a small reference contract, not the production engine.
// The original code uses PE32 integers, x87 truncation, and signed shifts. Keep
// these operations visible until the remaining camera modes are reconstructed.

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace opentony::camera {

using Raw = std::int32_t;

constexpr Raw kQ12One = 0x1000;
constexpr Raw kQ16One = 0x10000;
constexpr std::uint16_t kAngleMask = 0x0fff;
constexpr float kOneOver4096 = 0.000244140625f;
constexpr float kTwoPi = 6.28318548f;
constexpr double kRadiansToAngle = 651.89862876367124;

struct Q16Vec3 {
    Raw x{};
    Raw y{};
    Raw z{};
};

struct Q12Vec3 {
    Raw x{};
    Raw y{};
    Raw z{};
};

struct Q12Vec4 {
    Raw x{};
    Raw y{};
    Raw z{};
    Raw w{};
};

// The binary stores +0x14, +0x16, +0x18 as three 16-bit angle fields. The
// first two are produced by Camera_BuildLookAngles; the third is always zero
// in that helper. Their semantic pitch/yaw labels remain renderer-convention
// dependent, so the neutral names are intentional.
struct LookAngles {
    std::uint16_t first{};
    std::uint16_t second{};
    std::uint16_t third{};
};

inline Raw truncate_x87_result(long double value) {
    // 0x5004f4 changes the x87 control word to round-toward-zero and uses
    // FISTP. std::trunc expresses that conversion without accidental nearby
    // rounding. Values used by the original helpers fit in signed 32 bits.
    const long double truncated = std::trunc(value);
    if (truncated > std::numeric_limits<Raw>::max()) {
        return std::numeric_limits<Raw>::max();
    }
    if (truncated < std::numeric_limits<Raw>::min()) {
        return std::numeric_limits<Raw>::min();
    }
    return static_cast<Raw>(truncated);
}

inline Raw divide_toward_zero(Raw numerator, Raw denominator) {
    return numerator / denominator;
}

inline Raw shift_left_12(Raw value) {
    return static_cast<Raw>(static_cast<std::uint32_t>(value) << 12);
}

inline Raw atan_ratio_to_angle(Raw ratio_q12) {
    // 0x4e8580: fild ratio; multiply by 1/4096; fpatan; multiply by
    // 4096/(2*pi); x87 conversion truncates toward zero in 0x5004f4.
    const long double ratio = static_cast<long double>(ratio_q12)
        * static_cast<long double>(kOneOver4096);
    return truncate_x87_result(std::atan(ratio) * static_cast<long double>(kRadiansToAngle));
}

inline Raw sin_angle_q12(Raw angle) {
    // 0x4f39b0 masks the argument to 12 bits, converts it to radians with
    // (angle / 4096) * 2*pi, scales by 4096, then truncates via 0x5004f4.
    const Raw normalized = static_cast<Raw>(static_cast<std::uint32_t>(angle) & kAngleMask);
    return truncate_x87_result(
        std::sin(static_cast<long double>(normalized) * static_cast<long double>(kOneOver4096)
            * static_cast<long double>(kTwoPi))
        * 4096.0L);
}

inline Raw sar12_world(Raw value) {
    if (value >= 0) {
        return value / kQ12One;
    }
    return static_cast<Raw>(-((-static_cast<std::int64_t>(value) + kQ12One - 1) / kQ12One));
}

inline Raw horizontal_range_q4(const Q16Vec3& delta) {
    const Raw x = sar12_world(delta.x);
    const Raw z = sar12_world(delta.z);
    const std::int64_t square = static_cast<std::int64_t>(x) * x
        + static_cast<std::int64_t>(z) * z;
    if (square <= 0) {
        return 0;
    }
    return truncate_x87_result(std::sqrt(static_cast<long double>(square)));
}

// Reproduces 0x004c9770. Inputs are the original world-position words; the
// helper shifts their differences right by 12 before deriving angles.
inline LookAngles build_look_angles(const Q16Vec3& target, const Q16Vec3& origin) {
    const Q16Vec3 delta{
        static_cast<Raw>(target.x - origin.x),
        static_cast<Raw>(target.y - origin.y),
        static_cast<Raw>(target.z - origin.z),
    };
    const Raw x = sar12_world(delta.x);
    const Raw y = sar12_world(delta.y);
    const Raw z = sar12_world(delta.z);

    Raw second = 0;
    if (z != 0) {
        const Raw ratio = divide_toward_zero(shift_left_12(x), z);
        const Raw angle = atan_ratio_to_angle(ratio);
        second = z > 0 ? 0x800 - angle : angle;
    } else {
        second = x > 0 ? static_cast<Raw>(0xfc00) : 0x0400;
    }

    const Raw range = horizontal_range_q4(delta);
    Raw first = 0;
    if (range != 0) {
        if (y > 0) {
            first = atan_ratio_to_angle(divide_toward_zero(shift_left_12(y), range));
        } else {
            first = -atan_ratio_to_angle(divide_toward_zero(shift_left_12(-y), range));
        }
    } else {
        first = y <= 0 ? 0x0400 : static_cast<Raw>(0xfc00);
    }

    return {
        static_cast<std::uint16_t>(first & kAngleMask),
        static_cast<std::uint16_t>(second & kAngleMask),
        0,
    };
}

inline Raw sar12(std::int64_t value) {
    // x86 SAR rounds negative values toward negative infinity. C++ division
    // is toward zero, so use an explicit equivalent for the reference path.
    if (value >= 0) {
        return static_cast<Raw>(value / kQ12One);
    }
    return static_cast<Raw>(-((-value + kQ12One - 1) / kQ12One));
}

// 0x004e39a0: signed 16-bit Q12 matrix, three signed integer vector words,
// three dot products, arithmetic shift right by 12.
inline Q12Vec3 multiply_matrix_q12(
    const std::array<std::int16_t, 9>& matrix,
    const std::array<Raw, 3>& vector) {
    return {
        sar12(static_cast<std::int64_t>(matrix[0]) * vector[0]
            + static_cast<std::int64_t>(matrix[1]) * vector[1]
            + static_cast<std::int64_t>(matrix[2]) * vector[2]),
        sar12(static_cast<std::int64_t>(matrix[3]) * vector[0]
            + static_cast<std::int64_t>(matrix[4]) * vector[1]
            + static_cast<std::int64_t>(matrix[5]) * vector[2]),
        sar12(static_cast<std::int64_t>(matrix[6]) * vector[0]
            + static_cast<std::int64_t>(matrix[7]) * vector[1]
            + static_cast<std::int64_t>(matrix[8]) * vector[2]),
    };
}

} // namespace opentony::camera
