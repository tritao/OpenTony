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

inline Raw wrap_s32(std::int64_t value) {
    return static_cast<Raw>(static_cast<std::uint32_t>(value));
}

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

// The camera embeds these four words behind a small object vtable at +0x444.
// The three constructors at 0x004a9820/0x004a9870/0x004a98c0 generate
// half-angle sine/cosine records, and 0x004a9650 composes two records.  Keep
// the name neutral until the output-matrix convention is independently
// validated, but model the payload as a rotation transform rather than as a
// fifth matrix column.
using TransformQ12 = Q12Vec4;
using MatrixQ12 = std::array<std::int16_t, 9>;

// Render_SetViewProjection receives a ten-short viewport record. The original
// routine mutates fields 5, 7, 8, and 9 before building its Q12 basis. Keep
// these as raw shorts: the runtime globals that feed the record are not yet
// portable display/FOV concepts.
struct ViewportInputRaw {
    std::array<std::uint16_t, 10> words{};
};

struct ProjectionBasisQ12 {
    // Five contiguous eight-short blocks beginning at 0x00563a90. The first
    // three blocks provide the six 3-short vectors consumed by the two calls
    // to Fixed_MatrixMultiplyQ12; retaining all five preserves the static
    // projection state without assigning it a conventional matrix name.
    std::array<std::array<std::int16_t, 8>, 5> blocks{};
};

