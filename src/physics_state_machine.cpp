#include "physics_state_machine.hpp"

#include <algorithm>
#include <bit>
#include <limits>

namespace opentony::physics {

namespace {

std::int32_t ground_wrap32(std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

std::int32_t ground_arithmetic_shift_right(
    std::int32_t value,
    unsigned amount) noexcept {
    if (amount == 0) {
        return value;
    }
    if (amount >= 32) {
        return value < 0 ? -1 : 0;
    }
    std::uint32_t shifted = static_cast<std::uint32_t>(value) >> amount;
    if (value < 0) {
        shifted |= ~std::uint32_t{0} << (32 - amount);
    }
    return std::bit_cast<std::int32_t>(shifted);
}

std::int32_t ground_add32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return ground_wrap32(static_cast<std::int64_t>(left) + right);
}

std::int32_t ground_sub32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return ground_wrap32(static_cast<std::int64_t>(left) - right);
}

std::int32_t ground_mul_low32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return ground_wrap32(static_cast<std::int64_t>(left) * right);
}

std::int32_t ground_signed_div32(
    std::int32_t numerator,
    std::int32_t denominator) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::int64_t>(numerator) / denominator);
}

std::int64_t absolute_value(std::int64_t value) noexcept {
    return value < 0 ? -value : value;
}

