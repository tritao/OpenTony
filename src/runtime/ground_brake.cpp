#include "ground_brake.hpp"

#include <algorithm>
#include <limits>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t saturating_i32(std::int64_t value) noexcept {
    if (value > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

[[nodiscard]] std::int32_t arithmetic_shift(
    std::int64_t value,
    unsigned shift) noexcept {
    if (value >= 0) {
        return saturating_i32(value >> shift);
    }
    const std::int64_t magnitude = -value;
    const std::int64_t rounded =
        (magnitude + ((std::int64_t{1} << shift) - 1)) >> shift;
    return saturating_i32(-rounded);
}

[[nodiscard]] std::int32_t magnitude_q12(
    const FixedPosition& response) noexcept {
    const std::int64_t squared =
        static_cast<std::int64_t>(response[0]) * response[0]
        + static_cast<std::int64_t>(response[1]) * response[1]
        + static_cast<std::int64_t>(response[2]) * response[2];
    const std::uint64_t target = squared > 0
        ? static_cast<std::uint64_t>(squared)
        : 0;
    std::uint64_t low = 0;
    std::uint64_t high = std::min<std::uint64_t>(
        target + 1,
        static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1U);
    while (low + 1 < high) {
        const std::uint64_t middle = low + (high - low) / 2;
        if (middle <= target / middle) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return saturating_i32(static_cast<std::int64_t>(low));
}

[[nodiscard]] std::int32_t speed_metric(
    std::int32_t magnitude) noexcept {
    return saturating_i32(static_cast<std::int64_t>(magnitude) * 0x40);
}

[[nodiscard]] std::int32_t brake_term(
    std::int32_t value,
    std::int32_t brake_parameter) noexcept {
    if (brake_parameter < 1) {
        return arithmetic_shift(value, 4);
    }
    const std::int32_t factor = arithmetic_shift(
        static_cast<std::int64_t>(brake_parameter)
            + (brake_parameter < 0 ? 0xf : 0),
        4);
    const std::int64_t product = static_cast<std::int64_t>(value) * factor;
    // Retail uses (product + (product >> 31 & 0x7f)) >> 7, i.e. the
    // arithmetic right shift emitted by the original integer expression.
    const std::int64_t correction = product < 0 ? 0x7f : 0;
    return arithmetic_shift(product + correction, 7);
}

} // namespace

GroundBrakeResult GroundBrake::apply(
    const GroundBrakeInput& input) noexcept {
    GroundBrakeResult result{};
    result.response = input.response;
    result.magnitude_q12 = input.magnitude_q12 >= 0
        ? input.magnitude_q12
        : magnitude_q12(result.response);
    result.speed_metric = speed_metric(result.magnitude_q12);

    // Retail: iVar8 = (-normal_y * 0x1000) >> 12; clamp to 0x300;
    // threshold = ((0x1000 - iVar8) * 0x1800) / 0xd0; clamp to 0xa000.
    std::int32_t incline = arithmetic_shift(
        -static_cast<std::int64_t>(input.slope_normal_y_q12) * 0x1000,
        12);
    incline = std::max(incline, 0x300);
    result.speed_threshold = saturating_i32(
        (static_cast<std::int64_t>(0x1000 - incline) * 0x1800) / 0xd0);
    result.speed_threshold = std::max(result.speed_threshold, 0xa000);

    if (!input.surface_allows_brake || input.physics_state != 0) {
        return result;
    }

    if (result.speed_threshold < result.speed_metric) {
        for (std::int32_t& value : result.response) {
            const std::int32_t term = brake_term(value, input.brake_parameter);
            value = saturating_i32(
                static_cast<std::int64_t>(value)
                - (static_cast<std::int64_t>(term) * input.frame_scale_q8 >> 8));
        }
        result.decelerated = true;
        return result;
    }

    if (result.speed_threshold < 0xa001) {
        result.response = FixedPosition{0, 0, 0};
        result.stopped = true;
        result.requested_state7 = true;
    }
    return result;
}

} // namespace opentony::runtime
