#include "ollie.hpp"

#include <limits>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int64_t absolute_value(std::int64_t value) noexcept {
    return value < 0 ? -value : value;
}

[[nodiscard]] std::int32_t narrow_impulse(std::int64_t value) noexcept {
    if (value > std::numeric_limits<std::int32_t>::max()) {
        return std::numeric_limits<std::int32_t>::max();
    }
    if (value < std::numeric_limits<std::int32_t>::min()) {
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(value);
}

} // namespace

OllieImpulseResult compute_ollie_vertical_impulse(
    const OllieImpulseInput& input) noexcept {
    const OllieImpulseRandom& random = input.random;
    const bool high_slope = absolute_value(input.slope_metric) >= 0x9c4;
    std::int64_t impulse = 0;

    if (!high_slope) {
        const std::int64_t first_term =
            ((((-900 - random.fifth) * 3000) / 10000) * 0x400) / 10;
        const std::int64_t charge_term =
            ((((random.third + 900) * 3000) / 10000 -
              (random.fourth * 0xce4 + 0x166e30) / 10000) * input.charge * 0x400) /
            10;
        const std::int64_t denominator =
            0xf - (random.first + random.second) / 0x14;
        // Valid retail random draws keep this non-zero. A bounded fallback
        // keeps synthetic replay fixtures defined without changing retail
        // results for valid draws.
        const std::int64_t safe_denominator = denominator == 0 ? 1 : denominator;
        impulse = (first_term - charge_term / safe_denominator) * 3;
    } else {
        const std::int64_t first_term =
            ((((-0x21c - random.fifth) * 5000) / 10000) * 0x400) / 10;
        const std::int64_t charge_term =
            ((((random.third + 0x21c) * 5000) / 10000 -
              (random.fourth * 0xce4 + 0x166e30) / 10000) * input.charge * 0x400) /
            10;
        const std::int64_t denominator =
            0xf - (random.first + random.second) / 0x14;
        const std::int64_t safe_denominator = denominator == 0 ? 1 : denominator;
        impulse = (first_term - charge_term / safe_denominator) * 3;
    }

    OllieImpulseResult result;
    result.high_slope_branch = high_slope;
    result.adjusted_height_delta = input.height_delta_metric;
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

} // namespace opentony::runtime
