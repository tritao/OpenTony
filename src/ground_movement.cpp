#include "ground_movement.hpp"

#include <bit>
#include <cmath>

namespace opentony::ground {
namespace {

constexpr std::int32_t kNormalTurnCap = 0x2d000;
constexpr std::int32_t kFastTurnCap = 0x5a000;

[[nodiscard]] std::int32_t bits_to_i32(std::uint32_t bits) noexcept {
    return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] std::uint32_t bits_from_i32(std::int32_t value) noexcept {
    return std::bit_cast<std::uint32_t>(value);
}

[[nodiscard]] std::int16_t bits_to_i16(std::uint16_t bits) noexcept {
    return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int32_t dot_q12(const Vec3i32& lhs, const Vec3i32& rhs) noexcept {
    const auto sum = static_cast<std::int64_t>(lhs.x) * rhs.x
                   + static_cast<std::int64_t>(lhs.y) * rhs.y
                   + static_cast<std::int64_t>(lhs.z) * rhs.z;
    // 0x004f5f90 uses x87 division by 4096 followed by round-toward-zero.
    return wrap32(sum / 4096);
}

[[nodiscard]] std::uint32_t integer_sqrt(std::uint32_t value) noexcept {
    std::uint32_t result = 0;
    std::uint32_t bit = std::uint32_t{1} << 30;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

[[nodiscard]] std::int32_t turn_base(const TurnConfig& config) noexcept {
    if (config.tuning == 0) {
        return 0x3c;
    }
    if (config.tuning == 1) {
        return 0x78;
    }
    return 0xb4;
}

[[nodiscard]] std::int32_t component_scale_q8(std::int32_t value,
                                               std::int32_t scalar_q8) noexcept {
    return arithmetic_shift_right(mul_low32(value, scalar_q8), 8);
}

} // namespace

std::int32_t wrap32(std::int64_t value) noexcept {
    return bits_to_i32(static_cast<std::uint32_t>(value));
}

std::int16_t wrap16(std::int32_t value) noexcept {
    return bits_to_i16(static_cast<std::uint16_t>(value));
}

std::int32_t add32(std::int32_t lhs, std::int32_t rhs) noexcept {
    return wrap32(static_cast<std::int64_t>(lhs) + rhs);
}

std::int32_t sub32(std::int32_t lhs, std::int32_t rhs) noexcept {
    return wrap32(static_cast<std::int64_t>(lhs) - rhs);
}

std::int32_t mul_low32(std::int32_t lhs, std::int32_t rhs) noexcept {
    return wrap32(static_cast<std::int64_t>(lhs) * rhs);
}

std::int32_t arithmetic_shift_right(std::int32_t value, unsigned amount) noexcept {
    if (amount == 0) {
        return value;
    }
    if (amount >= 32) {
        return value < 0 ? -1 : 0;
    }

    auto shifted = bits_from_i32(value) >> amount;
    if (value < 0) {
        shifted |= ~std::uint32_t{0} << (32 - amount);
    }
    return bits_to_i32(shifted);
}

std::int32_t trunc_div32(std::int32_t numerator, std::int32_t denominator) noexcept {
    // The recovered callsite divides by small nonzero positive values.  Use a
    // widened quotient so C++ signed-overflow rules cannot alter x86 idiv.
    if (denominator == 0) {
        return 0;
    }
    const auto quotient = static_cast<std::int64_t>(numerator) / denominator;
    return wrap32(quotient);
}

std::int32_t frame_scale_squared_q8(std::int32_t frame_scale_q8) noexcept {
    return arithmetic_shift_right(mul_low32(frame_scale_q8, frame_scale_q8), 8);
}

std::int32_t multiply_q12_trunc(std::int32_t lhs, std::int32_t rhs) noexcept {
    // 0x004f5fc0 multiplies two integer words in x87 and divides by 4096
    // before the shared round-toward-zero conversion.
    return wrap32((static_cast<std::int64_t>(lhs) * rhs) / 4096);
}

Vec3i32 integrate_position_q16(const Vec3i32& position_q16,
                               const Vec3i32& velocity_q16,
                               const Vec3i32& correction_q16,
                               std::int32_t frame_scale_q8) noexcept {
    const auto squared = frame_scale_squared_q8(frame_scale_q8);

    const auto acceleration = Vec3i32{
        trunc_div32(arithmetic_shift_right(mul_low32(correction_q16.x, squared), 8), 2),
        trunc_div32(arithmetic_shift_right(mul_low32(correction_q16.y, squared), 8), 2),
        trunc_div32(arithmetic_shift_right(mul_low32(correction_q16.z, squared), 8), 2),
    };
    const auto velocity_term = Vec3i32{
        component_scale_q8(velocity_q16.x, frame_scale_q8),
        component_scale_q8(velocity_q16.y, frame_scale_q8),
        component_scale_q8(velocity_q16.z, frame_scale_q8),
    };

    return {
        add32(position_q16.x, add32(acceleration.x, velocity_term.x)),
        add32(position_q16.y, add32(acceleration.y, velocity_term.y)),
        add32(position_q16.z, add32(acceleration.z, velocity_term.z)),
    };
}

void update_grounded_turn(TurnState& state,
                          std::uint16_t action_mask,
                          const TurnConfig& config,
                          std::int32_t frame_scale_q8) noexcept {
    const auto base = turn_base(config);
    const auto step = arithmetic_shift_right(
        mul_low32(mul_low32(base, 0x100), frame_scale_q8), 8);
    const auto cap = config.fast_turn ? kFastTurnCap : kNormalTurnCap;
    const bool left = (action_mask & kLeftAction) != 0;
    const bool right = (action_mask & kRightAction) != 0;

    state.active = left || right;
    if (left) {
        state.accumulator = sub32(state.accumulator, step);
        if (state.accumulator < -cap) {
            state.accumulator = -cap;
        }
    } else if (right) {
        state.accumulator = add32(state.accumulator, step);
        if (state.accumulator > cap) {
            state.accumulator = cap;
        }
    } else {
        const auto decay = arithmetic_shift_right(state.accumulator,
                                                  config.fast_turn ? 1U : 2U);
        state.accumulator = sub32(state.accumulator, decay);
        if (((static_cast<std::uint32_t>(state.accumulator) + 0x800U) & 0xfffff000U) == 0) {
            state.accumulator = 0;
        }
    }
    state.mirror = state.accumulator;
}

std::int32_t turn_angle_q12(std::int32_t turn_accumulator,
                            std::int32_t frame_scale_q8) noexcept {
    const auto turn_units = arithmetic_shift_right(turn_accumulator, 12);
    return arithmetic_shift_right(mul_low32(frame_scale_q8, turn_units), 8);
}

MatrixQ12 multiply_q12(const MatrixQ12& lhs, const MatrixQ12& rhs) noexcept {
    MatrixQ12 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            auto sum = std::int32_t{};
            for (std::size_t inner = 0; inner < 3; ++inner) {
                // 0x004e3130 uses three low-32-bit imul results, x86 add,
                // then arithmetic sar 12 before the short store.
                sum = add32(sum, mul_low32(lhs.at(row, inner), rhs.at(inner, column)));
            }
            result.at(row, column) = wrap16(arithmetic_shift_right(sum, 12));
        }
    }
    return result;
}

MatrixQ12 transpose(const MatrixQ12& matrix) noexcept {
    MatrixQ12 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            result.at(row, column) = matrix.at(column, row);
        }
    }
    return result;
}

