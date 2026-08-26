#include "ground_turn.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace opentony::runtime {

GroundTurnResult GroundTurn::update(
    std::int32_t current,
    bool left,
    bool right,
    GroundTurnConfig config) noexcept {
    const std::int32_t base_turn = config.turn_profile == 0
        ? 0x3c
        : config.turn_profile == 1 ? 0x78 : 0xb4;
    const std::int64_t scaled_step =
        (static_cast<std::int64_t>(base_turn) * 0x100 * config.frame_scale_q8) >> 8;
    const std::int32_t step = static_cast<std::int32_t>(std::clamp(
        scaled_step,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())));
    const std::int64_t configured_limit = config.limit;
    const std::int64_t limit = configured_limit < 0
        ? -configured_limit
        : configured_limit;

    std::int64_t next = current;
    // The retail branch gives Left priority if both records are active.
    if (left) {
        next -= step;
    } else if (right) {
        next += step;
    } else if (std::abs(config.lean) < 0x1a) {
        // FUN_00493370's no-record branch uses +3144 >> 2 for the ordinary
        // ground profile (and quantizes the small remainder to zero). The
        // board/surface profile can use a separate >> 1 path.
        const std::int32_t decay_shift = std::clamp(
            config.release_decay_shift,
            1,
            30);
        const std::int32_t decay = static_cast<std::int32_t>(
            next >> decay_shift);
        next -= decay;
        if ((static_cast<std::uint32_t>(next + 0x800) & 0xfffff000U) == 0) {
            next = 0;
        }
    } else {
        // FUN_00493370's analog branch first forms the signed target with
        // the same truncation-toward-zero adjustment used by the x86 SAR.
        const std::int64_t product =
            static_cast<std::int64_t>(limit) * config.lean;
        const std::int64_t target = std::clamp(
            (product + ((product >> 63) & 0x7f)) >> 7,
            -limit,
            limit);
        const std::int64_t denominator = std::max<std::int64_t>(
            1,
            (limit >> 12) / 2);
        if (next < target) {
            const std::int64_t distance = next < target
                ? (target - next) >> 12
                : 0;
            const std::int64_t advance =
                (distance * step) / denominator + step;
            next = std::min(target, next + advance);
        } else if (target < next) {
            const std::int64_t distance = (next - target) >> 12;
            const std::int64_t retreat =
                (distance * step) / denominator + step;
            next = std::max(target, next - retreat);
        }
    }
    next = std::clamp(
        next,
        -limit,
        limit);
    const std::int32_t accumulator = static_cast<std::int32_t>(next);
    const std::int64_t absolute_limit = configured_limit < 0
        ? -configured_limit
        : configured_limit;
    return GroundTurnResult{
        accumulator,
        accumulator,
        accumulator - current,
        absolute_limit == 0x5a000,
        accumulator != current,
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
