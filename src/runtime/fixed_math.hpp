#pragma once

#include "position_commit.hpp"

#include <cstdint>

namespace opentony::runtime {

constexpr std::int32_t kFixedOne = 0x1000;

// Retail FUN_004f5fc0 and FUN_004f5f90 use a Q12 scale and convert through an
// x87 helper configured for truncation toward zero. These helpers preserve
// that arithmetic while checking the native 32-bit result boundary.
[[nodiscard]] std::int32_t fixed_multiply_q12(
    std::int32_t left,
    std::int32_t right);

// Retail frame-scale operations use a signed integer product followed by an
// arithmetic shift of eight bits (DAT_0056865c is Q8 here).
[[nodiscard]] std::int32_t fixed_scale_q8(
    std::int32_t value,
    std::int32_t scale_q8);

[[nodiscard]] std::int32_t fixed_dot_q12(
    const FixedPosition& left,
    const FixedPosition& right);

// Retail FUN_004f5f90(vector, vector) followed by FUN_004f53b0. The dot
// helper truncates the squared fixed-point sum after the 1/4096 scale, then
// the integer square root is taken. This is the scalar before the caller's
// speed-metric << 6.
[[nodiscard]] std::int32_t retail_vector_magnitude_q12(
    const FixedPosition& vector) noexcept;

// Retail FUN_004f5f90(vector, vector) followed by FUN_004f53b0 and the
// caller's << 6. The dot helper truncates the squared fixed-point sum after
// the 1/4096 scale, then the integer square root is taken.
[[nodiscard]] std::int32_t retail_vector_speed_metric(
    const FixedPosition& vector) noexcept;

// Exact geometric core of retail FUN_00490610: remove the vector component
// parallel to a fixed-point collision normal. Returns the Q12 dot product
// used for the projection.
[[nodiscard]] std::int32_t remove_normal_component(
    FixedPosition& vector,
    const FixedPosition& normal);

} // namespace opentony::runtime