MatrixQ12 rotation_y_q12(std::int32_t angle_q12) noexcept {
    const auto angle = static_cast<std::uint32_t>(angle_q12) & 0xfffU;
    constexpr long double scale = 4096.0L;
    constexpr long double pi = 3.141592653589793238462643383279502884L;
    const auto radians = static_cast<long double>(angle) * (2.0L * pi / 4096.0L);
    const auto cosine = static_cast<std::int32_t>(std::trunc(std::cos(radians) * scale));
    const auto sine = static_cast<std::int32_t>(std::trunc(std::sin(radians) * scale));

    MatrixQ12 result{};
    result.at(0, 0) = wrap16(cosine);
    result.at(0, 2) = wrap16(sine);
    result.at(1, 1) = 0x1000;
    result.at(2, 0) = wrap16(-sine);
    result.at(2, 2) = wrap16(cosine);
    return result;
}

BasisQ12 basis_from_matrix(const MatrixQ12& matrix) noexcept {
    return {
        {matrix.at(0, 2), matrix.at(1, 2), matrix.at(2, 2)},
        {matrix.at(0, 0), matrix.at(1, 0), matrix.at(2, 0)},
        {matrix.at(0, 1), matrix.at(1, 1), matrix.at(2, 1)},
    };
}

Vec3i32 transform_q12(const MatrixQ12& matrix, const Vec3i32& vector_q16) noexcept {
    const Vec3i32 rows[] = {
        {matrix.at(0, 0), matrix.at(0, 1), matrix.at(0, 2)},
        {matrix.at(1, 0), matrix.at(1, 1), matrix.at(1, 2)},
        {matrix.at(2, 0), matrix.at(2, 1), matrix.at(2, 2)},
    };
    return {
        dot_q12(rows[0], vector_q16),
        dot_q12(rows[1], vector_q16),
        dot_q12(rows[2], vector_q16),
    };
}