std::int32_t narrow_impulse(std::int64_t value) noexcept {
    if (value > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

std::int32_t wrap_i32(std::int64_t value) noexcept {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

std::int32_t wrap_add_i32(std::int32_t left, std::int32_t right) noexcept {
    return wrap_i32(static_cast<std::int64_t>(left) + right);
}

std::int32_t wrap_subtract_i32(std::int32_t left,
                               std::int32_t right) noexcept {
    return wrap_i32(static_cast<std::int64_t>(left) - right);
}

std::int32_t wrap_multiply_i32(std::int32_t left,
                               std::int32_t right) noexcept {
    return wrap_i32(static_cast<std::int64_t>(left) * right);
}

std::int32_t wrap_negate_i32(std::int32_t value) noexcept {
    return wrap_i32(-static_cast<std::int64_t>(value));
}

std::int32_t sign_extend_short(std::int32_t value) noexcept {
    const std::uint32_t bits = static_cast<std::uint32_t>(value) & 0xffffu;
    return bits >= 0x8000u ? static_cast<std::int32_t>(bits) - 0x10000
                           : static_cast<std::int32_t>(bits);
}

FixedVec3 signed_short_components(const FixedVec3& value) noexcept {
    return FixedVec3{sign_extend_short(value.x), sign_extend_short(value.y),
                     sign_extend_short(value.z)};
}

std::int16_t narrow_orientation_short(std::int64_t value) noexcept {
    const std::uint16_t bits = static_cast<std::uint16_t>(value & 0xffff);
    return bits >= 0x8000
               ? static_cast<std::int16_t>(static_cast<std::int32_t>(bits) -
                                           0x10000)
               : static_cast<std::int16_t>(bits);
}

std::array<std::int16_t, 9> rotate_orientation_z_small(
    const std::array<std::int16_t, 9>& orientation,
    std::int32_t angle) noexcept {
    // FUN_004e80e0 receives 11 or 0xff5 angle units. Its x87 path uses
    // angle * (2*pi / 0x1000), then truncates sin/cos after scaling by 0x1000:
    // cos(11) = 4095 and sin(11) = 69 in the retail Q12 domain. The 0xff5
    // branch is the same rotation with the negative sine.
    const std::int32_t sine = angle > 0 ? 69 : -69;
    constexpr std::int32_t cosine = 4095;
    constexpr std::int32_t one = 4096;
    const std::array<std::int32_t, 9> rotation{
        cosine, -sine, 0,
        sine, cosine, 0,
        0, 0, one,
    };

    std::array<std::int16_t, 9> result{};
    for (std::size_t row = 0; row != 3; ++row) {
        for (std::size_t column = 0; column != 3; ++column) {
            std::int64_t sum = 0;
            for (std::size_t index = 0; index != 3; ++index) {
                sum += static_cast<std::int32_t>(orientation[row * 3 + index]) *
                       rotation[index * 3 + column];
            }
            result[row * 3 + column] = narrow_orientation_short(sum >> 12);
        }
    }
    return result;
}

std::int32_t fixed_shift8(std::int64_t value) noexcept {
    // The retail helper uses SAR, rather than C/C++ division, for these
    // fixed-point conversions. The supported native targets use an
    // arithmetic right shift for signed integers; spelling it as a helper
    // keeps that binary operation visible at the call sites.
    return static_cast<std::int32_t>(value >> retail::kFixedPointShift);
}

std::int64_t truncate_fixed12(std::int64_t value) noexcept {
    // FUN_005004f4 ORs 0x0c into the high control-word byte, selecting x87
    // round-toward-zero for FISTP. C++ signed division has the same quotient
    // rule for this positive denominator.
    return value / 0x1000;
}

std::int64_t saturating_add(std::int64_t left, std::int64_t right) noexcept {
    if (right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return left + right;
}

std::uint64_t integer_square_root(std::uint64_t value) noexcept;

std::int32_t speed_metric(const FixedVec3& velocity) noexcept {
    const std::int32_t squared = fixed12_dot(velocity, velocity);
    const std::uint64_t nonnegative_squared =
        squared > 0 ? static_cast<std::uint64_t>(squared) : 0;
    const std::uint64_t length = integer_square_root(nonnegative_squared);
    const std::uint64_t metric = length * 0x40u;
    return metric > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())
               ? std::numeric_limits<std::int32_t>::max()
               : static_cast<std::int32_t>(metric);
}

std::int32_t threshold_from_random(std::int32_t random,
                                   std::int32_t offset) noexcept {
    return narrow_impulse((static_cast<std::int64_t>(random) + offset) *
                           0x2d000 / 0x118);
}

bool uses_common_air_handler(std::int32_t raw_state) noexcept {
    return raw_state == 1 || raw_state == 3 || raw_state == 6;
}

std::int32_t damp_toward_zero(std::int32_t value,
                              std::int32_t shift) noexcept {
    const std::int32_t correction = static_cast<std::int32_t>(
        (static_cast<std::int64_t>(value) +
         ((value < 0) ? ((std::int64_t{1} << shift) - 1) : 0)) >> shift);
    return value - correction;
}

std::uint64_t integer_square_root(std::uint64_t value) noexcept {
    // FUN_004f53b0 is an x87 sqrt followed by the retail truncate-toward-zero
    // conversion. For a nonnegative square root this is the floor, so the
    // restoring integer implementation is exact and host-float independent.
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;
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

}  // namespace

std::int32_t fixed12_dot(const FixedVec3& left, const FixedVec3& right) noexcept {
    const std::int64_t x = static_cast<std::int64_t>(left.x) * right.x;
    const std::int64_t y = static_cast<std::int64_t>(left.y) * right.y;
    const std::int64_t z = static_cast<std::int64_t>(left.z) * right.z;
    const std::int64_t product = saturating_add(saturating_add(x, y), z);
    return narrow_impulse(truncate_fixed12(product));
}

std::int32_t fixed12_scalar_multiply(std::int32_t scalar,
                                     std::int32_t value) noexcept {
    return narrow_impulse(
        truncate_fixed12(static_cast<std::int64_t>(scalar) * value));
}

FixedVec3 fixed12_scalar_multiply(std::int32_t scalar,
                                  const FixedVec3& value) noexcept {
    return FixedVec3{
        fixed12_scalar_multiply(scalar, value.x),
        fixed12_scalar_multiply(scalar, value.y),
        fixed12_scalar_multiply(scalar, value.z),
    };
}

FixedVec3 decode_surface_normal(PackedSurfaceNormal packed) noexcept {
    const auto signed_half = [](std::uint32_t word) {
        const std::int32_t value = static_cast<std::int32_t>(word & 0xffffu);
        return value >= 0x8000 ? value - 0x10000 : value;
    };
    return FixedVec3{
        signed_half(packed.xy),
        signed_half(packed.xy >> 16),
        signed_half(packed.z),
    };
}

FixedVec3 project_vector_off_surface(const FixedVec3& velocity,
                                     PackedSurfaceNormal packed) noexcept {
    const FixedVec3 normal = decode_surface_normal(packed);
    const std::int32_t normal_dot = fixed12_dot(velocity, normal);
    const FixedVec3 projection =
        fixed12_scalar_multiply(normal_dot, normal);
    return FixedVec3{
        narrow_impulse(static_cast<std::int64_t>(velocity.x) - projection.x),
        narrow_impulse(static_cast<std::int64_t>(velocity.y) - projection.y),
        narrow_impulse(static_cast<std::int64_t>(velocity.z) - projection.z),
    };
}

SurfaceVelocityProjectionResult project_velocity_preserving_speed(
    const FixedVec3& velocity, PackedSurfaceNormal packed) noexcept {
    SurfaceVelocityProjectionResult result;
    result.original_speed_metric = speed_metric(velocity);
    result.velocity = project_vector_off_surface(velocity, packed);
    result.projected_speed_metric = speed_metric(result.velocity);

    const std::int32_t original_length = result.original_speed_metric >> 8;
    const std::int32_t projected_length = result.projected_speed_metric >> 8;
    if (projected_length <= 0) {
        return result;
    }

    const auto rescale = [original_length, projected_length](
                             std::int32_t component) {
        return narrow_impulse(
            static_cast<std::int64_t>(component) * original_length /
            projected_length);
    };
    result.velocity.x = rescale(result.velocity.x);
    result.velocity.y = rescale(result.velocity.y);
    result.velocity.z = rescale(result.velocity.z);
    result.speed_rescaled = true;
    return result;
}

OrientationBasis copy_orientation_basis(
    const std::array<std::int16_t, 9>& object_orientation) noexcept {
    // +0x30f4/+0x30f8/+0x30fc = object +0x2e5c/+0x2e62/+0x2e68
    // +0x3100/+0x3104/+0x3108 = object +0x2e58/+0x2e5e/+0x2e64
    // +0x310c/+0x3110/+0x3114 = object +0x2e5a/+0x2e60/+0x2e66
    return OrientationBasis{
        FixedVec3{object_orientation[2], object_orientation[5], object_orientation[8]},
        FixedVec3{object_orientation[0], object_orientation[3], object_orientation[6]},
        FixedVec3{object_orientation[1], object_orientation[4], object_orientation[7]},
    };
}

FixedVec3 normalize_retail_fixed12(const FixedVec3& value) noexcept {
    // FUN_00465f60 first keeps halving the vector with an arithmetic shift
    // until every component is strictly inside +/-0x6883. This bounds the
    // following 32-bit squared-length products without changing the final
    // normalized direction materially in the retail fixed-point domain.
    FixedVec3 normalized = value;
    while (normalized.x >= 0x6883 || normalized.x <= -0x6883 ||
           normalized.y >= 0x6883 || normalized.y <= -0x6883 ||
           normalized.z >= 0x6883 || normalized.z <= -0x6883) {
        normalized.x >>= 1;
        normalized.y >>= 1;
        normalized.z >>= 1;
    }

    const std::int64_t squared =
        static_cast<std::int64_t>(normalized.x) * normalized.x +
        static_cast<std::int64_t>(normalized.y) * normalized.y +
        static_cast<std::int64_t>(normalized.z) * normalized.z;
    const std::uint64_t length = integer_square_root(
        squared > 0 ? static_cast<std::uint64_t>(squared) : 0);
    if (length == 0) {
        return FixedVec3{0x1000, 0, 0};
    }

    const auto normalize_component = [length](std::int32_t component) {
        // The retail code uses x86 IDIV after SHL 12, which truncates toward
        // zero for the signed component/positive length division.
        return narrow_impulse((static_cast<std::int64_t>(component) << 12) /
                              static_cast<std::int64_t>(length));
    };
    return FixedVec3{normalize_component(normalized.x),
                     normalize_component(normalized.y),
                     normalize_component(normalized.z)};
}

FixedVec3 fixed12_cross(const FixedVec3& left,
                        const FixedVec3& right) noexcept {
    const std::int64_t x = static_cast<std::int64_t>(left.y) * right.z -
                           static_cast<std::int64_t>(left.z) * right.y;
    const std::int64_t y = static_cast<std::int64_t>(left.z) * right.x -
                           static_cast<std::int64_t>(left.x) * right.z;
    const std::int64_t z = static_cast<std::int64_t>(left.x) * right.y -
                           static_cast<std::int64_t>(left.y) * right.x;
    return FixedVec3{static_cast<std::int32_t>(x >> 12),
                     static_cast<std::int32_t>(y >> 12),
                     static_cast<std::int32_t>(z >> 12)};
}

FixedVec3 clamp_cross_scratch_to_int16(const FixedVec3& value) noexcept {
    // FUN_004e2070's ordinary path derives the signed 16-bit range through
    // its x87 power helper, then stores the bounded values in the separate
    // 0x006a3eb0/2/4 scratch words.  0x006a3eb8/c/0 remains the raw vector
    // produced by 0x004e2ff0.
    const auto clamp = [](std::int32_t component) {
        return std::clamp(component, static_cast<std::int32_t>(-0x8000),
                          static_cast<std::int32_t>(0x7fff));
    };
    return FixedVec3{clamp(value.x), clamp(value.y), clamp(value.z)};
}

GroundSurfaceAccelerationResult apply_ground_surface_acceleration(
    const GroundSurfaceAccelerationInput& input) noexcept {
    GroundSurfaceAccelerationResult result;
    result.velocity = input.velocity;
    result.acceleration = input.acceleration;

    // The 0x004972da path calls 0x00490680 before this tail. The part that is
    // local to FUN_00496550 projects velocity off basis +0x3100 in place.
    const std::int32_t surface_dot =
        fixed12_dot(result.velocity, input.basis_3100);
    const FixedVec3 surface_projection =
        fixed12_scalar_multiply(surface_dot, input.basis_3100);
    result.velocity.x -= surface_projection.x;
    result.velocity.y -= surface_projection.y;
    result.velocity.z -= surface_projection.z;
    result.velocity_projected = true;

    const std::int32_t tangent_dot =
        fixed12_dot(result.velocity, input.basis_30f4);
    const FixedVec3 tangent_projection =
        fixed12_scalar_multiply(tangent_dot, input.basis_30f4);

    const auto subtract_acceleration = [&](std::int32_t coefficient) {
        const FixedVec3 term = fixed12_scalar_multiply(coefficient, tangent_projection);
        result.acceleration.x -= fixed_shift8(
            static_cast<std::int64_t>(term.x) * input.frame_delta_fixed);
        result.acceleration.y -= fixed_shift8(
            static_cast<std::int64_t>(term.y) * input.frame_delta_fixed);
        result.acceleration.z -= fixed_shift8(
            static_cast<std::int64_t>(term.z) * input.frame_delta_fixed);
    };

    // 0x0049771b selects coefficient zero when +0x2d94 is set. Otherwise the
    // indexed collision flag selects the base coefficient eight.
    if (!input.speed_gate && input.surface_acceleration_enabled) {
        subtract_acceleration(8);
    }
    if (input.contact_identity == 5 || input.contact_identity == 6) {
        subtract_acceleration(0x78);
    }
    if (input.contact_identity == 3) {
        subtract_acceleration(0xb4);
    }
    if (input.special_mode) {
        subtract_acceleration(input.special_coefficient);
    }
    return result;
}

GroundCollisionResult apply_ground_collision_handoff(
    const GroundCollisionInput& input) noexcept {
    GroundCollisionResult result;
    result.velocity = input.velocity;
    result.acceleration = input.acceleration;
    result.surface_normal = input.surface_normal;
    if (!input.collision_result) {
        return result;
    }

    // This is the 0x00496550 sequence at 0x004975dc-0x00497649. The
    // reference vector is collision/state-owned; 0x00490610 removes its
    // component along the packed surface normal before it is accumulated into
    // acceleration and optionally published.
    result.projected_surface_vector = project_vector_off_surface(
        input.reference_surface_vector, input.surface_normal);
    result.acceleration.x += result.projected_surface_vector.x;
    result.acceleration.y += result.projected_surface_vector.y;
    result.acceleration.z += result.projected_surface_vector.z;
    result.surface_vector_published = !input.transient_state;

    result.velocity_projection = project_velocity_preserving_speed(
        input.velocity, input.surface_normal);
    result.velocity = result.velocity_projection.velocity;
    return result;
}

bool standard_air_landing_accepted(
    const AirCollisionObservation& observation,
    const AirLandingPredicateInput& input) noexcept {
    if (!observation.present()) {
        return false;
    }

    // This is the compound gate immediately before the ordinary landing
    // branch in FUN_00497f40. The signed frame subtraction intentionally
    // remains a signed operation: a recent/negative age satisfies retail's
    // `< 0x29` comparison just as it does in the original.
    const std::int32_t surface_age =
        input.current_frame - input.last_surface_frame;
    const bool landing_gate =
        (observation.material_flags_transient != 0 &&
         (observation.material_flags == 0 ||
          observation.material_flags_contact == 0)) ||
        observation.material_flags_secondary == 0 ||
        input.prephysics_blocked ||
        (!input.jump_held && input.jump_inactive_counter > 0x13) ||
        surface_age < 0x29;
    if (!landing_gate) {
        return false;
    }

    // The ordinary contact branch then requires the transient material flag,
    // except for raw state 3 with the alternate material flag.
    return observation.material_flags_transient != 0 ||
           (observation.material_flags != 0 && input.raw_state == 3);
}

std::int32_t compute_gravity_acceleration(const GravityInput& input) noexcept {
    const std::int64_t gravity = input.gravity_percent;
    std::int64_t result = input.level_variant_nine
                               ? ((gravity * 13000) / 100) * 100 / 100
                               : (gravity * 13000) / 100;
    if (input.raw_state == 2) {
        result = (static_cast<std::int64_t>(500 - input.transient_random) *
                  result * 0x14) /
                 10000;
    }
    if (input.modifier_c) {
        result = (result * 0x96) / 100;
    }
    if (input.modifier_e) {
        result = (result * 0x32) / 100;
    }
    if (input.global_half_modifier) {
        result = (result * 0x32) / 200;
    }
    return narrow_impulse(result);
}

VelocityDampingResult apply_velocity_damping(
    const FixedVec3& velocity, std::int32_t frame_delta_fixed,
    const VelocityDampingRandom& random, bool apply_low_speed_damping) noexcept {
    VelocityDampingResult result;
    result.velocity = velocity;
    result.initial_speed_metric = speed_metric(result.velocity);
    result.cap_threshold = threshold_from_random(random.cap, 500);

    std::int32_t current_speed = result.initial_speed_metric;
    if (result.cap_threshold < current_speed &&
        (static_cast<std::uint32_t>(current_speed) & 0xfffff000u) != 0) {
        const std::int32_t denominator = current_speed >> 12;
        result.cap_component_targets = FixedVec3{
            threshold_from_random(random.cap_x, 500) >> 12,
            threshold_from_random(random.cap_y, 500) >> 12,
            threshold_from_random(random.cap_z, 500) >> 12,
        };
        const auto rescale = [denominator](std::int32_t value,
                                           std::int32_t target) {
            return denominator == 0
                       ? 0
                       : narrow_impulse(static_cast<std::int64_t>(target) * value /
                                        denominator);
        };
        result.velocity.x = rescale(result.velocity.x,
                                     result.cap_component_targets.x);
        result.velocity.y = rescale(result.velocity.y,
                                     result.cap_component_targets.y);
        result.velocity.z = rescale(result.velocity.z,
                                     result.cap_component_targets.z);
        result.cap_applied = true;
        current_speed = speed_metric(result.velocity);
    }

    result.drag_threshold = threshold_from_random(random.threshold, 0x186);
    if (result.drag_threshold < current_speed) {
        const auto subtract_drag = [frame_delta_fixed](std::int32_t value) {
            const std::int32_t scaled = fixed12_scalar_multiply(100, value);
            return fixed_shift8(static_cast<std::int64_t>(scaled) *
                                frame_delta_fixed);
        };
        result.velocity.x -= subtract_drag(result.velocity.x);
        result.velocity.y -= subtract_drag(result.velocity.y);
        result.velocity.z -= subtract_drag(result.velocity.z);
        result.drag_applied = true;
    }

    if (apply_low_speed_damping) {
        const std::int32_t low_speed = speed_metric(result.velocity);
        if (low_speed < 0x10000) {
            result.velocity.x = damp_toward_zero(result.velocity.x, 5);
            result.velocity.y = damp_toward_zero(result.velocity.y, 5);
            result.velocity.z = damp_toward_zero(result.velocity.z, 5);
            result.low_speed_damped = true;
        }
        if (low_speed < 0x2000) {
            result.velocity.x = damp_toward_zero(result.velocity.x, 2);
            result.velocity.y = damp_toward_zero(result.velocity.y, 2);
            result.velocity.z = damp_toward_zero(result.velocity.z, 2);
            const auto zero_small = [](std::int32_t value) {
                return absolute_value(value) < 0x10 ? 0 : value;
            };
            result.velocity.x = zero_small(result.velocity.x);
            result.velocity.y = zero_small(result.velocity.y);
            result.velocity.z = zero_small(result.velocity.z);
            result.low_speed_damped = true;
        }
    }
    result.final_speed_metric = speed_metric(result.velocity);
    return result;
}

void ActionState::update(bool pressed) noexcept {
    ++update_counter;
    press_edge = false;

    if (held == 0 && pressed) {
        // This is the retail inactive -> active edge at 0x0048994d. The
        // cadence byte is reset on the edge before the active counters run.
        edge_latch = 1;
        press_edge = true;
        update_counter = 0;
        held_counter = 0;
        inactive_counter = 0;
    }

    held = pressed ? 1 : 0;
    if (held != 0) {
        inactive_counter = 0;
        ++held_counter;
    } else {
        held_counter = 0;
        ++inactive_counter;
    }
}

bool ActionState::consume_press_edge() noexcept {
    const bool edge = edge_latch != 0;
    edge_latch = 0;
    return edge;
}

PhysicsStateSemantic classify_physics_state(std::int32_t raw_state) noexcept {
    switch (raw_state) {
    case retail::kPhysicsOnGround:
        return PhysicsStateSemantic::OnGround;
    case retail::kPhysicsInAir:
        return PhysicsStateSemantic::InAir;
    case retail::kPhysicsOnInvisible:
        return PhysicsStateSemantic::OnInvisible;
    case retail::kPhysicsInAirStickTo:
        return PhysicsStateSemantic::InAirStickTo;
    case retail::kPhysicsOnRail:
        return PhysicsStateSemantic::OnRail;
    case retail::kPhysicsInWallride:
        return PhysicsStateSemantic::InWallride;
    case retail::kPhysicsInFootplant:
        return PhysicsStateSemantic::InFootplant;
    case retail::kPhysicsStopped:
        return PhysicsStateSemantic::Stopped;
    case retail::kPhysicsInHandplant:
        return PhysicsStateSemantic::InHandplant;
    default:
        return PhysicsStateSemantic::Unknown;
    }
}

bool ground_leave_air_predicate(
    const GroundAirTransitionInput& input) noexcept {
    if (input.slope_metric <= retail::kGroundLeaveAirSlopeThreshold) {
        return false;
    }

    // The retail instruction is a signed 32-bit subtraction. Do the
    // subtraction in unsigned space first so a replay at the frame-counter
    // wrap boundary retains the original two's-complement result without
    // invoking signed-overflow undefined behavior in the host compiler.
    const std::uint32_t age_bits =
        static_cast<std::uint32_t>(input.current_frame) -
        static_cast<std::uint32_t>(input.last_landing_frame);
    const std::int32_t frames_since_landing =
        age_bits <= static_cast<std::uint32_t>(
                        std::numeric_limits<std::int32_t>::max())
            ? static_cast<std::int32_t>(age_bits)
            : static_cast<std::int32_t>(
                  static_cast<std::int64_t>(age_bits) - 0x100000000LL);
    const std::int32_t recent_window = fixed_shift8(
        static_cast<std::int64_t>(input.frame_delta_fixed) *
        retail::kGroundLeaveAirFrameWindowMultiplier);

    // Both comparisons are strict in the retail dispatcher:
    //   +0x3130 > 0x5000 || frame - +0x2d98 < (dt * 6) >> 8.
    return input.recovery_progress > retail::kGroundLeaveAirRecoveryThreshold ||
           frames_since_landing < recent_window;
}

bool ground_to_rail_geometry_hint(
    const GroundRailGeometryInput& input) noexcept {
    // This is the uVar4 selector at 0x00491630-0x00491661, passed as the
    // second argument to FUN_00490ef0. Every comparison is strict; in
    // particular, equality at a threshold does not take the reset branch.
    return input.velocity_y < 0 &&
           absolute_value(input.slope_y) <
               retail::kStartGrindSlopeAbsThreshold &&
           input.up_basis_y > retail::kStartGrindVelocityYThreshold &&
           absolute_value(input.target_normal_y) <
               retail::kStartGrindTargetNormalAbsThreshold &&
           absolute_value(input.rail_basis_y) <
               retail::kStartGrindRailBasisAbsThreshold;
}

OllieImpulseResult compute_ollie_vertical_impulse(const OllieImpulseInput& input) noexcept {
    const auto& random = input.random;
    const bool high_slope = absolute_value(input.slope_metric) >= 0x9c4;
    std::int64_t impulse = 0;

    if (!high_slope) {
        // FUN_0049a280, the abs(+0x3110) < 0x9c4 branch. The five random
        // values are in call order: (2), (0), (0), (0), (0).
        const std::int64_t first_term =
            ((((-900 - random.fifth) * 3000) / 10000) * 0x400) / 10;
        const std::int64_t charge_term =
            ((((random.third + 900) * 3000) / 10000 -
              (random.fourth * 0xce4 + 0x166e30) / 10000) * input.charge * 0x400) /
            10;
        const std::int64_t denominator = 0xf - (random.first + random.second) / 0x14;
        // Valid retail random draws keep this denominator non-zero. Preserve
        // a bounded native result if a synthetic fixture violates that
        // contract instead of invoking undefined behavior.
        const std::int64_t safe_denominator = denominator == 0 ? 1 : denominator;
        impulse = (first_term - charge_term / safe_denominator) * 3;
    } else {
        // The high-slope branch uses random types (2), (0), (2), (2), (2).
        const std::int64_t first_term =
            ((((-0x21c - random.fifth) * 5000) / 10000) * 0x400) / 10;
        const std::int64_t charge_term =
            ((((random.third + 0x21c) * 5000) / 10000 -
              (random.fourth * 0xce4 + 0x166e30) / 10000) * input.charge * 0x400) /
            10;
        const std::int64_t denominator = 0xf - (random.first + random.second) / 0x14;
        const std::int64_t safe_denominator = denominator == 0 ? 1 : denominator;
        impulse = (first_term - charge_term / safe_denominator) * 3;
    }

    OllieImpulseResult result;
    result.high_slope_branch = high_slope;
    result.adjusted_height_delta = input.height_delta_metric;

    // This is the decompiler's recovered condition at 0x0049a280. The
    // source expression is not available; keeping the observed >500 gate
    // explicit makes that uncertainty visible to callers and tests.
    if (input.horizontal_speed_metric > 0x27fff &&
        input.height_delta_metric > 500) {
        const std::int64_t adjusted_delta =
            input.height_delta_metric > 700 ? 700 : input.height_delta_metric;
        result.adjusted_height_delta = narrow_impulse(adjusted_delta);
        impulse = (((adjusted_delta - 300) * impulse) / 200 * 0x78) / 100;
        result.speed_adjustment_applied = true;
    }

    if (input.wallie) {
        impulse -= 0xf000;
    }
    result.delta_y = narrow_impulse(impulse);
    return result;
}

FixedVec3 compute_in_air_position_delta(const FixedVec3& velocity,
                                        const FixedVec3& acceleration,
                                        std::int32_t frame_delta_fixed) noexcept {
    // 0x00497f40 obtains DAT_00568804 as (dt * dt) >> 8, converts the
    // acceleration product through another >> 8, divides that term by two,
    // converts velocity * dt through >> 8, and adds the two vectors.
    const std::int64_t dt = frame_delta_fixed;
    const std::int32_t frame_delta_squared = fixed_shift8(dt * dt);

    const auto acceleration_delta = [frame_delta_squared](std::int32_t value) {
        const std::int32_t product = fixed_shift8(
            static_cast<std::int64_t>(value) * frame_delta_squared);
        // 0x004cac90 is an integer idiv by the local scalar 2, so this is
        // truncation toward zero, unlike the SAR conversion above.
        return static_cast<std::int32_t>(product / 2);
    };
    const auto velocity_delta = [frame_delta_fixed](std::int32_t value) {
        return fixed_shift8(static_cast<std::int64_t>(value) * frame_delta_fixed);
    };

    return FixedVec3{
        velocity_delta(velocity.x) + acceleration_delta(acceleration.x),
        velocity_delta(velocity.y) + acceleration_delta(acceleration.y),
        velocity_delta(velocity.z) + acceleration_delta(acceleration.z),
    };
}

FixedVec3 compute_velocity_delta(const FixedVec3& acceleration,
                                 std::int32_t frame_delta_fixed) noexcept {
    const auto delta = [frame_delta_fixed](std::int32_t value) {
        return fixed_shift8(static_cast<std::int64_t>(value) *
                            frame_delta_fixed);
    };
    return FixedVec3{delta(acceleration.x), delta(acceleration.y),
                     delta(acceleration.z)};
}

InAirActionControlResult apply_in_air_action_control(
    const InAirActionControlInput& input) noexcept {
    InAirActionControlResult result;
    result.acceleration = input.acceleration;
    if (!input.control_enabled) {
        return result;
    }

    const auto scaled_axis = [](const FixedVec3& axis,
                                std::int32_t percent,
                                std::int32_t gravity) {
        // 0x00497f40 first computes (gravity * percent) / 100, then
        // 0x004caa50 multiplies each axis component and 0x004caab0 applies
        // an arithmetic >> 12. This is intentionally not
        // fixed12_scalar_multiply(), whose x87 helper truncates toward zero.
        const std::int32_t scale =
            static_cast<std::int32_t>((static_cast<std::int64_t>(gravity) *
                                        percent) /
                                       100);
        const auto shift12 = [scale](std::int32_t component) {
            return static_cast<std::int32_t>(
                (static_cast<std::int64_t>(component) * scale) >> 12);
        };
        return FixedVec3{shift12(axis.x), shift12(axis.y), shift12(axis.z)};
    };
    const auto add = [&result](const FixedVec3& term) {
        result.acceleration.x = narrow_impulse(
            static_cast<std::int64_t>(result.acceleration.x) + term.x);
        result.acceleration.y = narrow_impulse(
            static_cast<std::int64_t>(result.acceleration.y) + term.y);
        result.acceleration.z = narrow_impulse(
            static_cast<std::int64_t>(result.acceleration.z) + term.z);
    };
    const auto subtract = [&result](const FixedVec3& term) {
        result.acceleration.x = narrow_impulse(
            static_cast<std::int64_t>(result.acceleration.x) - term.x);
        result.acceleration.y = narrow_impulse(
            static_cast<std::int64_t>(result.acceleration.y) - term.y);
        result.acceleration.z = narrow_impulse(
            static_cast<std::int64_t>(result.acceleration.z) - term.z);
    };

    // Preserve the retail action order. Opposing inputs therefore cancel in
    // the same order as the original rather than being collapsed into a
    // single signed axis value.
    if (input.kick_held) {
        add(scaled_axis(input.basis_310c, 0xb4,
                        input.gravity_acceleration));
    }
    if (input.up_held) {
        subtract(scaled_axis(input.basis_30f4, 0x96,
                             input.gravity_acceleration));
    }
    if (input.down_held) {
        // Retail materializes the same basis in normal X/Y/Z order before
        // adding it.  The apparent reversal in the decompiler is only the
        // stack-local assignment order, not a component permutation.
        add(scaled_axis(input.basis_30f4, 0x96,
                        input.gravity_acceleration));
    }
    if (input.spin_left_held) {
        add(scaled_axis(input.basis_3100, 0x96,
                        input.gravity_acceleration));
    }
    if (input.spin_right_held) {
        subtract(scaled_axis(input.basis_3100, 0x96,
                            input.gravity_acceleration));
    }

    // The final instructions in the same retail block project velocity onto
    // basis_3100, divide that vector by 0x20 with integer division, and
    // subtract it from acceleration. FUN_004cac90 uses idiv, so preserve
    // truncation toward zero here rather than using an arithmetic shift.
    const std::int32_t velocity_along_spin_axis =
        fixed12_dot(input.velocity, input.basis_3100);
    const FixedVec3 stabilization_projection = fixed12_scalar_multiply(
        velocity_along_spin_axis, input.basis_3100);
    const FixedVec3 stabilization_term{
        stabilization_projection.x / 0x20,
        stabilization_projection.y / 0x20,
        stabilization_projection.z / 0x20,
    };
    subtract(stabilization_term);
    result.applied = true;
    return result;
}

void PhysicsStateMachine::update_action_mask(std::uint32_t action_mask) noexcept {
    state_.action_mask = action_mask;
    state_.jump.update((action_mask & retail::kJumpActionBit) != 0);
    state_.kick.update((action_mask & retail::kKickActionBit) != 0);
    state_.grind.update((action_mask & retail::kGrindActionBit) != 0);
    state_.grab.update((action_mask & retail::kGrabActionBit) != 0);
    state_.spin_left.update((action_mask & retail::kSpinLeftActionBit) != 0);
    state_.nollie.update((action_mask & retail::kNollieActionBit) != 0);
    state_.spin_right.update((action_mask & retail::kSpinRightActionBit) != 0);
    state_.switch_stance.update((action_mask & retail::kSwitchActionBit) != 0);
    state_.left.update((action_mask & retail::kLeftActionBit) != 0);
    state_.right.update((action_mask & retail::kRightActionBit) != 0);
    state_.up.update((action_mask & retail::kUpActionBit) != 0);
    state_.down.update((action_mask & retail::kDownActionBit) != 0);
}

std::size_t PhysicsStateMachine::update_action_history(
    std::int32_t physics_action, std::int32_t frame) noexcept {
    const std::int32_t event_frame =
        frame >= 0 ? frame : static_cast<std::int32_t>(state_.frame_counter);
    std::size_t changed = 0;

    // This is the call order in FUN_00492190. The first eight calls compare
    // the opaque FUN_00492120 result; the remaining calls read the recovered
    // action-record bank directly. FUN_00491c90's change filter and ring
    // increment are kept in one lambda so every source follows the same
    // writer semantics.
    const auto record = [&](std::size_t action_index, bool pressed) {
        const std::uint8_t value = pressed ? 1 : 0;
        if (state_.action_history_previous[action_index] == value) {
            return;
        }

        state_.action_history_previous[action_index] = value;
        auto& event = state_.action_history_events[
            state_.action_history_write_index];
        event.action_index = static_cast<std::uint8_t>(action_index);
        event.pressed = value;
        event.frame = event_frame;
        ++state_.action_history_write_index;
        if (state_.action_history_write_index ==
            retail::kActionHistoryCapacity) {
            state_.action_history_write_index = 0;
        }
        ++changed;
    };

    for (std::size_t action_index = 1; action_index <= 8; ++action_index) {
        record(action_index, physics_action ==
                                  static_cast<std::int32_t>(action_index));
    }
    record(9, state_.grab.held != 0);
    record(10, state_.grind.held != 0);
    record(11, state_.kick.held != 0);
    record(12, state_.jump.held != 0);
    record(14, state_.nollie.held != 0);
    record(16, state_.switch_stance.held != 0);
    return changed;
}

StateRequest PhysicsStateMachine::request_state(std::int32_t next,
                                                std::uint32_t reason,
                                                std::uint32_t request_callsite) {
    const StateRequest request{
        state_.raw_state,
        next,
        reason,
        state_.raw_state != next,
        retail::kStateWriter,
        request_callsite,
    };

    // The order is observable in the original: the old value is read, the
    // raw state is stored at 0x004902bf, then the old value is copied to +0x30c0.
    state_.raw_state = next;
    state_.phase_state = request.from;
    requests_.push_back(request);
    return request;
}

StateRequest PhysicsStateMachine::enter_collision_transient() {
    return request_state(2, retail::kCollisionTransientEnterReason,
                         retail::kCollisionTransientEnterCallsite);
}

StateRequest PhysicsStateMachine::exit_collision_transient() {
    return request_state(0, retail::kCollisionTransientExitReason,
                         retail::kCollisionTransientExitCallsite);
}

StateRequest PhysicsStateMachine::enter_state4_from_collision() {
    return request_state(4, retail::kState4EnterReason,
                         retail::kState4EnterRequestCallsite);
}

StateRequest PhysicsStateMachine::leave_state4_to_air() {
    return apply_off_ground_transition(OffGroundTransitionInput{}).request;
}

OffGroundTransitionResult PhysicsStateMachine::apply_off_ground_transition(
    const OffGroundTransitionInput& input) {
    OffGroundTransitionResult result;
    result.mode = input.mode;
    result.invocation_callsite = input.invocation_callsite;

    // FUN_004904d0's deterministic stores. The mode-specific calls to
    // animation/speed helpers (0x004de010, 0x0048fb20, 0x004904c0,
    // 0x00491b80, and 0x004be450) remain external side effects; every player
    // field reset and the state request are retained here.
    state_.off_ground.word_2c64 = 0;
    state_.off_ground.control_argument_2f60 = input.control_argument;
    state_.off_ground.word_2c8c = 0;
    ++state_.off_ground.transition_count_302c;
    state_.ollie.landing_contact_auxiliary = 0;
    state_.off_ground.word_29e0 = 0;
    state_.off_ground.word_2bd8 = 0;
    state_.off_ground.word_2ec8 = 0;
    state_.off_ground.word_2e90 = 0;
    state_.off_ground.word_29d4 = 0;
    state_.off_ground.word_29d0 = 0;
    state_.off_ground.word_29f0 = 0;
    state_.off_ground.gravity_percent_29f4 = 100;
    state_.off_ground.word_2dd0 = 0;
    state_.ollie.special_mode = 0;
    state_.ollie.action_context = 0;
    state_.off_ground.word_2f68 = 0;
    state_.prephysics_blocked = 1;
    state_.ollie.latched = 0;
    state_.ollie.charge = 0;
    state_.ollie.recovery_latch = 0;
    state_.off_ground.word_2e94 = 0;

    result.request = request_state(
        retail::kPhysicsInAir, retail::kOffGroundReason,
        retail::kOffGroundStateRequestCallsite);
    result.handled = true;
    result.bookkeeping_reset = true;
    return result;
}

LandingCleanupResult PhysicsStateMachine::apply_landing_cleanup() noexcept {
    LandingCleanupResult result;
    result.handled = true;

    // FUN_004aaf70, called at the top of FUN_004914d0, clears this collision
    // gate immediately when the jump record is no longer held. Its geometry
    // scan and all selected-contact writes remain caller-owned.
    if (state_.jump.held == 0) {
        result.collision_gate_cleared = state_.off_ground.word_2e34 != 0;
        state_.off_ground.word_2e34 = 0;
    }

    // FUN_004914d0 starts with two independent pieces of landing/animation
    // housekeeping.  These stores happen before the marker countdown and are
    // observable even when the helper returns through one of its early exits.
    if (state_.jump.held == 0 && state_.off_ground.word_2e90 != 0) {
        state_.ollie.animation_gate = 0;
        result.animation_gate_cleared = true;
    }
    if (state_.off_ground.word_2ec4 != 0) {
        --state_.off_ground.word_2ec4;
        result.timer_decremented = true;
    }

    // An accepted landing enters with +0x2ec0 == 1. Retail decrements that
    // marker, clears +0x2e90/+0x2e94, and returns before publishing the
    // deeper recovery/animation state. Keep that deterministic boundary
    // separate from the still-external animation calls below it.
    if (state_.ollie.landing_effect_state > 0) {
        --state_.ollie.landing_effect_state;
        state_.off_ground.word_2e90 = 0;
        state_.off_ground.word_2e94 = 0;
        result.landing_marker_consumed = true;
        result.recovery_marker_cleared = true;
        result.returned_early = true;
        return result;
    }

    // These are the exact early-return gates after the landing marker has
    // drained. The deeper recovery/animation dispatch is intentionally not
    // guessed here; it depends on external animation and trick state.
    if (state_.jump.held == 0 || state_.prephysics_blocked != 0 ||
        (state_.raw_state == retail::kPhysicsOnInvisible &&
         state_.velocity.y > 0) ||
        state_.off_ground.word_2e34 != 0) {
        state_.off_ground.word_2e94 = 0;
        result.recovery_marker_cleared = true;
        result.returned_early = true;
        return result;
    }
    if (state_.off_ground.word_2e94 == 0) {
        return result;
    }
    if (state_.off_ground.word_2e90 != 0) {
        state_.off_ground.word_2e94 = 0;
        result.recovery_marker_cleared = true;
        result.returned_early = true;
        return result;
    }

    // +0x2ec0 is also cleared on the fall-through path. The raw-state-0
    // angle cleanup is deterministic; the subsequent recovery animation
    // selection remains outside this native state-machine boundary.
    state_.ollie.landing_effect_state = 0;
    if (state_.raw_state == retail::kPhysicsOnGround) {
        state_.ollie.launch_angle_turns = 0;
        state_.ollie.launch_angle_accumulator = 0;
    }

    // The remaining deterministic part of 0x004914d0 decides whether a
    // recovery condition should invoke FUN_004904d0 again. The first flag is
    // the angle-window test around +0x2c88/+0x2c90/+0x2c8c; the second is the
    // recovery-latch test around +0x2c68/+0x2c6c. The mode/animation values
    // are kept as raw fields because their gameplay names are not established.
    bool angle_window = false;
    if (state_.off_ground.word_2c88 != 0) {
        const std::int32_t angle_remainder =
            state_.off_ground.word_2c8c & 0xfff;
        if (state_.off_ground.word_2c90 < 1) {
            angle_window = angle_remainder > 300;
        } else {
            angle_window = angle_remainder < 0xed4;
        }
    }
    const bool recovery_condition =
        (state_.ollie.recovery_latch != 0 &&
         state_.off_ground.word_2c6c == 0) ||
        angle_window;
    if (recovery_condition) {
        const std::int32_t recovery_offset = state_.off_ground.word_2e2c;
        bool reset_requested = recovery_offset == 0;
        if (!reset_requested && state_.off_ground.byte_100 == 1) {
            reset_requested =
                state_.off_ground.word_f4 <
                static_cast<std::int32_t>(state_.off_ground.byte_101) -
                    recovery_offset;
        } else if (!reset_requested) {
            reset_requested =
                static_cast<std::int32_t>(state_.off_ground.byte_101) +
                    recovery_offset < state_.off_ground.word_f4;
        }
        if (reset_requested) {
            result.off_ground = apply_off_ground_transition(
                OffGroundTransitionInput{
                    static_cast<std::int32_t>(state_.off_ground.word_29f2),
                    0, retail::kLandingRecoveryOffGroundCallsite});
            state_.off_ground.word_29f2 = 0;
            ++state_.off_ground.counter_3034;
            result.recovery_reset_requested = true;
            result.returned_early = true;
            return result;
        }
    }

    if (state_.ollie.recovery_latch != 0) {
        state_.off_ground.word_2c70 = 1;
    }

    // FUN_004925e0 recognizes recent action-history/trick sequences. It is
    // intentionally not reimplemented here, but the native result exposes
    // that this branch was reached and whether the follow-up FUN_00490ef0
    // dispatch is required by +0x3064. The exact signed geometry hint is
    // deterministic and is retained for a future caller-owned recognizer.
    result.trick_scan_requested = true;
    bool trick_direction_hint = false;
    if (state_.velocity.y < 0 &&
        absolute_value(state_.basis_310c.y) < 3000 &&
        state_.basis_30f4.y > 0x800 &&
        absolute_value(state_.off_ground.word_82) < 0x5dc &&
        absolute_value(state_.basis_3100.y) < 0x800) {
        trick_direction_hint = true;
    }
    result.trick_direction_hint = trick_direction_hint;
    result.trick_dispatch_requested = state_.off_ground.word_3064 != 0;
    return result;
}

void PhysicsStateMachine::begin_dispatcher_phase() noexcept {
    state_.phase_state = state_.raw_state;
}

bool PhysicsStateMachine::reset_non_air_acceleration() noexcept {
    if (state_.raw_state == 1) {
        return false;
    }
    state_.acceleration = FixedVec3{};
    return true;
}

void PhysicsStateMachine::begin_position_history() noexcept {
    ++state_.frame_counter;
    state_.older_position = state_.position_history;
    state_.position_history = state_.position;
}

bool PhysicsStateMachine::update_collision_recovery_window(
    std::int32_t current_frame) noexcept {
    // This is the frame-start condition at 0x0049e680. The signed byte
    // comparisons are intentionally expressed on the native int8_t fields,
    // matching the retail char loads at +0x31a1/+0x31a2.
    const bool cancel_window =
        (state_.up.held == 0 && state_.heading_deadband > -0x29) ||
        state_.left.held != 0 || state_.right.held != 0 ||
        absolute_value(static_cast<std::int32_t>(state_.heading_input)) > 0x31;
    if (cancel_window) {
        state_.collision_recovery_frame = 0;
        return false;
    }
    if (state_.collision_recovery_frame == 0) {
        state_.collision_recovery_frame = current_frame;
    }
    return true;
}

std::int32_t PhysicsStateMachine::initialize_gravity_acceleration(
    const GravityInput& input) noexcept {
    state_.gravity_acceleration = compute_gravity_acceleration(input);
    return state_.gravity_acceleration;
}

BlockedPhysicsResult PhysicsStateMachine::apply_blocked_physics_reset(
    const BlockedPhysicsInput& input) noexcept {
    BlockedPhysicsResult result;
    if (state_.prephysics_blocked == 0) {
        return result;
    }
    result.handled = true;

    // 0x0049d8a0 clears only the horizontal acceleration components here;
    // the main frame's earlier non-air reset owns the ordinary full clear.
    state_.acceleration.x = 0;
    state_.acceleration.z = 0;
    result.acceleration_xz_cleared = true;

    state_.ollie.latched = 0;
    state_.ollie.in_progress = 0;
    result.launch_bookkeeping_cleared = true;

    if (input.clamp_negative_vertical && state_.velocity.y < 0) {
        state_.velocity.y = 0;
        result.vertical_velocity_clamped = true;
    }

    if (input.velocity_decay_divisor != 0) {
        const auto decay = [&](std::int32_t value) {
            // Retail performs the component/divisor idiv first, then the
            // product with dt is converted using SAR 8.
            const std::int32_t quotient =
                value / input.velocity_decay_divisor;
            return fixed_shift8(static_cast<std::int64_t>(quotient) *
                                input.frame_delta_fixed);
        };
        state_.velocity.x -= decay(state_.velocity.x);
        state_.velocity.y -= decay(state_.velocity.y);
        state_.velocity.z -= decay(state_.velocity.z);
        result.velocity_decayed = true;
    }
    return result;
}

DispatchResult PhysicsStateMachine::step_frame(
    std::uint32_t action_mask, const PhysicsFrameCallbacks& callbacks, void* user) {
    update_action_mask(action_mask);

    if (callbacks.gravity_initialization != nullptr) {
        callbacks.gravity_initialization(*this, user);
    }
    if (callbacks.movement_action_step != nullptr) {
        callbacks.movement_action_step(*this, user);
    }
    reset_non_air_acceleration();
    if (callbacks.prephysics != nullptr) {
        callbacks.prephysics(*this, user);
    }
    if (callbacks.action_history != nullptr) {
        callbacks.action_history(*this, user);
    }
    if (callbacks.ground_preparation != nullptr) {
        callbacks.ground_preparation(*this, user);
    }

    // 0x0049e680 publishes the phase word and rotates position history before
    // entering the collision-start check at 0x00490730.
    begin_dispatcher_phase();
    begin_position_history();
    update_collision_recovery_window(
        static_cast<std::int32_t>(state_.frame_counter));

    if (callbacks.collision_preparation != nullptr) {
        callbacks.collision_preparation(*this, user);
    }

    // Keep the raw-state-3 timeout deferred until the common in-air work has
    // run. Retail's dispatcher performs that check after its direct call to
    // 0x00497f40; dispatch_impl() therefore exposes the same split boundary
    // to this callback-driven native frame.
    DispatchResult result = dispatch_impl(
        callbacks.dispatcher, user, false);
    if (result.raw_state == retail::kPhysicsOnGround &&
        state_.raw_state == retail::kPhysicsOnGround &&
        callbacks.ground_leave_air_input != nullptr) {
        GroundAirTransitionInput input{};
        if (callbacks.ground_leave_air_input(*this, input, user)) {
            result.ground_leave_air = try_ground_to_air(input);
        }
    }
    if (result.kind == DispatchKind::State6PreAir &&
        callbacks.state6_preair_setup != nullptr) {
        // Case 6's first callee requests raw state 1 before the switch falls
        // through into 0x00497f40. Keep this between dispatch observation and
        // the common in-air action/motion block so the callback sees raw 6 and
        // subsequent air callbacks see the requested raw 1.
        callbacks.state6_preair_setup(*this, user);
    }
    if (result.kind == DispatchKind::InAir ||
        result.kind == DispatchKind::State6PreAir) {
        if (callbacks.air_preparation != nullptr) {
            // The common in-air handler's 0x00497df0 preparation precedes its
            // action-record acceleration block. Keep basis publication ahead
            // of the native action-control calculation.
            callbacks.air_preparation(*this, user);
        }
        apply_in_air_action_control();
        apply_in_air_jump_hold_effect();
        if (callbacks.air_motion != nullptr) {
            callbacks.air_motion(*this, user);
        }
        if (callbacks.upright_correction != nullptr) {
            callbacks.upright_correction(*this, user);
        }
    }
    bool air_contact_accepted = false;
    if (callbacks.air_contact != nullptr &&
        uses_common_air_handler(result.raw_state) &&
        uses_common_air_handler(state_.raw_state)) {
        FixedVec3 contact_position = state_.position;
        if (callbacks.air_contact(*this, contact_position, user)) {
            air_contact_accepted = accept_air_contact(
                true, contact_position, callbacks.position_commit, user);
        }
    }
    if (result.kind == DispatchKind::InAir ||
        result.kind == DispatchKind::State6PreAir) {
        // The common air handler adds +0x2da8/+0x2dac/+0x2db0 at 0x004992f0
        // after its motion and contact branches.  Gravity therefore affects
        // the next frame's integration, not the displacement just computed.
        // An accepted contact jumps over this fallthrough in the retail
        // handler, so the landing frame does not receive the gravity add.
        if (!air_contact_accepted) {
            apply_in_air_gravity();
        }
    }

    finalize_dispatch_post_handler(result);

    if (callbacks.blocked_physics_reset != nullptr) {
        callbacks.blocked_physics_reset(*this, user);
    }

    // The outer frame calls 0x004914d0 for every state except handplant,
    // after the dispatch/position work and before final velocity integration.
    // Accepted common-air contact has already invoked this same helper at its
    // in-air landing branch, so the normal landing marker is then a no-op.
    if (state_.raw_state != retail::kPhysicsInHandplant &&
        callbacks.landing_collision_preparation != nullptr) {
        callbacks.landing_collision_preparation(*this, user);
    }
    if (callbacks.landing_cleanup != nullptr &&
        state_.raw_state != retail::kPhysicsInHandplant) {
        callbacks.landing_cleanup(*this, user);
    }
    if (callbacks.ground_to_rail_input != nullptr &&
        state_.raw_state != retail::kPhysicsInHandplant) {
        GroundRailTransitionInput input{};
        if (callbacks.ground_to_rail_input(*this, input, user)) {
            result.ground_to_rail = try_ground_to_rail(input);
        }
    }

    if (callbacks.velocity_integration != nullptr) {
        callbacks.velocity_integration(*this, user);
    }

    if (callbacks.postphysics != nullptr) {
        callbacks.postphysics(*this, user);
    }

    // The outer frame maintains +0x2de4 after the physics work: it is zero
    // outside raw states 1..3 and is initialized to the current frame the
    // first time the player remains in that range.  This timestamp is what
    // the release-side ollie path later uses for its twenty-frame stale-latch
    // test.
    if (state_.raw_state < 1 || state_.raw_state > 3) {
        state_.ollie.latch_timestamp = 0;
    } else if (state_.ollie.latch_timestamp == 0) {
        state_.ollie.latch_timestamp = static_cast<std::int32_t>(state_.frame_counter);
    }
    return result;
}

DispatchResult PhysicsStateMachine::dispatch(DispatchCallback callback, void* user) {
    return dispatch_impl(callback, user, true);
}

DispatchResult PhysicsStateMachine::dispatch_impl(
    DispatchCallback callback, void* user, bool finalize_post_handler) {
    DispatchResult result;
    result.raw_state = state_.raw_state;
    result.semantic_state = classify_physics_state(state_.raw_state);

    auto add_handler = [&result](std::uint32_t pc) {
        result.handler_pcs[result.handler_count++] = pc;
    };

    switch (state_.raw_state) {
    case 0:
        result.kind = DispatchKind::Ground;
        state_.auxiliary = 0;
        state_.off_ground.word_2ec8 = 0;
        add_handler(0x0049dad0);
        add_handler(0x00496550);
        add_handler(0x00495cc0);
        add_handler(0x0049d9c0);
        break;
    case 1:
        result.kind = DispatchKind::InAir;
        add_handler(retail::kInAirHandler);
        break;
    case 2:
        result.kind = DispatchKind::CollisionTransient;
        state_.auxiliary = 1;
        add_handler(0x00496550);
        break;
    case 3:
        result.kind = DispatchKind::InAir;
        add_handler(retail::kInAirHandler);
        break;
    case 4:
        result.kind = DispatchKind::State4;
        state_.auxiliary = 0;
        add_handler(retail::kRailPhysicsHandler);
        break;
    case 5:
        result.kind = DispatchKind::State5;
        state_.auxiliary = 0;
        state_.off_ground.word_2ec8 = 0;
        add_handler(0x00499710);
        break;
    case 6:
        result.kind = DispatchKind::State6PreAir;
        add_handler(retail::kState6PreAirSetup);
        // The retail switch falls through from case 6 to case 1, so the
        // preparation helper is followed by the common in-air handler.
        add_handler(retail::kInAirHandler);
        break;
    case 7:
        result.kind = DispatchKind::State7Ground;
        // Case 7 shares the grounded helper sequence with case 0 and adds a
        // position copy between 0x00495cc0 and 0x0049d9c0.
        add_handler(0x0049dad0);
        add_handler(0x00496550);
        add_handler(0x00495cc0);
        state_.position = state_.position_history;
        add_handler(0x0049d9c0);
        break;
    case 8:
        result.kind = DispatchKind::State8;
        state_.auxiliary = 0;
        add_handler(0x004995d0);
        break;
    default:
        result.kind = DispatchKind::Unknown;
        break;
    }

    if (callback != nullptr) {
        callback(state_, result, user);
    }

    if (finalize_post_handler) {
        finalize_dispatch_post_handler(result);
    }
    return result;
}

void PhysicsStateMachine::finalize_dispatch_post_handler(
    const DispatchResult& result) noexcept {
    // Dispatcher case 3 shares the in-air handler with case 1, then promotes
    // an alternate launch to raw state 1 after twenty-five frames. The
    // timeout is a post-handler request, so do not fire it if the handler has
    // already landed or otherwise changed the raw state.
    const std::int64_t alternate_age =
        static_cast<std::int64_t>(state_.frame_counter) -
        state_.ollie.alternate_state_frame;
    if (result.raw_state == 3 && state_.raw_state == 3 &&
        alternate_age > 0x19) {
        request_state(1, retail::kAlternateStateTimeoutReason,
                      retail::kAlternateStateTimeoutRequestCallsite);
    }
}

GroundAirTransitionResult PhysicsStateMachine::try_ground_to_air(
    const GroundAirTransitionInput& input) {
    GroundAirTransitionResult result;
    if (state_.raw_state != retail::kPhysicsOnGround) {
        return result;
    }
    result.predicate_evaluated = true;
    if (!ground_leave_air_predicate(input)) {
        return result;
    }

    result.eligible = true;
    result.request = request_state(
        retail::kPhysicsInAir, retail::kGroundLeaveAirReason,
        retail::kGroundLeaveAirRequestCallsite);
    result.transitioned = result.request.changed;

    // This is the store immediately following the state request in the
    // case-0 tail at 0x0049ddd9. It prevents a stale downward velocity from
    // surviving the leave-ground transition.
    if (state_.velocity.y < 0) {
        state_.velocity.y = 0;
        result.vertical_velocity_clamped = true;
    }
    result.off_ground = apply_off_ground_transition(
        OffGroundTransitionInput{0x14, 0, retail::kGroundLeaveAirOffGroundCallsite});
    state_.off_ground.marker_3204 = retail::kGroundLeaveAirMarker;
    return result;
}

GroundRailTransitionResult PhysicsStateMachine::try_ground_to_rail(
    const GroundRailTransitionInput& input) {
    GroundRailTransitionResult result;
    result.predicate_evaluated = true;
    if (!input.trick_dispatch_requested) {
        // FUN_004914d0 returns at 0x0049167b when +0x3064 is zero. No
        // StartGrind field is touched on this path.
        return result;
    }

    result.eligible = true;
    const auto record_stage = [&result](GroundRailEntryStage stage) {
        result.stages[result.stage_count++] = stage;
    };

    // 0x00490ef0 first negates +0x2eb4 in place, uses that direction for the
    // complete entry, and restores the original vector at 0x00491493.
    FixedVec3 direction{
        wrap_negate_i32(state_.rail.direction_2eb4.x),
        wrap_negate_i32(state_.rail.direction_2eb4.y),
        wrap_negate_i32(state_.rail.direction_2eb4.z),
    };
    state_.rail.direction_2eb4 = direction;
    record_stage(GroundRailEntryStage::DirectionNegated);

    result.geometry_hint = ground_to_rail_geometry_hint(
        GroundRailGeometryInput{
            state_.velocity.y,
            state_.basis_310c.y,
            state_.basis_30f4.y,
            state_.off_ground.word_82,
            state_.basis_3100.y,
        });
    if (result.geometry_hint) {
        state_.rail.vector_gate_2c78 = 0;
        state_.rail.vector_gate_2c7c = 0;
        state_.rail.pending_motion_2c94_2cb4.fill(0);
        state_.ollie.latched = 0;
        result.pending_vectors_cleared = true;
        record_stage(GroundRailEntryStage::GeometryHintResets);
    }

    // The entry relocates the live position from +0x2ea8 and applies the
    // fixed -0x19000 vertical offset before publishing both copies used by
    // the following rail frame.
    state_.rail.animation_marker_2eec = 0;
    state_.position = state_.rail.rail_position_2ea8;
    state_.position.y = wrap_subtract_i32(
        state_.position.y, retail::kStartGrindPositionYOffset);
    state_.off_ground.word_2e94 = 0;
    state_.off_ground.word_2e90 = 1;
    state_.rail.relocated_position_2ee0 = state_.position;
    state_.rail.entry_position_2ecc = state_.position;
    result.position_relocated = true;
    record_stage(GroundRailEntryStage::PositionRelocated);

    // +0x2efc changes by one rail-distance quantum, while +0x2ef4 is reset
    // on a node change and then receives the two type-7 random draws. Keep
    // each retail 32-bit multiply visible so replay values wrap like imul.
    if (state_.rail.node_current_2e9c == state_.rail.node_previous_2e98) {
        state_.rail.speed_2efc = wrap_add_i32(
            state_.rail.speed_2efc, 0x28000);
    } else {
        state_.rail.speed_2efc = wrap_subtract_i32(
            state_.rail.speed_2efc, 0x28000);
        if (state_.rail.speed_2efc < 0) {
            state_.rail.speed_2efc = 0;
        }
        state_.rail.node_previous_2e98 = state_.rail.node_current_2e9c;
        state_.rail.speed_delta_2ef4 = 0;
    }

    const std::int32_t random_numerator =
        wrap_add_i32(wrap_multiply_i32(input.random_speed_primary, -0x708),
                     300000) /
        10000;
    const std::int32_t random_scaled =
        wrap_multiply_i32(random_numerator, state_.rail.speed_delta_2ef4) /
        100;
    const std::int32_t speed_factor =
        (state_.rail.speed_2efc >> 12) + 0x3c;
    const std::int32_t random_speed_product =
        wrap_multiply_i32(random_scaled, speed_factor);
    const std::int32_t random_denominator =
        wrap_multiply_i32(wrap_add_i32(input.random_speed_secondary, 0x15e),
                          2000) /
        10000;
    if (random_denominator != 0) {
        state_.rail.speed_delta_2ef4 = random_speed_product / random_denominator;
        result.speed_formula_applied = true;
    } else {
        // The retail IDIV would fault for a zero denominator. Native replay
        // keeps the transition inspectable while leaving the field unchanged.
        result.speed_formula_applied = false;
    }
    state_.ollie.recovery_latch = 1;
    state_.rail.sequence_count_2e88 =
        wrap_add_i32(state_.rail.sequence_count_2e88, 1);
    state_.rail.update_count_3070 =
        wrap_add_i32(state_.rail.update_count_3070, 1);
    state_.rail.update_count_304c =
        wrap_add_i32(state_.rail.update_count_304c, 1);
    record_stage(GroundRailEntryStage::RailKinematicsUpdated);

    // Vector3_Subtract(&local_c, +0x2ea8, +0x2e18) computes the relative
    // vector from the already-negated direction to the rail start. Its X/Z
    // perpendicular is the third dot-product basis used by the turn flags.
    state_.rail.relative_vector_2e18 = FixedVec3{
        wrap_subtract_i32(direction.x, state_.rail.rail_position_2ea8.x),
        wrap_subtract_i32(direction.y, state_.rail.rail_position_2ea8.y),
        wrap_subtract_i32(direction.z, state_.rail.rail_position_2ea8.z),
    };
    const FixedVec3 perpendicular{
        state_.rail.relative_vector_2e18.z,
        0,
        wrap_negate_i32(state_.rail.relative_vector_2e18.x),
    };
    const std::int32_t tangent_dot = fixed12_dot(
        state_.rail.direction_2eb4, state_.basis_30f4);
    const std::int32_t rail_dot = fixed12_dot(
        state_.rail.direction_2eb4, state_.basis_3100);
    const std::int32_t perpendicular_dot = fixed12_dot(
        state_.rail.direction_2eb4, perpendicular);

    if (state_.off_ground.word_2ec8 != state_.rail.node_current_2e9c) {
        state_.rail.turn_flag_2bf0 = 0;
        state_.rail.turn_flag_2bf4 = 0;
        if (perpendicular_dot > 0) {
            state_.rail.turn_flag_2bf0 = 1;
        }
        state_.rail.stance_flag_2bf8 =
            (state_.object_flags_d8 & 2) != 0 ? 1 : 0;
        if (state_.rail.turn_flag_2bf0 != state_.rail.stance_flag_2bf8) {
            state_.rail.turn_flag_2bf4 = 1;
        }
    }
    state_.rail.normal_direction_flag_2bec = 0;
    state_.rail.normal_flag_2be8 = 0;
    if (absolute_value(rail_dot) < retail::kStartGrindNormalDotThreshold) {
        state_.rail.normal_flag_2be8 = 1;
        if (tangent_dot > 0) {
            state_.rail.normal_direction_flag_2bec = 1;
        }
    } else if (rail_dot < 0) {
        state_.rail.normal_direction_flag_2bec = 1;
    }
    state_.off_ground.word_2ec8 = state_.rail.node_current_2e9c;
    record_stage(GroundRailEntryStage::DirectionFlagsUpdated);

    state_.rail.derived_pattern_2c04 = 0;
    result.derived_pattern = input.derived_pattern;
    if (state_.rail.pattern_source_2bfc != 0) {
        const std::uint32_t pattern =
            ((1u - static_cast<std::uint32_t>(state_.rail.pattern_state_2c00)) ^
             static_cast<std::uint32_t>(state_.rail.pattern_source_2bfc)) >>
            1;
        state_.rail.derived_pattern_2c04 = static_cast<std::int32_t>(
            pattern ^ static_cast<std::uint32_t>(state_.rail.turn_flag_2bf0));
    }
    state_.rail.mode_2ed8 = 1;
    state_.rail.effect_2c0c = 0;
    // TrickInput_DerivePattern, TrickScript_Execute, and the rail-type sound
    // switch all run here in retail. Their producers/consumers are external,
    // but this stage preserves their position before the state writer.
    record_stage(GroundRailEntryStage::TrickServicesPrepared);

    result.request = request_state(
        retail::kPhysicsOnRail, retail::kState4EnterReason,
        retail::kState4EnterRequestCallsite);
    result.transitioned = result.request.changed;
    record_stage(GroundRailEntryStage::StateRequested);

    // After the request, velocity is projected onto the negated rail
    // direction. The random kick is ordinary integer vector multiplication
    // followed by component-wise division by ten, not Q12 multiplication.
    const std::int32_t direction_velocity_dot = fixed12_dot(
        state_.rail.direction_2eb4, state_.velocity);
    state_.velocity = FixedVec3{
        fixed12_scalar_multiply(state_.rail.direction_2eb4.x,
                                direction_velocity_dot),
        fixed12_scalar_multiply(state_.rail.direction_2eb4.y,
                                direction_velocity_dot),
        fixed12_scalar_multiply(state_.rail.direction_2eb4.z,
                                direction_velocity_dot),
    };
    const std::int32_t random_scalar =
        wrap_multiply_i32(
            wrap_add_i32(input.random_velocity_adjustment, 100), 5000) /
        10000;
    const auto random_component = [random_scalar](std::int32_t component) {
        return wrap_multiply_i32(component, random_scalar) / 10;
    };
    state_.velocity.x = wrap_add_i32(
        state_.velocity.x, random_component(state_.rail.direction_2eb4.x));
    state_.velocity.y = wrap_add_i32(
        state_.velocity.y, random_component(state_.rail.direction_2eb4.y));
    state_.velocity.z = wrap_add_i32(
        state_.velocity.z, random_component(state_.rail.direction_2eb4.z));
    result.velocity_projected = true;
    record_stage(GroundRailEntryStage::VelocityShaped);

    state_.rail.direction_2eb4 = FixedVec3{
        wrap_negate_i32(state_.rail.direction_2eb4.x),
        wrap_negate_i32(state_.rail.direction_2eb4.y),
        wrap_negate_i32(state_.rail.direction_2eb4.z),
    };
    result.direction_restored = true;
    record_stage(GroundRailEntryStage::DirectionRestored);
    return result;
}

StateRequest PhysicsStateMachine::begin_ollie(LaunchPath path,
                                              LaunchCallback callback,
                                              void* user) {
    const std::int32_t launch_state = path == LaunchPath::Ordinary ? 1 : 3;
    const std::uint32_t reason = path == LaunchPath::Ordinary
                                     ? retail::kOrdinaryLaunchReason
                                     : retail::kAlternateLaunchReason;

    if (callback != nullptr) {
        callback(state_, launch_state, reason, user);
    }
    const std::uint32_t request_callsite =
        path == LaunchPath::Ordinary
            ? retail::kOrdinaryLaunchRequestCallsite
            : retail::kAlternateLaunchRequestCallsite;
    const StateRequest request =
        request_state(launch_state, reason, request_callsite);
    if (path == LaunchPath::Alternate) {
        state_.ollie.alternate_state_frame = static_cast<std::int32_t>(state_.frame_counter);
    }
    return request;
}

OllieImpulseResult PhysicsStateMachine::apply_ollie_impulse(
    const OllieImpulseInput& input) {
    const OllieImpulseResult result = compute_ollie_vertical_impulse(input);
    state_.ollie.speed_metric = input.horizontal_speed_metric;
    state_.ollie.wallie = input.wallie ? 1 : 0;
    state_.ollie.launch_charge = input.charge;
    state_.ollie.latched = 0;
    state_.ollie.in_progress = 1;
    state_.ollie.mode_latched = state_.ollie.mode;
    state_.ollie.launch_auxiliary = 0;
    state_.ollie.launch_count += 1;
    if (input.charge < 0xf -
                         (input.early_release_random.first +
                          input.early_release_random.second) /
                             0x14) {
        state_.ollie.early_release_count += 1;
    }
    state_.ollie.charge = 0;
    state_.ollie.launch_frame = static_cast<std::int32_t>(state_.frame_counter);
    state_.ollie.launch_angle_accumulator = 0;
    state_.ollie.launch_angle_turns = 0;
    state_.velocity.y += result.delta_y;
    state_.ollie.pending = 0;
    return result;
}

OllieChargeResult PhysicsStateMachine::advance_ollie_charge(
    bool animation_eligible, std::int32_t first_random, std::int32_t second_random,
    bool force_cap) noexcept {
    OllieChargeResult result;
    if (!animation_eligible || state_.kick.held == 0) {
        result.charge = state_.ollie.charge;
        result.cap = 0xf - (first_random + second_random) / 0x14;
        return result;
    }

    ++state_.ollie.charge;
    result.cap = 0xf - (first_random + second_random) / 0x14;
    if ((result.cap < state_.ollie.charge) || force_cap) {
        state_.ollie.charge = result.cap;
        result.capped = true;
    }

    // These are the state tests at 0x0049a280. +0x2e30 is an additional
    // animation/board gate and is intentionally represented by
    // `animation_eligible` at this native boundary.
    if (state_.raw_state == 0 || state_.raw_state == 7 ||
        state_.raw_state == 4 || state_.raw_state == 5) {
        state_.ollie.latched = 1;
        result.latched = true;
    }
    state_.ollie.pending = 1;
    result.pending = true;
    result.charge = state_.ollie.charge;
    return result;
}

OlliePrePhysicsResult PhysicsStateMachine::run_ollie_prephysics(
    const OlliePrePhysicsInput& input, LaunchCallback callback, void* user) {
    OlliePrePhysicsResult result;
    state_.prephysics_blocked = input.prephysics_blocked ? 1 : 0;
    result.charge = state_.ollie.charge;
    result.cap = 0xf -
                 (input.charge_cap_random.first +
                  input.charge_cap_random.second) /
                     0x14;

    // This is the first instruction-level gate in 0x0049a280. A blocked
    // frame returns before either the held or release-side KICK path runs.
    if (state_.prephysics_blocked != 0) {
        return result;
    }

    // The retail routine has a ground re-entry cleanup before it reads the
    // KICK record. After a launch, the request helper leaves the old raw
    // state in +0x30c0; on the next raw-0/raw-7 prephysics pass that nonzero
    // phase gates the cleanup. These are the deterministic stores at
    // 0x0049a4b3, 0x0049a4b9, 0x0049a4bf, and 0x0049a587. Animation, sound,
    // and combo bookkeeping called from the same block remain outside this
    // native boundary.
    if (state_.ollie.in_progress != 0 && state_.phase_state != 0 &&
        (state_.raw_state == 0 || state_.raw_state == 7)) {
        if (state_.ollie.special_mode == 0) {
            state_.ollie.action_context = 0;
        }
        state_.ollie.in_progress = 0;
        state_.ollie.mode_latched = 0;
        state_.ollie.recovery_latch = 0;
        result.recovery_cleared = true;
    }

    const std::int32_t current_frame =
        input.current_frame >= 0 ? input.current_frame
                                 : static_cast<std::int32_t>(state_.frame_counter);

    if (state_.kick.held != 0) {
        ++state_.ollie.charge;
        result.event = OlliePrePhysicsEvent::Charging;

        if (result.cap < state_.ollie.charge || input.force_cap) {
            // The retail cap is recalculated from a fresh random pair at
            // 0x0049a5f6. The explicit secondary stream keeps that ordering
            // observable in deterministic native replays.
            result.cap = 0xf -
                         (input.charge_cap_refresh_random.first +
                          input.charge_cap_refresh_random.second) /
                             0x14;
            state_.ollie.charge = result.cap;
            result.capped = true;
        }

        const std::int32_t raw_state = state_.raw_state;
        if ((raw_state == 0 || raw_state == 7) &&
            state_.ollie.animation_gate == 0) {
            state_.ollie.latched = 1;
            result.latch_set = true;
            if (state_.ollie.pending == 0 &&
                absolute_value(state_.movement_target_x) < 0xa000) {
                state_.movement_target_x = 0;
            }
        } else if ((raw_state == 4 || raw_state == 5) &&
                   state_.ollie.animation_gate == 0) {
            state_.ollie.latched = 1;
            result.latch_set = true;
        }

        if (state_.ollie.animation_gate != 0) {
            result.charge = state_.ollie.charge;
            return result;
        }

        state_.ollie.pending = 1;
        result.pending_set = true;
        result.charge = state_.ollie.charge;
        return result;
    }

    // Release-side cleanup runs for KICK byte +0 == 0. It clears the pending
    // bit before deciding whether the latch is stale, cancelled, or consumed.
    state_.ollie.animation_gate = 0;
    state_.ollie.pending = 0;
    if (state_.ollie.latch_timestamp > 0 &&
        current_frame - state_.ollie.latch_timestamp > 0x14) {
        state_.ollie.latched = 0;
        result.stale_latch_cleared = true;
        result.event = OlliePrePhysicsEvent::StaleLatchCleared;
    }

    if (state_.ollie.latched == 0) {
        // 0x0049af1a is the common release-side charge reset.
        state_.ollie.charge = 0;
        result.charge = 0;
        return result;
    }

    if (input.global_release_mode == 1 && state_.raw_state != 4 &&
        state_.raw_state != 5 && state_.ollie.special_mode == 0) {
        state_.ollie.latched = 0;
        state_.ollie.charge = 0;
        result.event = OlliePrePhysicsEvent::Cancelled;
        result.charge = 0;
        return result;
    }

    // The launch consumes +0x2de0 at 0x0049a751. The charge snapshot and
    // impulse happen before the request helper, matching the observed frame
    // where the raw writer follows 0x0049a751 in the same prephysics pass.
    OllieImpulseInput impulse = input.impulse;
    impulse.wallie = state_.raw_state == 5;
    if (impulse.wallie) {
        state_.ollie.charge =
            0xf - (input.wallie_charge_random.first +
                   input.wallie_charge_random.second) /
                      0x14;
    }
    impulse.charge = state_.ollie.charge;
    impulse.early_release_random = input.early_release_random;
    const OllieImpulseResult impulse_result = apply_ollie_impulse(impulse);
    (void)impulse_result;
    state_.ollie.launch_frame = current_frame;
    result.launch_consumed = true;
    result.event = OlliePrePhysicsEvent::Launched;

    // Raw state 2 is the one path that consumes the impulse without issuing
    // the ordinary/alternate air request. Retail selects ordinary raw 1 when
    // the latched mode is zero or the current raw state is nonzero. Only a
    // zero-state launch with a nonzero latched mode requests alternate raw 3.
    if (state_.raw_state != 2) {
        const bool ordinary = state_.ollie.mode_latched == 0 || state_.raw_state != 0;
        const LaunchPath path = ordinary ? LaunchPath::Ordinary : LaunchPath::Alternate;
        const StateRequest request = begin_ollie(path, callback, user);
        result.state_requested = true;
        result.request = request;
        if (path == LaunchPath::Alternate) {
            state_.ollie.alternate_state_frame = current_frame;
        }
    }

    // The special-mode release tail clears these action-context fields after
    // the request. Their animation/event consumers are intentionally not
    // reconstructed here, but the state writes are deterministic.
    if (state_.ollie.special_mode != 0) {
        state_.ollie.recovery_latch = 0;
        state_.ollie.action_context = 0;
    }

    result.charge = state_.ollie.charge;
    return result;
}

State6PreAirResult PhysicsStateMachine::run_state6_preair_setup(
    const State6PreAirInput& input) {
    State6PreAirResult result;
    if (state_.raw_state != 6) {
        return result;
    }

    // 0x004993f0 stores 0xf - (rand(2) + rand(0)) / 0x14 before issuing
    // the state request. Retail integer division truncates toward zero.
    state_.ollie.charge = 0xf -
                          (input.charge_random_first +
                           input.charge_random_second) /
                              0x14;
    result.charge = state_.ollie.charge;
    result.request = request_state(1, retail::kState6PreAirReason,
                                   retail::kState6PreAirRequestCallsite);
    state_.ollie.in_progress = 1;

    // 0x004993f0 uses 0x004ca8f0 to measure the preexisting velocity after
    // an arithmetic shift by 12, negates the published +0x30f4 basis, and
    // scales that basis by the integer length. It then adds five basis units
    // to X/Z and applies the three remaining type-2 random draws to Y.
    const auto shifted_short = [](std::int32_t value) {
        const std::uint32_t bits =
            static_cast<std::uint32_t>(value >> 0xc) & 0xffffu;
        const std::int32_t narrowed = static_cast<std::int32_t>(bits);
        return narrowed >= 0x8000 ? narrowed - 0x10000 : narrowed;
    };
    const std::int64_t x = shifted_short(state_.velocity.x);
    const std::int64_t y = shifted_short(state_.velocity.y);
    const std::int64_t z = shifted_short(state_.velocity.z);
    const std::int64_t squared = x * x + y * y + z * z;
    const std::int32_t length = static_cast<std::int32_t>(
        integer_square_root(squared > 0 ? static_cast<std::uint64_t>(squared)
                                        : 0));

    state_.velocity.x = narrow_impulse(
        -static_cast<std::int64_t>(state_.basis_30f4.x) * length +
        static_cast<std::int64_t>(state_.basis_30f4.x) * 5);
    state_.velocity.y = narrow_impulse(
        -static_cast<std::int64_t>(state_.basis_30f4.y) * length);
    state_.velocity.z = narrow_impulse(
        -static_cast<std::int64_t>(state_.basis_30f4.z) * length +
        static_cast<std::int64_t>(state_.basis_30f4.z) * 5);

    const auto divide_toward_zero = [](std::int64_t value,
                                       std::int64_t divisor) {
        return divisor == 0 ? value : value / divisor;
    };
    const auto random_10000 = [](std::int64_t value) {
        return value / 10000;
    };
    const std::int64_t first_term = random_10000(
        3300 * (static_cast<std::int64_t>(input.velocity_random_first) +
                0x21c) +
        0x166e30);
    const std::int64_t second_base = random_10000(
        3300 * static_cast<std::int64_t>(input.velocity_random_second) +
        0x166e30);
    const std::int64_t charge_term = divide_toward_zero(
        divide_toward_zero((first_term - second_base) *
                               state_.ollie.charge * 0x400,
                           10),
        state_.ollie.charge == 0 ? 1 : state_.ollie.charge);
    const std::int64_t third_base = random_10000(
        -5000 * (static_cast<std::int64_t>(input.velocity_random_third) +
                 0x21c));
    const std::int64_t third_term = divide_toward_zero(third_base * 0x400, 10);
    state_.velocity.y = narrow_impulse(
        static_cast<std::int64_t>(state_.velocity.y) +
        (third_term - charge_term) * 3);

    result.velocity = state_.velocity;
    result.velocity_shaped = true;
    result.handled = true;
    result.state_requested = true;
    return result;
}

AirCollisionRecoveryResult PhysicsStateMachine::handle_air_collision_recovery(
    const AirCollisionRecoveryInput& input) {
    AirCollisionRecoveryResult result;

    // FUN_00497aa0's first branch preserves a latched launch mode for a
    // short/recent recovery window (or whenever the selected material says
    // contact is special) by entering the raw-2 transient state.
    if (state_.ollie.mode_latched != 0 &&
        (input.recovery_frame == 0 ||
         input.current_frame - input.recovery_frame < 5 ||
         input.material_contact)) {
        result.request = request_state(
            2, retail::kAirCollisionTransientReason,
            retail::kAirCollisionTransientRequestCallsite);
        result.handled = true;
        result.transient_requested = true;
        return result;
    }

    result.request = request_state(
        1, retail::kAirCollisionRecoveryReason,
        retail::kAirCollisionRecoveryRequestCallsite);
    result.recovery_requested = true;

    // The optional correction uses the selected result's signed-short X/Z
    // direction and a speed-derived Q12 scalar.  FUN_004962f0 is an empty
    // function in the retail image, so there is no additional hidden write
    // after these two velocity components.
    if (state_.up.held != 0 || state_.heading_deadband < -0x28) {
        const std::int32_t squared = fixed12_dot(state_.velocity,
                                                 state_.velocity);
        const std::uint64_t nonnegative_squared =
            squared > 0 ? static_cast<std::uint64_t>(squared) : 0;
        const std::int32_t length = static_cast<std::int32_t>(
            integer_square_root(nonnegative_squared));
        const std::int64_t length_times_64 =
            static_cast<std::int64_t>(length) * 0x40;
        const std::int32_t correction_scalar = static_cast<std::int32_t>(
            (length_times_64 + ((length_times_64 >> 31) & 3)) >> 2);
        const FixedVec3 direction = signed_short_components(
            input.collision_direction);
        state_.velocity.x = narrow_impulse(
            static_cast<std::int64_t>(state_.velocity.x) -
            fixed12_scalar_multiply(correction_scalar, direction.x));
        state_.velocity.z = narrow_impulse(
            static_cast<std::int64_t>(state_.velocity.z) -
            fixed12_scalar_multiply(correction_scalar, direction.z));
        result.velocity_adjusted = true;
    }

    state_.ollie.mode = 0;
    if (state_.ollie.in_progress == 0 || state_.ollie.recovery_latch == 0) {
        state_.ollie.in_progress = 1;
        result.in_progress_set = true;
    }
    result.handled = true;
    return result;
}

AirOrientationPreparationResult
PhysicsStateMachine::prepare_in_air_orientation() noexcept {
    AirOrientationPreparationResult result;
    result.handled = true;

    // 0x00497df0 advances the rolling axis before rebuilding the other two
    // axes. The comparison is against -0xe0c, and the fallback is the
    // canonical -Y Q12 vector.
    if (state_.basis_310c.y < -0xe0c) {
        state_.basis_310c = FixedVec3{0, -0x1000, 0};
        result.rolling_axis_reset = true;
    } else {
        state_.basis_310c.y -= 500;
    }
    state_.basis_310c = normalize_retail_fixed12(state_.basis_310c);

    // FUN_004e2ff0 reads the short-valued globals, so each source basis is
    // sign-extended from 16 bits at both cross-product sites.
    const FixedVec3 basis_30f4 = signed_short_components(state_.basis_30f4);
    const FixedVec3 basis_310c = signed_short_components(state_.basis_310c);
    state_.basis_3100 = normalize_retail_fixed12(
        fixed12_cross(basis_30f4, basis_310c));

    const FixedVec3 basis_3100 = signed_short_components(state_.basis_3100);
    state_.basis_30f4 = fixed12_cross(basis_310c, basis_3100);

    // FUN_0049c850 writes only the low 16 bits into the object orientation
    // matrix. The recovered basis fields remain the full integer values.
    const auto published_short = [](std::int32_t value) {
        return static_cast<std::int16_t>(sign_extend_short(value));
    };
    state_.orientation_shorts = {
        published_short(state_.basis_3100.x),
        published_short(state_.basis_310c.x),
        published_short(state_.basis_30f4.x),
        published_short(state_.basis_3100.y),
        published_short(state_.basis_310c.y),
        published_short(state_.basis_30f4.y),
        published_short(state_.basis_3100.z),
        published_short(state_.basis_310c.z),
        published_short(state_.basis_30f4.z),
    };

    result.basis = OrientationBasis{
        state_.basis_30f4,
        state_.basis_3100,
        state_.basis_310c,
    };
    return result;
}

UprightCorrectionResult PhysicsStateMachine::apply_upright_correction(
    const FixedVec3& global_up) noexcept {
    UprightCorrectionResult result;
    // This is the caller-side gate immediately before FUN_0049c330 in the
    // common air handler. State 2 owns a collision-transient path and an
    // auxiliary nonzero state suppresses the correction.
    if (state_.raw_state == 2 || state_.auxiliary != 0) {
        return result;
    }
    result.handled = true;

    // 0x0049c330 copies the current +0x30f4 axis and DAT_0056b7c0 into
    // signed-short scratch words before FUN_004e2ff0. The dot uses the full
    // published +0x310c field against that raw cross result.
    const FixedVec3 current_axis = signed_short_components(state_.basis_30f4);
    const FixedVec3 up_axis = signed_short_components(global_up);
    const FixedVec3 cross = fixed12_cross(current_axis, up_axis);
    result.cross_dot = fixed12_dot(state_.basis_310c, cross);

    if (result.cross_dot > 0x29) {
        result.rotation_angle = 0xb;
    } else if (result.cross_dot < -0x29) {
        // Retail writes 0xff5 into an unsigned short angle slot. In the
        // 12-bit turn domain that is -0xb, and therefore has the same
        // positive-cosine/negative-sine matrix below.
        result.rotation_angle = -0xb;
    }

    if (result.rotation_angle != 0) {
        state_.orientation_shorts = rotate_orientation_z_small(
            state_.orientation_shorts, result.rotation_angle);
        result.rotation_applied = true;
    }

    const OrientationBasis basis =
        copy_orientation_basis(state_.orientation_shorts);
    state_.basis_30f4 = basis.basis_30f4;
    state_.basis_3100 = basis.basis_3100;
    state_.basis_310c = basis.basis_310c;
    result.basis = basis;
    return result;
}

OrientationRecoveryResult PhysicsStateMachine::apply_orientation_recovery() noexcept {
    OrientationRecoveryResult result;
    if (state_.orientation_recovery_progress >= 0x18001) {
        result.skipped = true;
        return result;
    }

    // 0x0049d080 reads the target as signed shorts, while the recovery base
    // is a full 32-bit interpolation vector. The retail `>> 2` is SAR, so
    // keep it as a signed shift rather than replacing it with division.
    const FixedVec3 target_short{
        state_.orientation_target_shorts[0],
        state_.orientation_target_shorts[1],
        state_.orientation_target_shorts[2],
    };
    FixedVec3 target = target_short;
    if (state_.orientation_recovery_progress != 0x18000) {
        const auto approach = [](std::int32_t destination,
                                 std::int32_t base) {
            return ((destination - base) >> 2) + base;
        };
        target = FixedVec3{
            approach(target_short.x, state_.orientation_recovery_base.x),
            approach(target_short.y, state_.orientation_recovery_base.y),
            approach(target_short.z, state_.orientation_recovery_base.z),
        };
    }
    target = normalize_retail_fixed12(target);
    result.target = target;

    // The helper receives signed 16-bit globals. The first source basis is
    // the object's +0x2e5c/+0x2e62/+0x2e68 axis, not the already-expanded
    // +0x30f4 fields.
    const FixedVec3 current_30f4{
        state_.orientation_shorts[2],
        state_.orientation_shorts[5],
        state_.orientation_shorts[8],
    };
    const FixedVec3 target_short_again = signed_short_components(target);
    const FixedVec3 first_cross = normalize_retail_fixed12(
        fixed12_cross(current_30f4, target_short_again));
    const FixedVec3 first_cross_short = signed_short_components(first_cross);
    const FixedVec3 second_cross = fixed12_cross(
        target_short_again, first_cross_short);
    const FixedVec3 published_3100 = signed_short_components(first_cross);
    const FixedVec3 published_310c = target_short_again;
    const FixedVec3 published_30f4 = signed_short_components(second_cross);

    state_.basis_3100 = published_3100;
    state_.basis_310c = published_310c;
    state_.basis_30f4 = published_30f4;
    state_.orientation_shorts = {
        static_cast<std::int16_t>(published_3100.x),
        static_cast<std::int16_t>(published_310c.x),
        static_cast<std::int16_t>(published_30f4.x),
        static_cast<std::int16_t>(published_3100.y),
        static_cast<std::int16_t>(published_310c.y),
        static_cast<std::int16_t>(published_30f4.y),
        static_cast<std::int16_t>(published_3100.z),
        static_cast<std::int16_t>(published_310c.z),
        static_cast<std::int16_t>(published_30f4.z),
    };
    state_.orientation_recovery_base = target;

    result.handled = true;
    result.basis = OrientationBasis{
        state_.basis_30f4,
        state_.basis_3100,
        state_.basis_310c,
    };
    return result;
}

GroundActionStepResult PhysicsStateMachine::apply_ground_action_step(
    std::int32_t frame_delta_fixed) noexcept {
    GroundActionStepResult result;
    if (state_.raw_state != 0 && state_.raw_state != 7) {
        return result;
    }
    result.grounded_path = true;

    const std::int32_t old_target_x = state_.movement_target_x;
    const std::int32_t old_target_z = state_.movement_target_z;
    state_.steering_active = 0;

    // 0x00493370 maps +0x29b7 to the three retail ground speed profiles:
    // 0 -> 0x3c, 1 -> 0x78, and all other nonzero values -> 0xb4.
    const std::int32_t speed_profile =
        static_cast<std::uint8_t>(state_.ground_speed_profile);
    const std::int32_t speed_units =
        speed_profile == 0 ? 0x3c : (speed_profile == 1 ? 0x78 : 0xb4);
    std::int32_t target_step = ground_arithmetic_shift_right(
        ground_mul_low32(
            ground_mul_low32(speed_units, 0x100),
            frame_delta_fixed),
        retail::kFixedPointShift);
    std::int32_t target_limit = 0x2d000;

    const bool left_held = state_.left.held != 0;
    const bool right_held = state_.right.held != 0;
    const bool down_held = state_.down.held != 0;
    if (state_.heading_deadband < 0x1f && !down_held) {
        state_.brake_mode = 0;
    } else {
        state_.brake_mode = 1;
        target_step = ground_mul_low32(target_step, 2);
        target_limit = 0x5a000;

        const std::int32_t fixed12_speed_squared = fixed12_dot(
            state_.velocity, state_.velocity);
        const std::uint64_t speed_squared = fixed12_speed_squared > 0
                                                 ? static_cast<std::uint64_t>(
                                                       fixed12_speed_squared)
                                                 : 0;
        const std::uint64_t speed_length = integer_square_root(speed_squared);
        const std::int64_t divisor =
            (static_cast<std::int64_t>(speed_length) << 6) >> 12;
        if (divisor > 0) {
            state_.acceleration.x = ground_sub32(
                state_.acceleration.x,
                static_cast<std::int32_t>(
                    static_cast<std::int64_t>(state_.velocity.x) / divisor));
            state_.acceleration.y = ground_sub32(
                state_.acceleration.y,
                static_cast<std::int32_t>(
                    static_cast<std::int64_t>(state_.velocity.y) / divisor));
            state_.acceleration.z = ground_sub32(
                state_.acceleration.z,
                static_cast<std::int32_t>(
                    static_cast<std::int64_t>(state_.velocity.z) / divisor));
        }
    }
    result.brake_mode = state_.brake_mode != 0;
    result.target_step = target_step;
    result.target_limit = target_limit;

    const auto clamp_target = [target_limit](std::int32_t value) {
        if (value < -target_limit) {
            return -target_limit;
        }
        if (value > target_limit) {
            return target_limit;
        }
        return value;
    };
    const auto is_near_zero = [](std::int32_t value) {
        return (static_cast<std::uint32_t>(value) + 0x800u) & 0xfffff000u;
    };
    const auto steer_left = [&] {
        state_.steering_active = 1;
        state_.movement_target_x = clamp_target(
            ground_sub32(state_.movement_target_x, target_step));
    };
    const auto steer_right = [&] {
        state_.steering_active = 1;
        state_.movement_target_x = clamp_target(
            ground_add32(state_.movement_target_x, target_step));
    };
    const auto decay_target = [&] {
        const std::int32_t decay = state_.brake_mode != 0
                                       ? ground_arithmetic_shift_right(
                                             state_.movement_target_x, 1)
                                       : ground_arithmetic_shift_right(
                                             state_.movement_target_x, 2);
        state_.movement_target_x = ground_sub32(
            state_.movement_target_x,
            decay);
        if (is_near_zero(state_.movement_target_x) == 0) {
            state_.movement_target_x = 0;
        }
    };
    const auto analog_target = [&] {
        const std::int32_t target_product = ground_mul_low32(
            target_limit,
            static_cast<std::int32_t>(state_.heading_input));
        const std::int32_t desired = clamp_target(
            ground_arithmetic_shift_right(
                target_product < 0
                    ? ground_add32(target_product, 0x7f)
                    : target_product,
                7));
        const std::int32_t denominator = (target_limit >> 12) / 2;
        if (state_.movement_target_x < desired) {
            state_.steering_active = 1;
            const std::int32_t distance = ground_arithmetic_shift_right(
                ground_sub32(desired, state_.movement_target_x),
                12);
            const std::int32_t correction = ground_add32(
                ground_signed_div32(
                    ground_mul_low32(distance, target_step),
                    denominator),
                target_step);
            state_.movement_target_x = ground_add32(
                state_.movement_target_x,
                correction);
            if (desired < state_.movement_target_x) {
                state_.movement_target_x = desired;
            }
        } else if (state_.movement_target_x > desired) {
            state_.steering_active = 1;
            const std::int32_t distance = ground_arithmetic_shift_right(
                ground_sub32(state_.movement_target_x, desired),
                12);
            const std::int32_t weighted = ground_signed_div32(
                ground_mul_low32(distance, target_step),
                denominator);
            state_.movement_target_x = ground_sub32(
                ground_sub32(state_.movement_target_x, weighted),
                target_step);
            if (state_.movement_target_x < desired) {
                state_.movement_target_x = desired;
            }
        }
    };

    const std::int32_t absolute_heading = absolute_value(
        static_cast<std::int32_t>(state_.heading_input));
    if (absolute_heading < 0x1a) {
        if (left_held) {
            steer_left();
        } else if (!right_held) {
            decay_target();
        } else {
            steer_right();
        }
    } else if (left_held) {
        steer_left();
    } else if (!right_held) {
        analog_target();
    } else {
        steer_right();
    }

    // The retail routine normalizes the two target words immediately before
    // calling 0x00492f20. The helper's animation/lean side effects are not
    // folded into the movement arithmetic here.
    state_.movement_target_z = state_.movement_target_x;
    result.steering_active = state_.steering_active != 0;
    result.target_changed = state_.movement_target_x != old_target_x ||
                            state_.movement_target_z != old_target_z;
    return result;
}

GroundSurfaceAccelerationResult PhysicsStateMachine::apply_ground_surface_acceleration(
    const GroundSurfaceAccelerationInput& input) noexcept {
    GroundSurfaceAccelerationResult result =
        opentony::physics::apply_ground_surface_acceleration(input);
    state_.velocity = result.velocity;
    state_.acceleration = result.acceleration;
    return result;
}

GroundCollisionResult PhysicsStateMachine::apply_ground_collision_handoff(
    const GroundCollisionInput& input) noexcept {
    const GroundCollisionResult result =
        opentony::physics::apply_ground_collision_handoff(input);
    state_.velocity = result.velocity;
    state_.acceleration = result.acceleration;
    if (input.collision_result) {
        state_.contact_surface_normal = input.surface_normal;
        if (result.surface_vector_published) {
            state_.contact_surface_vector = result.projected_surface_vector;
            state_.contact_surface_vector_valid = true;
        } else {
            state_.contact_surface_vector_valid = false;
        }
    }
    return result;
}

FixedVec3 PhysicsStateMachine::integrate_in_air_position(
    std::int32_t frame_delta_fixed) noexcept {
    const FixedVec3 delta = compute_in_air_position_delta(
        state_.velocity, state_.acceleration, frame_delta_fixed);
    state_.position.x += delta.x;
    state_.position.y += delta.y;
    state_.position.z += delta.z;
    return delta;
}

FixedVec3 PhysicsStateMachine::integrate_velocity(
    std::int32_t frame_delta_fixed) noexcept {
    const FixedVec3 delta = compute_velocity_delta(
        state_.acceleration, frame_delta_fixed);
    state_.velocity.x = narrow_impulse(
        static_cast<std::int64_t>(state_.velocity.x) + delta.x);
    state_.velocity.y = narrow_impulse(
        static_cast<std::int64_t>(state_.velocity.y) + delta.y);
    state_.velocity.z = narrow_impulse(
        static_cast<std::int64_t>(state_.velocity.z) + delta.z);
    return delta;
}

FixedVec3 PhysicsStateMachine::apply_in_air_gravity() noexcept {
    state_.acceleration.y = narrow_impulse(
        static_cast<std::int64_t>(state_.acceleration.y) +
        state_.gravity_acceleration);
    return state_.acceleration;
}

InAirActionControlResult PhysicsStateMachine::apply_in_air_action_control() noexcept {
    const InAirActionControlInput input{
        state_.velocity,
        state_.acceleration,
        state_.basis_30f4,
        state_.basis_3100,
        state_.basis_310c,
        state_.gravity_acceleration,
        state_.air_control_enabled,
        state_.kick.held != 0,
        state_.up.held != 0,
        state_.down.held != 0,
        state_.spin_left.held != 0,
        state_.spin_right.held != 0,
    };
    const InAirActionControlResult result =
        opentony::physics::apply_in_air_action_control(input);
    state_.acceleration = result.acceleration;
    return result;
}

VelocityDampingResult PhysicsStateMachine::apply_postphysics_velocity_damping(
    std::int32_t frame_delta_fixed, const VelocityDampingRandom& random,
    bool apply_low_speed_damping) noexcept {
    const VelocityDampingResult result = opentony::physics::apply_velocity_damping(
        state_.velocity, frame_delta_fixed, random, apply_low_speed_damping);
    state_.velocity = result.velocity;
    return result;
}

bool PhysicsStateMachine::apply_in_air_jump_hold_effect() noexcept {
    // The retail comparison sequence at 0x00497fff is:
    //   action_record[0] != 0 && action_record[4] > 2
    // followed by the two stores at 0x00498009 and 0x0049800c. The action
    // record itself is owned by the input/skater boundary; this method only
    // models the physics-side consequence.
    if (state_.jump.held == 0 ||
        state_.jump.held_counter <= retail::kAirJumpHoldCounterThreshold) {
        return false;
    }

    state_.acceleration.y = 0;
    state_.velocity.y = 0;
    return true;
}

bool PhysicsStateMachine::accept_air_contact(bool accepted,
                                             const FixedVec3& contact_position,
                                             PositionCommitCallback callback,
                                             void* user) {
    AirContactInput input;
    input.accepted = accepted;
    input.position = contact_position;
    return accept_air_contact(input, callback, user);
}

bool PhysicsStateMachine::accept_air_contact(const AirContactInput& input,
                                             PositionCommitCallback callback,
                                             void* user) {
    if (!input.accepted || !uses_common_air_handler(state_.raw_state)) {
        return false;
    }

    commit_position(input.position, callback, user);

    // The in-air contact branch commits the swept contact position even when
    // alternate raw state 3 is still inside its thirty-frame launch grace.
    // In that case it does not issue the normal landing request yet.
    const std::int64_t launch_age =
        static_cast<std::int64_t>(state_.frame_counter) - state_.ollie.launch_frame;
    if (state_.raw_state == 3 && launch_age <= 0x1e) {
        return true;
    }

    // Normal accepted contact writes the landing/effect marker, publishes the
    // collision-owned identity, and clears the shared movement target before
    // requesting raw state 0. The retail landing helper runs immediately after
    // setting +0x2ec0, before the identity and request stores.
    state_.ollie.landing_effect_state = 1;
    state_.ollie.landing_contact_auxiliary = 0;
    apply_landing_cleanup();
    if (state_.off_ground.word_2e90 != 0) {
        return true;
    }
    state_.ollie.landing_contact_identity = input.contact_identity;
    state_.ollie.landing_frame = static_cast<std::int32_t>(state_.frame_counter);
    state_.movement_target_x = 0;
    request_state(0, retail::kLandingReason, retail::kLandingRequestCallsite);
    if (input.surface_normal_valid) {
        state_.contact_surface_normal = input.surface_normal;
        state_.velocity = project_velocity_preserving_speed(
                              state_.velocity, input.surface_normal)
                              .velocity;
    }
    return true;
}

bool PhysicsStateMachine::accept_standard_air_collision(
    const AirCollisionObservation& observation, std::int32_t last_surface_frame,
    const AirContactInput& contact, PositionCommitCallback callback, void* user) {
    AirLandingPredicateInput predicate;
    predicate.raw_state = state_.raw_state;
    predicate.prephysics_blocked = state_.prephysics_blocked != 0;
    predicate.jump_held = state_.jump.held != 0;
    predicate.jump_inactive_counter = state_.jump.inactive_counter;
    predicate.current_frame = static_cast<std::int32_t>(state_.frame_counter);
    predicate.last_surface_frame = last_surface_frame;
    if (!standard_air_landing_accepted(observation, predicate)) {
        return false;
    }

    AirContactInput accepted_contact = contact;
    accepted_contact.accepted = true;
    // FUN_00497f40 publishes DAT_0056b7e8 to player+0x30b0 immediately before
    // the landing request. The caller-provided contact identity remains useful
    // for the lower-level direct contact boundary, but the standard predicate
    // path is driven by the decoded observation.
    accepted_contact.contact_identity =
        static_cast<std::int32_t>(observation.material_type);
    return accept_air_contact(accepted_contact, callback, user);
}

void PhysicsStateMachine::commit_position(const FixedVec3& next,
                                          PositionCommitCallback callback,
                                          void* user) {
    const FixedVec3 previous = state_.position;
    state_.old_position = previous;
    state_.position = next;
    if (callback != nullptr) {
        callback(state_, previous, next, user);
    }
}

}  // namespace opentony::physics
