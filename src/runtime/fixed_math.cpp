#include "fixed_math.hpp"

#include <limits>
#include <stdexcept>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t checked_fixed_result(std::int64_t value) {
    if (value < std::numeric_limits<std::int32_t>::min()
        || value > std::numeric_limits<std::int32_t>::max()) {
        throw std::overflow_error("fixed-point arithmetic exceeds int32 range");
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::uint64_t integer_sqrt(std::uint64_t value) noexcept {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62U;
    while (bit > value) {
        bit >>= 2U;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1U) + bit;
        } else {
            result >>= 1U;
        }
        bit >>= 2U;
    }
    return result;
}

} // namespace

std::int32_t fixed_multiply_q12(
    std::int32_t left,
    std::int32_t right) {
    return checked_fixed_result(
        (static_cast<std::int64_t>(left) * right) / kFixedOne);
}

std::int32_t fixed_scale_q8(
    std::int32_t value,
    std::int32_t scale_q8) {
    return checked_fixed_result(
        (static_cast<std::int64_t>(value) * scale_q8) >> 8);
}

std::int32_t fixed_dot_q12(
    const FixedPosition& left,
    const FixedPosition& right) {
    const std::int64_t sum =
        static_cast<std::int64_t>(left[0]) * right[0]
        + static_cast<std::int64_t>(left[1]) * right[1]
        + static_cast<std::int64_t>(left[2]) * right[2];
    return checked_fixed_result(sum / kFixedOne);
}

std::int32_t retail_vector_speed_metric(
    const FixedPosition& vector) noexcept {
    const std::int64_t sum =
        static_cast<std::int64_t>(vector[0]) * vector[0]
        + static_cast<std::int64_t>(vector[1]) * vector[1]
        + static_cast<std::int64_t>(vector[2]) * vector[2];
    const std::uint64_t scaled = sum > 0
        ? static_cast<std::uint64_t>(sum) / kFixedOne
        : 0;
    const std::uint64_t magnitude = integer_sqrt(scaled);
    const std::uint64_t metric = magnitude * 0x40U;
    return metric > static_cast<std::uint64_t>(
        std::numeric_limits<std::int32_t>::max())
        ? std::numeric_limits<std::int32_t>::max()
        : static_cast<std::int32_t>(metric);
}

std::int32_t remove_normal_component(
    FixedPosition& vector,
    const FixedPosition& normal) {
    const std::int32_t dot = fixed_dot_q12(vector, normal);
    for (std::size_t index = 0; index < vector.size(); ++index) {
        const std::int64_t value = static_cast<std::int64_t>(vector[index])
            - fixed_multiply_q12(normal[index], dot);
        vector[index] = checked_fixed_result(value);
    }
    return dot;
}

} // namespace opentony::runtime