void project_ground_motion(Vec3i32& velocity_q16,
                           Vec3i32& correction_q16,
                           const BasisQ12& basis,
                           std::int32_t frame_scale_q8,
                           bool surface_gate,
                           bool suppress_forward_correction) noexcept {
    const auto lateral_scalar = dot_q12(velocity_q16, basis.axis1);
    const Vec3i32 lateral{
        multiply_q12_trunc(lateral_scalar, basis.axis1.x),
        multiply_q12_trunc(lateral_scalar, basis.axis1.y),
        multiply_q12_trunc(lateral_scalar, basis.axis1.z),
    };
    velocity_q16 = {
        sub32(velocity_q16.x, lateral.x),
        sub32(velocity_q16.y, lateral.y),
        sub32(velocity_q16.z, lateral.z),
    };

    const auto forward_scalar = dot_q12(velocity_q16, basis.axis0);
    const Vec3i32 forward{
        multiply_q12_trunc(forward_scalar, basis.axis0.x),
        multiply_q12_trunc(forward_scalar, basis.axis0.y),
        multiply_q12_trunc(forward_scalar, basis.axis0.z),
    };
    if (surface_gate && !suppress_forward_correction) {
        const Vec3i32 eight_forward{
            multiply_q12_trunc(8, forward.x),
            multiply_q12_trunc(8, forward.y),
            multiply_q12_trunc(8, forward.z),
        };
        correction_q16 = {
            sub32(correction_q16.x, component_scale_q8(eight_forward.x, frame_scale_q8)),
            sub32(correction_q16.y, component_scale_q8(eight_forward.y, frame_scale_q8)),
            sub32(correction_q16.z, component_scale_q8(eight_forward.z, frame_scale_q8)),
        };
    }
}

void rotate_velocity_for_ground_turn(Vec3i32& velocity_q16,
                                     const MatrixQ12& saved_old_orientation,
                                     std::int32_t angle_q12,
                                     std::int16_t offset_q12) noexcept {
    const auto old_speed_squared = dot_q12(velocity_q16, velocity_q16);
    const auto old_speed = old_speed_squared > 0
        ? static_cast<std::int32_t>(integer_sqrt(static_cast<std::uint32_t>(old_speed_squared)))
        : 0;

    // This mirrors the three matrix phases in 0x0049b500's param_3 branch:
    // transpose(saved) * R, then saved * that result, then transform velocity.
    const auto phase = rotation_y_q12(sub32(angle_q12, offset_q12));
    const auto local_phase = multiply_q12(transpose(saved_old_orientation), phase);
    const auto effective = multiply_q12(saved_old_orientation, local_phase);
    auto rotated = transform_q12(effective, velocity_q16);

    const auto new_speed_squared = dot_q12(rotated, rotated);
    const auto new_speed = new_speed_squared > 0
        ? static_cast<std::int32_t>(integer_sqrt(static_cast<std::uint32_t>(new_speed_squared)))
        : 0;
    const auto old_ratio = arithmetic_shift_right(mul_low32(old_speed, 0x40), 8);
    const auto new_ratio = arithmetic_shift_right(mul_low32(new_speed, 0x40), 8);
    if (new_ratio > 0) {
        rotated.x = trunc_div32(mul_low32(rotated.x, old_ratio), new_ratio);
        rotated.y = trunc_div32(mul_low32(rotated.y, old_ratio), new_ratio);
        rotated.z = trunc_div32(mul_low32(rotated.z, old_ratio), new_ratio);
    }
    velocity_q16 = rotated;
}

GroundFrameResult step_grounded(GroundState& state,
                                const GroundFrameInput& input,
                                CandidateResolver resolver) {
    GroundFrameResult result{};

    // 0x0049e680 calls 0x0049c7d0 before 0x00493370.  Preserve both the
    // saved matrix and its columns because 0x0049b010 consumes this old basis.
    result.old_orientation = state.orientation;
    result.old_position_q16 = state.position_q16;
    state.basis = basis_from_matrix(state.orientation);
    result.old_basis = state.basis;

    update_grounded_turn(state.turn, input.action_mask, input.turn_config,
                         input.frame_scale_q8);

    result.integrated_position_q16 = integrate_position_q16(
        state.position_q16,
        state.velocity_q16,
        input.prephysics_correction_q16,
        input.frame_scale_q8);
    state.position_q16 = result.integrated_position_q16;

    result.angle_q12 = turn_angle_q12(state.turn.accumulator, input.frame_scale_q8);
    state.orientation = multiply_q12(state.orientation, rotation_y_q12(result.angle_q12));
    state.basis = basis_from_matrix(state.orientation);

    if (input.rotate_velocity) {
        rotate_velocity_for_ground_turn(state.velocity_q16,
                                        result.old_orientation,
                                        result.angle_q12,
                                        input.velocity_rotation_offset_q12);
    }

    if (input.apply_ground_projection) {
        auto correction = input.prephysics_correction_q16;
        project_ground_motion(state.velocity_q16,
                              correction,
                              state.basis,
                              input.frame_scale_q8,
                              input.surface_gate,
                              input.suppress_forward_correction);
        result.correction_after_projection_q16 = correction;
    } else {
        result.correction_after_projection_q16 = input.prephysics_correction_q16;
    }

    result.candidate_before_commit_q16 = state.position_q16;
    // The outer 0x0049e680 wrapper restores player+0x08/+0x0c/+0x10 from
    // +0xbc/+0xc0/+0xc4 immediately before calling 0x00496060.  The resolver
    // therefore sees the old position and receives the candidate separately.
    state.position_q16 = result.old_position_q16;
    result.committed_position_q16 = resolver
        ? resolver(state, result.candidate_before_commit_q16)
        : result.candidate_before_commit_q16;
    state.position_q16 = result.committed_position_q16;
    return result;
}

} // namespace opentony::ground