struct ViewportProjectionRaw {
    ViewportInputRaw viewport{};
    ProjectionBasisQ12 basis{};
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

inline Raw acos_ratio_to_angle(Raw ratio_q12) {
    // 0x004ca0a0 -> 0x00500db0: fild(dot), multiply by 1/4096, compute
    // acos through the x87 atan identity, multiply by 4096/(2*pi), and
    // truncate toward zero.
    const long double ratio = static_cast<long double>(ratio_q12)
        * static_cast<long double>(kOneOver4096);
    return truncate_x87_result(std::acos(ratio) * static_cast<long double>(kRadiansToAngle));
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

inline Raw cos_angle_q12(Raw angle) {
    const Raw normalized = static_cast<Raw>(static_cast<std::uint32_t>(angle) & kAngleMask);
    return truncate_x87_result(
        std::cos(static_cast<long double>(normalized) * static_cast<long double>(kOneOver4096)
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
        wrap_s32(static_cast<std::int64_t>(target.x) - origin.x),
        wrap_s32(static_cast<std::int64_t>(target.y) - origin.y),
        wrap_s32(static_cast<std::int64_t>(target.z) - origin.z),
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

inline Raw multiply_s32(Raw left, Raw right) {
    // The original uses 32-bit IMUL.  Do not let a host compiler promote the
    // product to a wider value and thereby change overflow-sensitive camera
    // behavior.
    return wrap_s32(static_cast<std::int64_t>(left) * right);
}

inline Raw add_s32(Raw left, Raw right) {
    return wrap_s32(static_cast<std::int64_t>(left) + right);
}

inline Raw subtract_s32(Raw left, Raw right) {
    return wrap_s32(static_cast<std::int64_t>(left) - right);
}

inline std::int32_t arithmetic_shift_right_one(std::int32_t value);

inline Raw arithmetic_shift_right(Raw value, unsigned bits) {
    if (bits == 0) {
        return value;
    }
    if (value >= 0) {
        return static_cast<Raw>(static_cast<std::uint32_t>(value) >> bits);
    }
    const auto magnitude = -static_cast<std::int64_t>(value);
    return wrap_s32(-((magnitude + (static_cast<std::int64_t>(1) << bits) - 1) >> bits));
}

inline std::int16_t low_s16_raw(Raw value) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

inline std::int16_t q11_product(Raw left, Raw right) {
    return low_s16_raw(arithmetic_shift_right(multiply_s32(left, right), 11));
}

inline std::uint32_t isqrt_u32_x87(std::uint32_t value);

// Exact 0x004a9650 payload composition.  The binary is called with the
// destination first, the second operand second, and the first operand third;
// this value-oriented form returns the destination result without exposing
// that calling-convention detail.  All products/adds wrap as PE32 signed
// operations before the final arithmetic SAR12.
inline TransformQ12 multiply_transform_q12(
    const TransformQ12& first,
    const TransformQ12& second) {
    const auto product = [](Raw a, Raw b) { return multiply_s32(a, b); };
    const auto sum3 = [&](Raw a, Raw b, Raw c) {
        return add_s32(add_s32(a, b), c);
    };
    const auto difference = [&](Raw a, Raw b, Raw c) {
        return subtract_s32(subtract_s32(a, b), c);
    };

    // Assembly operand order, with `first` corresponding to the third stack
    // argument and `second` to the second stack argument:
    //   x = sx*fw + sw*fx + sz*fy - sy*fz
    //   y = sx*fz + sy*fw + sw*fy - sz*fx
    //   z = sz*fw + sy*fx + sw*fz - sx*fy
    //   w = sw*fw - sy*fy - sx*fx - sz*fz
    const Raw x = sum3(product(second.x, first.w),
                       product(second.w, first.x),
                       subtract_s32(product(second.z, first.y),
                                     product(second.y, first.z)));
    const Raw y = sum3(product(second.x, first.z),
                       product(second.y, first.w),
                       subtract_s32(product(second.w, first.y),
                                     product(second.z, first.x)));
    const Raw z = sum3(product(second.z, first.w),
                       product(second.y, first.x),
                       subtract_s32(product(second.w, first.z),
                                     product(second.x, first.y)));
    const Raw w = difference(product(second.w, first.w),
                             product(second.y, first.y),
                             sum3(product(second.x, first.x),
                                  product(second.z, first.z), 0));
    return {sar12(x), sar12(y), sar12(z), sar12(w)};
}

inline TransformQ12 normalize_transform_q12(const TransformQ12& value) {
    const Raw sum = add_s32(
        add_s32(multiply_s32(value.x, value.x), multiply_s32(value.y, value.y)),
        add_s32(multiply_s32(value.z, value.z), multiply_s32(value.w, value.w)));
    Raw length = static_cast<Raw>(isqrt_u32_x87(static_cast<std::uint32_t>(sum)));
    if (length == 0) {
        length = 1;
    }
    return {
        wrap_s32((static_cast<std::int64_t>(value.x) << 12) / length),
        wrap_s32((static_cast<std::int64_t>(value.y) << 12) / length),
        wrap_s32((static_cast<std::int64_t>(value.z) << 12) / length),
        wrap_s32((static_cast<std::int64_t>(value.w) << 12) / length),
    };
}

inline TransformQ12 slerp_transform_q12(
    const TransformQ12& first,
    const TransformQ12& second,
    Raw weight_q12) {
    TransformQ12 normalized_first = normalize_transform_q12(first);
    const TransformQ12 normalized_second = normalize_transform_q12(second);
    const Raw dot_sum = add_s32(
        add_s32(multiply_s32(normalized_first.x, normalized_second.x),
                multiply_s32(normalized_first.y, normalized_second.y)),
        add_s32(multiply_s32(normalized_first.z, normalized_second.z),
                multiply_s32(normalized_first.w, normalized_second.w)));
    Raw dot = sar12(dot_sum);
    if (dot < 0) {
        dot = wrap_s32(-static_cast<std::int64_t>(dot));
        normalized_first = {
            wrap_s32(-static_cast<std::int64_t>(normalized_first.x)),
            wrap_s32(-static_cast<std::int64_t>(normalized_first.y)),
            wrap_s32(-static_cast<std::int64_t>(normalized_first.z)),
            wrap_s32(-static_cast<std::int64_t>(normalized_first.w)),
        };
    }
    if (dot > kQ12One) {
        dot = kQ12One;
    }

    Raw first_weight = kQ12One - weight_q12;
    Raw second_weight = weight_q12;
    if (kQ12One - dot > 0x80) {
        const Raw angle = acos_ratio_to_angle(dot);
        const Raw sin_angle = sin_angle_q12(angle);
        if (sin_angle != 0) {
            first_weight = wrap_s32(
                (static_cast<std::int64_t>(sin_angle_q12(
                    sar12(multiply_s32(kQ12One - weight_q12, angle)))) << 12)
                / sin_angle);
            second_weight = wrap_s32(
                (static_cast<std::int64_t>(sin_angle_q12(
                    sar12(multiply_s32(weight_q12, angle)))) << 12)
                / sin_angle);
        }
    }

    const auto blend = [&](Raw first_value, Raw second_value) {
        return sar12(add_s32(multiply_s32(first_weight, first_value),
                             multiply_s32(second_weight, second_value)));
    };
    return normalize_transform_q12({
        blend(normalized_first.x, normalized_second.x),
        blend(normalized_first.y, normalized_second.y),
        blend(normalized_first.z, normalized_second.z),
        blend(normalized_first.w, normalized_second.w),
    });
}

// Exact 0x004a9910 conversion. The four payload words are consumed as
// (x,y,z,w) at +4,+8,+c,+10. Products use 32-bit IMUL, each product is
// arithmetically shifted by 11 (the equivalent of multiplying by two in Q12),
// and each result is narrowed to a signed short at the matrix boundary.
inline MatrixQ12 transform_to_matrix_q12(const TransformQ12& transform) {
    const Raw x = transform.x;
    const Raw y = transform.y;
    const Raw z = transform.z;
    const Raw w = transform.w;

    const auto q11 = [](Raw value) { return q11_product(value, value); };
    const auto add = [](std::int16_t left, std::int16_t right) {
        return low_s16_raw(add_s32(left, right));
    };
    const auto subtract = [](std::int16_t left, std::int16_t right) {
        return low_s16_raw(subtract_s32(left, right));
    };
    const std::int16_t zz = q11(z);
    const std::int16_t yy = q11(y);
    const std::int16_t zw = q11_product(z, w);
    const std::int16_t yx = q11_product(y, x);
    const std::int16_t yw = q11_product(y, w);
    const std::int16_t zx = q11_product(z, x);
    const std::int16_t xx = q11(x);
    const std::int16_t xw = q11_product(x, w);
    const std::int16_t zy = q11_product(z, y);

    const std::int16_t one_minus_zz_yy = low_s16_raw(
        subtract_s32(subtract_s32(kQ12One, zz), yy));
    const std::int16_t one_minus_xx = low_s16_raw(subtract_s32(kQ12One, xx));

    return {
        one_minus_zz_yy,
        add(zw, yx),
        subtract(zx, yw),
        subtract(yx, zw),
        subtract(one_minus_xx, zz),
        add(xw, zy),
        add(yw, zx),
        subtract(zy, xw),
        subtract(one_minus_xx, yy),
    };
}

inline TransformQ12 rotation_x_q12(std::int16_t angle) {
    const Raw half = arithmetic_shift_right_one(static_cast<Raw>(angle));
    return {sin_angle_q12(half), 0, 0, cos_angle_q12(half)};
}

inline TransformQ12 rotation_y_q12(std::int16_t angle) {
    const Raw half = arithmetic_shift_right_one(static_cast<Raw>(angle));
    return {0, sin_angle_q12(half), 0, cos_angle_q12(half)};
}

inline TransformQ12 rotation_z_q12(std::int16_t angle) {
    const Raw half = arithmetic_shift_right_one(static_cast<Raw>(angle));
    return {0, 0, sin_angle_q12(half), cos_angle_q12(half)};
}

// 0x004e39a0: signed 16-bit Q12 matrix, three signed integer vector words,
// three dot products, arithmetic shift right by 12.
inline Q12Vec3 multiply_matrix_q12(
    const std::array<std::int16_t, 9>& matrix,
    const std::array<Raw, 3>& vector) {
    const auto dot = [&](std::size_t row) {
        return add_s32(
            add_s32(multiply_s32(matrix[row * 3], vector[0]),
                    multiply_s32(matrix[row * 3 + 1], vector[1])),
            multiply_s32(matrix[row * 3 + 2], vector[2]));
    };
    return {
        sar12(dot(0)),
        sar12(dot(1)),
        sar12(dot(2)),
    };
}

// 0x004e85a0: the related camera/effect conversion path. The binary performs
// each dot product through x87, multiplies by 1/4096, and converts with the
// shared round-toward-zero helper. This intentionally differs from
// multiply_matrix_q12 for negative fractional results: SAR(-2048, 12) is -1,
// while x87 truncation of -0.5 is 0.
inline Q12Vec3 transform_matrix_q12_trunc(
    const std::array<std::int16_t, 9>& matrix,
    const std::array<Raw, 3>& vector) {
    const auto dot = [&](std::size_t row) {
        return static_cast<long double>(matrix[row * 3]) * vector[0]
            + static_cast<long double>(matrix[row * 3 + 1]) * vector[1]
            + static_cast<long double>(matrix[row * 3 + 2]) * vector[2];
    };
    return {
        truncate_x87_result(dot(0) * static_cast<long double>(kOneOver4096)),
        truncate_x87_result(dot(1) * static_cast<long double>(kOneOver4096)),
        truncate_x87_result(dot(2) * static_cast<long double>(kOneOver4096)),
    };
}

// 0x004103b0: normalize the signed difference between two 12-bit angles.
// The original takes (second - first), then chooses the shortest turn in the
// half-open interval [-0x800, 0x800).
inline std::int32_t angle_delta12(
    std::uint16_t first,
    std::uint16_t second) {
    std::int32_t delta = static_cast<std::int32_t>(second & kAngleMask)
        - static_cast<std::int32_t>(first & kAngleMask);
    if (delta > 0x800) {
        delta -= 0x1000;
    }
    if (delta < -0x800) {
        delta += 0x1000;
    }
    return delta;
}

// Camera_Shake's per-axis decay in 0x0040f850. The rate is a byte field and
// the stored axis is an s16, so preserve the original 16-bit wrap at the
// state boundary.
inline std::int16_t decay_shake_axis(std::int16_t value, std::uint8_t rate) {
    const std::int32_t next = static_cast<std::int32_t>(value)
        + (value < 0 ? static_cast<std::int32_t>(rate)
                     : -static_cast<std::int32_t>(rate));
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(next));
}

// The update path clears an axis when its decayed value crosses the sign of
// the pre-effect camera value. Keep the comparison separate from decay so a
// caller can reproduce the original order of operations.
inline std::int16_t zero_shake_on_sign_crossing(
    std::int16_t before,
    std::int16_t after) {
    const auto before_bits = static_cast<std::uint16_t>(before);
    const auto after_bits = static_cast<std::uint16_t>(after);
    return ((before_bits ^ after_bits) & 0x8000U) != 0 ? 0 : after;
}

inline std::uint32_t wrap_u32(std::uint64_t value) {
    return static_cast<std::uint32_t>(value);
}

inline std::uint32_t isqrt_u32_x87(std::uint32_t value) {
    // 0x004f53b0 converts a positive 32-bit integer to x87, executes FSQRT,
    // and returns through 0x005004f4, which truncates toward zero.
    if (value == 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(truncate_x87_result(
        std::sqrt(static_cast<long double>(value))));
}

inline std::int32_t arithmetic_shift_right_one(std::int32_t value) {
    // Keep this explicit for the negative viewport-delta case; signed right
    // shift is implementation-defined in portable C++.
    if (value >= 0) {
        return value / 2;
    }
    return static_cast<std::int32_t>(
        -((-static_cast<std::int64_t>(value) + 1) / 2));
}

inline std::int16_t low_s16(std::uint32_t value) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

inline std::uint32_t viewport_extent_token(
    std::uint16_t left,
    std::uint16_t right) {
    // The binary computes right * 0x1fffff + left in a 32-bit register.
    return wrap_u32(static_cast<std::uint64_t>(right) * 0x1fffffU + left);
}

inline std::uint32_t normalized_basis_divisor(
    std::uint32_t first,
    std::uint32_t second) {
    const std::uint32_t first_square = wrap_u32(
        static_cast<std::uint64_t>(first) * first);
    const std::uint32_t second_square = wrap_u32(
        static_cast<std::uint64_t>(second) * second);
    return isqrt_u32_x87(wrap_u32(
        static_cast<std::uint64_t>(first_square) + second_square));
}

// Reproduces the integer viewport mutation and basis-building portion of
// 0x0045e8e0. The later display normalization and global scratch side effects
// remain outside this small value-oriented helper.
// `scale_x` and `scale_y` are the runtime values at 0x00563a6c/0x00563a70.
// A false result represents the original divide-by-zero failure path; valid
// game viewports do not take it.
inline bool build_viewport_projection(
    ViewportInputRaw input,
    std::uint16_t state_selector,
    std::uint32_t scale_x,
    std::uint32_t scale_y,
    ViewportProjectionRaw& out) {
    if (scale_y == 0) {
        return false;
    }

    auto& words = input.words;
    words[5] = state_selector;

    // Exact register-width operations around 0x0045ea4a.
    const std::int32_t vertical_scale = static_cast<std::int32_t>(
        (static_cast<std::uint32_t>(words[6]) << 12) / scale_y);
    if (vertical_scale == 0) {
        return false;
    }
    const std::uint32_t width_delta =
        static_cast<std::uint32_t>(words[0]) - words[2];
    const std::uint32_t width_shifted = width_delta << 11;
    const std::int32_t width_numerator = static_cast<std::int32_t>(
        width_shifted & 0xfffff000U);
    words[7] = static_cast<std::uint16_t>(
        width_numerator / vertical_scale);
    words[8] = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(words[0]) + words[2]) >> 1);
    words[9] = static_cast<std::uint16_t>(
        (static_cast<std::uint32_t>(words[3]) + words[1]) >> 1);

    ProjectionBasisQ12 basis{};
    auto& a = basis.blocks[0];
    auto& b = basis.blocks[1];
    auto& c = basis.blocks[2];
    auto& d = basis.blocks[3];
    auto& e = basis.blocks[4];

    a = {0, 0, static_cast<std::int16_t>(0xf000), low_s16(state_selector),
         0, 0, static_cast<std::int16_t>(0x1000),
         low_s16(static_cast<std::uint32_t>(0) - words[4])};

    const std::uint32_t extent = viewport_extent_token(words[0], words[2]);
    const std::uint32_t x_numerator = wrap_u32(
        static_cast<std::uint64_t>(words[7]) * scale_x);
    const std::uint32_t x_shifted = wrap_u32(
        static_cast<std::uint64_t>(extent) << 11);
    const std::uint32_t x_divisor = normalized_basis_divisor(
        x_shifted >> 12, x_numerator >> 12);
    if (x_divisor == 0) {
        return false;
    }
    const std::uint32_t x_axis = x_numerator / x_divisor;
    const std::uint32_t x_edge = x_shifted / x_divisor;
    b = {0, low_s16(x_axis), low_s16(x_edge), 0,
         0, low_s16(0U - x_axis), low_s16(x_edge), 0};

    const std::uint32_t y_numerator = wrap_u32(
        static_cast<std::uint64_t>(words[7]) * scale_y);
    const std::uint32_t y_shifted = x_shifted;
    const std::uint32_t y_divisor = normalized_basis_divisor(
        y_shifted >> 12, y_numerator >> 12);
    if (y_divisor == 0) {
        return false;
    }
    const std::uint32_t y_axis = y_numerator / y_divisor;
    const std::uint32_t y_edge = y_shifted / y_divisor;
    c = {low_s16(y_axis), 0, low_s16(y_edge), 0,
         low_s16(0U - y_axis), 0, low_s16(y_edge), 0};

    const std::int32_t vertical_half = arithmetic_shift_right_one(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(words[1]) - words[3]));
    const std::uint32_t vertical_square = wrap_u32(
        static_cast<std::uint64_t>(words[7]) * words[7]
        + static_cast<std::int64_t>(vertical_half) * vertical_half);
    const std::uint32_t vertical_norm = isqrt_u32_x87(vertical_square);
    if (vertical_norm == 0) {
        return false;
    }
    const std::uint32_t vertical_axis = wrap_u32(
        static_cast<std::uint64_t>(words[7]) << 12) / vertical_norm;
    const std::uint32_t vertical_edge = wrap_u32(
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(vertical_half)) << 12)
        / vertical_norm;
    d = {0, low_s16(vertical_axis), low_s16(vertical_edge), 0,
         0, low_s16(0U - vertical_axis), low_s16(vertical_edge), 0};

    const std::int32_t horizontal_half = arithmetic_shift_right_one(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(words[0]) - words[2]));
    const std::uint32_t horizontal_square = wrap_u32(
        static_cast<std::uint64_t>(words[7]) * words[7]
        + static_cast<std::int64_t>(horizontal_half) * horizontal_half);
    const std::uint32_t horizontal_norm = isqrt_u32_x87(horizontal_square);
    if (horizontal_norm == 0) {
        return false;
    }
    const std::uint32_t horizontal_axis = wrap_u32(
        static_cast<std::uint64_t>(words[7]) << 12) / horizontal_norm;
    const std::uint32_t horizontal_edge = wrap_u32(
        static_cast<std::uint64_t>(static_cast<std::uint32_t>(horizontal_half)) << 12)
        / horizontal_norm;
    e = {low_s16(horizontal_axis), 0, low_s16(horizontal_edge), 0,
         low_s16(0U - horizontal_axis), 0, low_s16(horizontal_edge), 0};

    out.viewport = input;
    out.basis = basis;
    return true;
}

} // namespace opentony::camera
