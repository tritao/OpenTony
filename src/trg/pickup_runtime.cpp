#include "pickup_runtime.hpp"

#include <algorithm>

namespace opentony::trg {

std::optional<PickupModelDefinition> pickup_model_definition(
    std::uint16_t subtype) noexcept {
    switch (subtype) {
    case 4:
        return PickupModelDefinition{"items", 0x2328a71c};
    case 5:
        return PickupModelDefinition{"items", 0x311d55d4};
    case 6:
        return PickupModelDefinition{"items", 0x2ebf22ca};
    case 10:
        return PickupModelDefinition{"items", 0x29b68a16};
    case 15:
        return PickupModelDefinition{"items", 0x34524351};
    case 16:
    case 18:
        return PickupModelDefinition{"items", 0x7c1b2c4a};
    case 24:
        return PickupModelDefinition{"items", 0x694ed947};
    case 25:
        return PickupModelDefinition{"items", 0x260f4f80};
    case 26:
        return PickupModelDefinition{"items", 0xcc4e141f};
    case 0x664:
        return PickupModelDefinition{"skmedals", 0x54636518};
    case 0x665:
        return PickupModelDefinition{"skmedals", 0xba6d0434};
    case 0x666:
        return PickupModelDefinition{"skmedals", 0x2364558e};
    // Subtype 33 selects a model from the current level's conditional table;
    // its source-level selector is not yet recovered, so leave it explicit.
    default:
        return std::nullopt;
    }
}

std::size_t pickup_model_index(
    const assets::PsxArchive& archive,
    std::uint32_t model_name) noexcept {
    const auto found = std::find(
        archive.model_names().begin(),
        archive.model_names().end(),
        model_name);
    return found == archive.model_names().end()
        ? static_cast<std::size_t>(-1)
        : static_cast<std::size_t>(std::distance(archive.model_names().begin(), found));
}

void advance_pickup_raw_motion(
    PickupRawMotionState& state,
    std::int32_t speed_scale_q8) noexcept {
    for (std::size_t axis = 0; axis < state.words_14_18.size(); ++axis) {
        const std::int32_t delta = static_cast<std::int32_t>(state.words_70_74[axis])
            * speed_scale_q8 >> 8;
        // Retail writes a WORD, so the result wraps at 16 bits even when the
        // host intermediate is wider.
        const std::uint16_t wrapped = static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(state.words_14_18[axis])
            + static_cast<std::int32_t>(delta));
        state.words_14_18[axis] = static_cast<std::int16_t>(wrapped);
    }
}

PickupLifecycleUpdate advance_pickup_lifecycle(
    PickupRawLifecycleState& state,
    std::uint8_t global_fade_flags) noexcept {
    PickupLifecycleUpdate result{};
    ++state.update_calls;

    // FUN_004a8620 leaves the 0xffff sentinel untouched and performs no
    // fade/destroy work for it. All other timer values saturate at zero
    // after the per-update decrement.
    if (state.timer_f0 == 0xffffU) {
        return result;
    }
    if (state.timer_f0 != 0) {
        --state.timer_f0;
    }

    if (state.timer_f0 < 0x003cU) {
        // The retail multiply-by-0x88888889 sequence is signed division by
        // 60 for these non-negative WORD operands. The final WORD store is
        // retained by the explicit cast below.
        const std::uint32_t phase =
            state.update_calls
            + (static_cast<std::uint32_t>(state.timer_f0)
                * static_cast<std::uint32_t>(state.phase_ea)) / 60U;
        state.phase_ec = static_cast<std::uint16_t>(phase);
    }

    if (state.timer_f0 > 0x001eU) {
        if ((global_fade_flags & 0x02U) != 0) {
            state.object_flags = static_cast<std::uint16_t>(
                state.object_flags & static_cast<std::uint16_t>(~0x0001U));
            result.cleared_active = true;
        } else {
            state.object_flags = static_cast<std::uint16_t>(
                state.object_flags | 0x0041U);
            result.requested_glow = true;
        }
    } else if ((global_fade_flags & 0x01U) != 0) {
        state.object_flags = static_cast<std::uint16_t>(
            state.object_flags & static_cast<std::uint16_t>(~0x0001U));
        result.cleared_active = true;
    } else {
        state.object_flags = static_cast<std::uint16_t>(
            state.object_flags | 0x0041U);
        result.requested_glow = true;
    }

    if (state.timer_f0 == 0) {
        // The virtual call at FUN_004a86de is the pickup destruction path.
        result.destroyed = true;
    }
    return result;
}

} // namespace opentony::trg
