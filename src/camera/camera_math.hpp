#pragma once

// Evidence-backed fixed-point camera primitives recovered from THPS2 PC.
//
// This is deliberately a small reference contract, not the production engine.
// The original code uses PE32 integers, x87 truncation, and signed shifts. Keep
// these operations visible until the remaining camera modes are reconstructed.

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
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

// Raw four-byte record emitted by the indexed/special 0x004d11d0 path at
// 0x0057e888. The renderer evidence separates this path from the common model
// transformer at 0x004d14d0; do not use these bytes as projected screen X/Y/Z.
// The binary writes byte0/1/2 and leaves the fourth byte untouched.
struct IndexedPacketByteRecordRaw {
    std::uint8_t byte0{};
    std::uint8_t byte1{};
    std::uint8_t byte2{};
    std::uint8_t untouched{};
};

// Compatibility name retained for the first camera probe fixtures. It is a
// packet record, not a claim that 0x0057e888 stores ordinary vertex positions.
using RasterVertexRecordRaw = IndexedPacketByteRecordRaw;

static_assert(sizeof(RasterVertexRecordRaw) == 4);

// Common model-path working record produced by 0x004d29e0 from
// Render_SubmitPolygon 0x004d14d0. Keep the words as raw float bits so a
// native renderer can preserve NaNs, clip flags, and auxiliary overrides
// without a host-side reinterpretation at the camera boundary.
struct TransformedVertexWorkingRecordRaw {
    std::array<std::uint32_t, 7> words{};
};

static_assert(sizeof(TransformedVertexWorkingRecordRaw) == 7 * sizeof(std::uint32_t));

// Model packet +0x1c supplies three signed shorts and one packed source word
// every eight bytes. This is the input consumed by 0x004d29e0's ordinary
// (source flags without bit 0x10) branch.
struct PackedVertexRecordRaw {
    std::int16_t x{};
    std::int16_t y{};
    std::int16_t z{};
    std::uint16_t source_flags{};
};

static_assert(sizeof(PackedVertexRecordRaw) == 8);

// Runtime scratch consumed by the common vertex transform. The executable
// stores the linear rows and bias as f32 values at 0x0056e84c and 0x0058f318,
// then uses the center/depth constants at 0x005808a0..0x005808a8. Keeping
// float bits here makes the f32 load/store boundaries explicit while avoiding
// an accidental host reinterpretation of the raw PE32 record.
struct CommonVertexTransformRaw {
    std::array<std::uint32_t, 9> linear_bits{};
    std::array<std::uint32_t, 3> bias_bits{};
    std::uint32_t center_x_bits{};
    std::uint32_t center_y_bits{};
    std::uint32_t depth_scale_bits{};
};

struct CommonVertexViewportEdgesRaw {
    // These are the unsigned-short view-input fields in comparison order:
    // input[2], input[0], input[3], input[1].
    std::uint16_t left{};
    std::uint16_t right{};
    std::uint16_t top{};
    std::uint16_t bottom{};
};

struct CommonVertexProjectionResultRaw {
    TransformedVertexWorkingRecordRaw record{};
    // The values written to the first three working words before perspective
    // conversion. They are useful for diagnostics and are not extra retail
    // record fields.
    std::array<float, 3> pre_perspective{};
};

