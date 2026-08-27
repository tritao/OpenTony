#include "ground_turn.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int32_t wrap32(std::int64_t value) noexcept {
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

[[nodiscard]] std::int32_t arithmetic_shift_right(
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

[[nodiscard]] std::int32_t add32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return wrap32(static_cast<std::int64_t>(left) + right);
}

[[nodiscard]] std::int32_t sub32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return wrap32(static_cast<std::int64_t>(left) - right);
}

[[nodiscard]] std::int32_t mul_low32(
    std::int32_t left,
    std::int32_t right) noexcept {
    return wrap32(static_cast<std::int64_t>(left) * right);
}

[[nodiscard]] std::int32_t signed_div32(
    std::int32_t numerator,
    std::int32_t denominator) noexcept {
    return static_cast<std::int32_t>(
        static_cast<std::int64_t>(numerator) / denominator);
}

[[nodiscard]] std::int32_t clamp_to_limit(
    std::int32_t value,
    std::int64_t limit) noexcept {
    if (static_cast<std::int64_t>(value) < -limit) {
        return static_cast<std::int32_t>(-limit);
    }
    if (static_cast<std::int64_t>(value) > limit) {
        return static_cast<std::int32_t>(limit);
    }
    return value;
}

} // namespace

GroundTurnResult GroundTurn::update(
    std::int32_t current,
    bool left,
    bool right,
    GroundTurnConfig config) noexcept {
    const std::int32_t base_turn = config.turn_profile == 0
        ? 0x3c
        : config.turn_profile == 1 ? 0x78 : 0xb4;
    // FUN_00493370 performs the product in a 32-bit register before SAR 8.
    const std::int32_t step = arithmetic_shift_right(
        wrap32(static_cast<std::int64_t>(base_turn) * 0x100 *
               config.frame_scale_q8),
        8);
    const std::int64_t configured_limit = config.limit;
    const std::int64_t limit = configured_limit < 0
        ? -configured_limit
        : configured_limit;
    const std::int32_t positive_limit = static_cast<std::int32_t>(limit);
    const std::int32_t negative_limit = static_cast<std::int32_t>(-limit);
    const std::int32_t denominator = std::max<std::int32_t>(
        1,
        static_cast<std::int32_t>(static_cast<std::uint64_t>(limit >> 12) / 2));

    std::int32_t next = current;
    bool policy_changed = false;
    // The retail branch gives Left priority if both records are active.
    if (left) {
        policy_changed = true;
        next = sub32(next, step);
        if (next < negative_limit) {
            next = negative_limit;
        }
    } else if (right) {
        policy_changed = true;
        next = add32(next, step);
        if (next > positive_limit) {
            next = positive_limit;
        }
    } else if ((config.lean < 0 ? -static_cast<int>(config.lean)
                               : static_cast<int>(config.lean)) < 0x1a) {
        // FUN_00493370's no-record branch uses +3144 >> 2 for the ordinary
        // ground profile (and quantizes the small remainder to zero). The
        // board/surface profile can use a separate >> 1 path.
        const std::int32_t decay_shift = std::clamp(
            config.release_decay_shift,
            1,
            30);
        const std::int32_t decay = arithmetic_shift_right(next, decay_shift);
        next = sub32(next, decay);
        if (((static_cast<std::uint32_t>(next) + 0x800U) & 0xfffff000U) == 0) {
            next = 0;
        }
    } else {
        // The signed byte is multiplied in a 32-bit register. Adding 0x7f
        // only for negative products makes the following SAR 7 truncate
        // toward zero, exactly as in the retail instruction sequence.
        const std::int32_t product = mul_low32(
            positive_limit,
            static_cast<std::int32_t>(config.lean));
        const std::int32_t corrected_product = product < 0
            ? add32(product, 0x7f)
            : product;
        const std::int32_t target = clamp_to_limit(
            arithmetic_shift_right(corrected_product, 7),
            limit);
        if (next < target) {
            policy_changed = true;
            const std::int32_t distance = arithmetic_shift_right(
                sub32(target, next),
                12);
            const std::int32_t weighted = signed_div32(
                mul_low32(distance, step),
                denominator);
            next = add32(next, add32(weighted, step));
            if (target < next) {
                next = target;
            }
        } else if (target < next) {
            policy_changed = true;
            const std::int32_t distance = arithmetic_shift_right(
                sub32(next, target),
                12);
            const std::int32_t weighted = signed_div32(
                mul_low32(distance, step),
                denominator);
            next = sub32(sub32(next, weighted), step);
            if (next < target) {
                next = target;
            }
        }
    }
    const std::int32_t accumulator = next;
    return GroundTurnResult{
        accumulator,
        accumulator,
        sub32(accumulator, current),
        limit == 0x5a000,
        policy_changed,
    };
}

std::int32_t GroundTurn::angle12(
    std::int32_t turn_accumulator,
    std::int32_t frame_scale_q8) noexcept {
    const std::int64_t turn_units = turn_accumulator >> 12;
    const std::int64_t scaled = (turn_units * frame_scale_q8) >> 8;
    return static_cast<std::int32_t>(scaled & 0xfff);
}

} // namespace opentony::runtime