inline float f32_from_bits(std::uint32_t bits) {
    float value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint32_t f32_to_bits(float value) {
    std::uint32_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline float store_f32_x87(long double value) {
    // The retail code performs the linear operation in x87 precision, then
    // stores the result to a float scratch word before perspective math.
    return static_cast<float>(value);
}

inline std::uint32_t common_vertex_clip_flags(
    float projected_x,
    float projected_y,
    float projected_z,
    const CommonVertexViewportEdgesRaw& viewport,
    std::uint32_t state_flags = 0,
    float near_limit = 10.0f,
    float far_limit = 20512000.0f) {
    std::uint32_t flags = 0;
    if (projected_x < static_cast<float>(viewport.left)) {
        flags |= 0x01;
    }
    if (projected_x > static_cast<float>(viewport.right)) {
        flags |= 0x02;
    }
    if (projected_y < static_cast<float>(viewport.top)) {
        flags |= 0x04;
    }
    if (projected_y > static_cast<float>(viewport.bottom)) {
        flags |= 0x08;
    }
    if (projected_z < near_limit) {
        flags |= 0x10;
    }
    // 0x0057e884 bit 0x10 suppresses the far-depth test.
    if ((state_flags & 0x10U) == 0 && projected_z >= far_limit) {
        flags |= 0x20;
    }
    return flags;
}

// Reproduces the ordinary source-flags-without-bit-0x10 branch of
// Render_TransformVertices 0x004d29e0. The three linear results are stored as
// f32 values first. The second stage then computes reciprocal depth from the
// stored Z and applies the screen center to the stored X/Y. The special source
// flag branch and its auxiliary polygon pointer remain a separate renderer
// path.
inline CommonVertexProjectionResultRaw project_common_vertex(
    const PackedVertexRecordRaw& source,
    const CommonVertexTransformRaw& transform,
    const CommonVertexViewportEdgesRaw& viewport,
    std::uint32_t state_flags = 0,
    float near_limit = 10.0f,
    float far_limit = 20512000.0f) {
    const long double x = source.x;
    const long double y = source.y;
    const long double z = source.z;
    const std::array<long double, 3> input{x, y, z};

    CommonVertexProjectionResultRaw result;
    for (std::size_t row = 0; row < 3; ++row) {
        long double value = f32_from_bits(transform.bias_bits[row]);
        for (std::size_t column = 0; column < 3; ++column) {
            value += static_cast<long double>(f32_from_bits(
                transform.linear_bits[row * 3 + column])) * input[column];
        }
        result.pre_perspective[row] = store_f32_x87(value);
    }

    const float reciprocal_depth = store_f32_x87(
        static_cast<long double>(f32_from_bits(transform.depth_scale_bits))
        / result.pre_perspective[2]);
    const float projected_x = store_f32_x87(
        static_cast<long double>(reciprocal_depth)
            * result.pre_perspective[0]
        + f32_from_bits(transform.center_x_bits));
    const float projected_y = store_f32_x87(
        static_cast<long double>(reciprocal_depth)
            * result.pre_perspective[1]
        + f32_from_bits(transform.center_y_bits));

    result.record.words = {
        f32_to_bits(projected_x),
        f32_to_bits(projected_y),
        f32_to_bits(result.pre_perspective[2]),
        f32_to_bits(reciprocal_depth),
        static_cast<std::uint32_t>(
            static_cast<std::int32_t>(source.source_flags)),
        common_vertex_clip_flags(
            projected_x, projected_y, result.pre_perspective[2], viewport,
            state_flags, near_limit, far_limit),
        0,
    };
    return result;
}

// Render_SetViewProjection receives a fourteen-short view-input record. The
// projection formulas consume fields 0..9 and mutate 5, 7, 8, and 9; fields
// 10..13 are still part of the live handoff and must be preserved for replay
// parity. Keep every word raw: the runtime globals are not yet portable
// display/FOV concepts.
struct ViewportInputRaw {
    std::array<std::uint16_t, 14> words{};
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

// FUN_004e87f0's render-state payload. The caller owns the first word; the
// helper writes the header at record+0x04 and the four following shorts at
// +0x08..+0x0e.
struct NormalizedViewportRecordRaw {
    std::uint32_t caller_word{};
    std::uint32_t header{0xe3000000U};
    std::int16_t origin_x{};
    std::int16_t origin_y{};
    std::int16_t extent_x{};
    std::int16_t extent_y{};
};

struct DisplayViewportNormalizationInputRaw {
    // DAT_029da3a0 selects the live viewport rectangle when nonzero.
    bool active_display_rect{};
    std::uint32_t display_width{};  // DAT_029da394
    std::uint32_t display_height{}; // DAT_029da398
    // DAT_005620a0 / DAT_005620b8, used when the live rectangle is disabled.
    std::int16_t default_extent_x{};
    std::int16_t default_extent_y{};
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

// 0x004f5f90: x87 dot product of two three-word integer vectors, scaled by
// 1/4096 and truncated through the shared return helper. This is used by the
// follow routine for angular/state thresholds; it is not a vector length.
inline Raw dot_q12_x87(const Q12Vec3& first, const Q12Vec3& second) {
    const long double dot = static_cast<long double>(first.x) * second.x
        + static_cast<long double>(first.y) * second.y
        + static_cast<long double>(first.z) * second.z;
    return truncate_x87_result(dot * static_cast<long double>(kOneOver4096));
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

// 0x004e2070 narrows cross-product results to the signed-short range. The
// original obtains the limits through x87 2^15 and 2^15-1 constants; express
// the resulting saturation directly while preserving the preceding 32-bit
// product/subtract operations.
inline std::int16_t saturate_s16(Raw value) {
    if (value < std::numeric_limits<std::int16_t>::min()) {
        return std::numeric_limits<std::int16_t>::min();
    }
    if (value > std::numeric_limits<std::int16_t>::max()) {
        return std::numeric_limits<std::int16_t>::max();
    }
    return static_cast<std::int16_t>(value);
}

// 0x004e2f80: cross product of two signed-short vectors followed by the
// saturating short boundary above. The camera follow path uses this twice to
// complete its orthogonal basis.
inline Q12Vec3 cross_product_s16(
    const Q12Vec3& first,
    const Q12Vec3& second) {
    return {
        saturate_s16(subtract_s32(
            multiply_s32(first.y, second.z),
            multiply_s32(first.z, second.y))),
        saturate_s16(subtract_s32(
            multiply_s32(first.z, second.x),
            multiply_s32(first.x, second.z))),
        saturate_s16(subtract_s32(
            multiply_s32(first.x, second.y),
            multiply_s32(first.y, second.x))),
    };
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

// 0x004a9a00: convert the signed-short 3x3 basis used by Camera_FollowTarget
// back into the four-word transform payload. The positive-trace path is the
// common normal-follow path. The alternate branch preserves the original
// largest-diagonal selection and the retail lookup order [1, 2, 0].
inline TransformQ12 matrix_to_transform_q12(const MatrixQ12& matrix) {
    const Raw trace = static_cast<Raw>(matrix[0])
        + static_cast<Raw>(matrix[4])
        + static_cast<Raw>(matrix[8]);
    if (trace > 0) {
        const Raw root_input = wrap_s32(
            static_cast<std::int64_t>(trace + kQ12One) << 12);
        const auto root = root_input > 0
            ? isqrt_u32_x87(static_cast<std::uint32_t>(root_input))
            : 0U;
        const Raw scale = root == 0 ? 0 : static_cast<Raw>(0x800000U / root);
        return {
            sar12(multiply_s32(
                static_cast<Raw>(matrix[5]) - static_cast<Raw>(matrix[7]), scale)),
            sar12(multiply_s32(
                static_cast<Raw>(matrix[6]) - static_cast<Raw>(matrix[2]), scale)),
            sar12(multiply_s32(
                static_cast<Raw>(matrix[1]) - static_cast<Raw>(matrix[3]), scale)),
            arithmetic_shift_right(static_cast<Raw>(root), 1),
        };
    }

    constexpr std::array<std::size_t, 3> next_axis{1, 2, 0};
    std::size_t pivot = static_cast<std::size_t>(
        static_cast<Raw>(matrix[0]) < static_cast<Raw>(matrix[4]));
    if (static_cast<Raw>(matrix[pivot * 4]) < static_cast<Raw>(matrix[8])) {
        pivot = 2;
    }
    const std::size_t first = next_axis[pivot];
    const std::size_t second = next_axis[first];
    const Raw diagonal_difference = static_cast<Raw>(matrix[pivot * 4])
        - static_cast<Raw>(matrix[second * 4])
        - static_cast<Raw>(matrix[first * 4]);
    const Raw root_input = wrap_s32(
        static_cast<std::int64_t>(diagonal_difference + kQ12One) << 12);
    const auto root = root_input > 0
        ? isqrt_u32_x87(static_cast<std::uint32_t>(root_input))
        : 0U;
    const Raw half_root = arithmetic_shift_right(static_cast<Raw>(root), 1);
    const Raw scale = root == 0 ? 0 : static_cast<Raw>(0x800000U / root);
    TransformQ12 result{};
    const auto set_component = [&](std::size_t index, Raw value) {
        if (index == 0) {
            result.x = value;
        } else if (index == 1) {
            result.y = value;
        } else {
            result.z = value;
        }
    };
    set_component(pivot, half_root);
    result.w = sar12(multiply_s32(
        static_cast<Raw>(matrix[first + second * 3])
            - static_cast<Raw>(matrix[second + first * 3]),
        scale));
    set_component(first, sar12(multiply_s32(
        static_cast<Raw>(matrix[pivot * 3 + first])
            + static_cast<Raw>(matrix[first * 3 + pivot]),
        scale)));
    set_component(second, sar12(multiply_s32(
        static_cast<Raw>(matrix[pivot * 3 + second])
            + static_cast<Raw>(matrix[second * 3 + pivot]),
        scale)));
    return result;
}

// 0x004f53e0: the view setup copies the nine prepared shorts into the backend
// record in transposed order before downstream render consumption.
inline MatrixQ12 transpose_matrix_q12(const MatrixQ12& matrix) {
    return {
        matrix[0], matrix[3], matrix[6],
        matrix[1], matrix[4], matrix[7],
        matrix[2], matrix[5], matrix[8],
    };
}

// Dynamic Warehouse traces establish the two render-record conventions:
// `+0x34` contains the row-ordered result of 0x004a9910 and the backend view
// record at `+0x54` contains its literal 0x004f53e0 transpose.  Keep this
// adapter separate from transform_to_matrix_q12 so callers cannot silently
// substitute a graphics-API convention for the retail record layout.
inline MatrixQ12 camera_view_record_matrix_q12(const TransformQ12& transform) {
    return transpose_matrix_q12(transform_to_matrix_q12(transform));
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

struct ViewPreparationRecordsRaw {
    // Render_SetViewProjection writes fifteen signed shorts to each of these
    // two scratch records at 0x005620c0 and 0x005620e8.  They are five
    // independent three-word products, not a conventional 4x4 matrix.
    std::array<std::int16_t, 15> record_005620c0{};
    std::array<std::int16_t, 15> record_005620e8{};
};

// Exact five-iteration handoff inside 0x0045e8e0.  The retail loop starts at
// basis word 1, advances four words per iteration, and multiplies the two
// cyclic three-word slices by the row-ordered view matrix.  Keeping the raw
// basis indexing here makes the object-submission contract testable without
// assigning the scratch arrays a graphics-API meaning.
inline ViewPreparationRecordsRaw prepare_view_records_q12(
    const MatrixQ12& view_matrix,
    const ProjectionBasisQ12& basis) {
    std::array<std::int16_t, 40> flat{};
    for (std::size_t block = 0; block < basis.blocks.size(); ++block) {
        for (std::size_t word = 0; word < basis.blocks[block].size(); ++word) {
            flat[block * 8 + word] = basis.blocks[block][word];
        }
    }

    ViewPreparationRecordsRaw records{};
    for (std::size_t iteration = 0; iteration < 5; ++iteration) {
        const std::size_t start = 1 + iteration * 4;
        const std::array<Raw, 3> first_vector{
            flat[start - 1], flat[start], flat[start + 1]};
        const std::array<Raw, 3> second_vector{
            flat[start + 11], flat[start + 12], flat[start + 13]};
        const Q12Vec3 first = multiply_matrix_q12(view_matrix, first_vector);
        const Q12Vec3 second = multiply_matrix_q12(view_matrix, second_vector);
        records.record_005620e8[iteration * 3 + 0] = low_s16_raw(first.x);
        records.record_005620e8[iteration * 3 + 1] = low_s16_raw(first.y);
        records.record_005620e8[iteration * 3 + 2] = low_s16_raw(first.z);
        records.record_005620c0[iteration * 3 + 0] = low_s16_raw(second.x);
        records.record_005620c0[iteration * 3 + 1] = low_s16_raw(second.y);
        records.record_005620c0[iteration * 3 + 2] = low_s16_raw(second.z);
    }
    return records;
}

// 0x004e85a0's row-ordered integer form. The binary performs each dot product
// through x87, multiplies by 1/4096, and converts with the shared
// round-toward-zero helper. This intentionally differs from
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

// Exact output ordering used by 0x004e85a0. Its three results are generated
// from matrix rows 1, 2, and 0 in that order. The rotation records passed by
// the camera tail are laid out so this cyclic order is part of the observable
// helper contract; do not silently replace it with a conventional row-0/1/2
// vector multiply when recreating the position/effect stage.
inline Q12Vec3 camera_transform_matrix_q12_trunc(
    const std::array<std::int16_t, 9>& matrix,
    const std::array<Raw, 3>& vector) {
    const auto dot = [&](std::size_t row) {
        return static_cast<long double>(matrix[row * 3]) * vector[0]
            + static_cast<long double>(matrix[row * 3 + 1]) * vector[1]
            + static_cast<long double>(matrix[row * 3 + 2]) * vector[2];
    };
    return {
        truncate_x87_result(dot(1) * static_cast<long double>(kOneOver4096)),
        truncate_x87_result(dot(2) * static_cast<long double>(kOneOver4096)),
        truncate_x87_result(dot(0) * static_cast<long double>(kOneOver4096)),
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

// Exact display-rectangle normalization at 0x0045e93e..0x0045e9d6. The
// original uses zero-extended DIV after sign-extending each source short;
// the low sixteen bits are then copied into the render record. Valid game
// rectangles are positive, but keeping the register-width behavior here
// avoids silently turning this into a float/normalized-device-coordinate API.
inline bool normalize_viewport_record(
    const ViewportInputRaw& viewport,
    const DisplayViewportNormalizationInputRaw& config,
    NormalizedViewportRecordRaw& out) {
    if (config.display_width == 0 || config.display_height == 0) {
        return false;
    }

    std::uint16_t source_x = 0;
    std::uint16_t source_y = 0;
    std::int16_t source_extent_x = config.default_extent_x;
    std::int16_t source_extent_y = config.default_extent_y;
    if (config.active_display_rect) {
        source_x = viewport.words[2];
        source_y = viewport.words[3];
        source_extent_x = low_s16(
            static_cast<std::uint32_t>(viewport.words[0]) - viewport.words[2]);
        source_extent_y = low_s16(
            static_cast<std::uint32_t>(viewport.words[1]) - viewport.words[3]);
    }

    const auto unsigned_divide = [](std::int32_t numerator,
                                    std::uint32_t denominator) {
        return static_cast<std::uint32_t>(numerator) / denominator;
    };
    out = {};
    out.header = 0xe3000000U;
    out.origin_x = low_s16(unsigned_divide(
        static_cast<std::int32_t>(static_cast<std::int16_t>(source_x)) << 9,
        config.display_width));
    out.origin_y = low_s16(unsigned_divide(
        static_cast<std::int32_t>(static_cast<std::int16_t>(source_y)) * 0xf0,
        config.display_height));
    out.extent_x = low_s16(unsigned_divide(
        static_cast<std::int32_t>(source_extent_x) << 9,
        config.display_width));
    out.extent_y = low_s16(unsigned_divide(
        static_cast<std::int32_t>(source_extent_y) * 0xf0,
        config.display_height));

    // FUN_004e87f0 replaces zero extents with one so later raster operations
    // never receive a zero-sized rectangle.
    if (out.extent_x == 0) {
        out.extent_x = 1;
    }
    if (out.extent_y == 0) {
        out.extent_y = 1;
    }
    return true;
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
    // The retail sequence is `(left - right) & 0x1ffffe`, followed by a
    // 0xb-bit shift. Express the mask before the shift so unusual wrapped
    // viewport words do not rely on an accidental host shift equivalence.
    const std::uint32_t width_shifted = wrap_u32(
        static_cast<std::uint64_t>(width_delta & 0x1ffffeU) << 11);
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
    const std::uint32_t extent_half = (extent & 0x1fffffU) >> 1;
    // The divisor uses the masked half-extent, but the edge numerator keeps
    // the original extent token and multiplies it by 0x800. These are equal
    // for the common even viewport widths, but differ for odd or wrapped
    // records and must remain separate PE32 expressions.
    const std::uint32_t edge_token_q12 = wrap_u32(
        static_cast<std::uint64_t>(extent) << 11);
    const std::uint32_t x_divisor = normalized_basis_divisor(
        extent_half, x_numerator >> 12);
    if (x_divisor == 0) {
        return false;
    }
    const std::uint32_t x_axis = x_numerator / x_divisor;
    const std::uint32_t x_edge = edge_token_q12 / x_divisor;
    b = {0, low_s16(x_axis), low_s16(x_edge), 0,
         0, low_s16(0U - x_axis), low_s16(x_edge), 0};

    const std::uint32_t y_numerator = wrap_u32(
        static_cast<std::uint64_t>(words[7]) * scale_y);
    const std::uint32_t y_divisor = normalized_basis_divisor(
        extent_half, y_numerator >> 12);
    if (y_divisor == 0) {
        return false;
    }
    const std::uint32_t y_axis = y_numerator / y_divisor;
    const std::uint32_t y_edge = edge_token_q12 / y_divisor;
    c = {low_s16(y_axis), 0, low_s16(y_edge), 0,
         low_s16(0U - y_axis), 0, low_s16(y_edge), 0};

    const std::int32_t vertical_half = arithmetic_shift_right_one(
        static_cast<std::int32_t>(static_cast<std::uint32_t>(words[1]) - words[3]));
    const std::uint32_t vertical_square = static_cast<std::uint32_t>(
        add_s32(
            multiply_s32(static_cast<Raw>(words[7]),
                         static_cast<Raw>(words[7])),
            multiply_s32(vertical_half, vertical_half)));
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
    const std::uint32_t horizontal_square = static_cast<std::uint32_t>(
        add_s32(
            multiply_s32(static_cast<Raw>(words[7]),
                         static_cast<Raw>(words[7])),
            multiply_s32(horizontal_half, horizontal_half)));
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
