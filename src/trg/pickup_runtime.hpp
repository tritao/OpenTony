#pragma once

#include "../assets/psx_asset.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <string_view>

namespace opentony::trg {

// The retail pickup constructor selects a resource and then resolves this
// model-name hash through FUN_004b1de0.  Keep the table separate from the
// scene loader so the source TRG subtype remains testable without assets.
struct PickupModelDefinition {
    std::string_view resource;
    std::uint32_t model_name{};
};

[[nodiscard]] std::optional<PickupModelDefinition> pickup_model_definition(
    std::uint16_t subtype) noexcept;

[[nodiscard]] std::size_t pickup_model_index(
    const assets::PsxArchive& archive,
    std::uint32_t model_name) noexcept;

// Raw constructor/update words at pickup +0x14/+0x16/+0x18 and
// +0x70/+0x72/+0x74. FUN_004a8300 advances the first triplet by the signed
// second triplet scaled by DAT_0056865c >> 8. The fields remain offset-named
// because their final animation/gameplay meaning is not yet proven.
struct PickupRawMotionState {
    std::array<std::int16_t, 3> words_14_18{};
    std::array<std::int16_t, 3> words_70_74{};
};

void advance_pickup_raw_motion(
    PickupRawMotionState& state,
    std::int32_t speed_scale_q8) noexcept;

// Raw lifecycle words used by FUN_004a8620. +0xf0 is a countdown in the
// retail pickup object; 0xffff is the no-countdown sentinel. +0xea/+0xec
// drive the final-60-tick phase, while +0xfc counts update calls. The global
// fade byte and object flag word are explicit inputs because their producers
// live outside the pickup/TRG loader boundary.
struct PickupRawLifecycleState {
    std::uint16_t timer_f0{0xffff};
    std::uint16_t phase_ea{0x0032};
    std::uint16_t phase_ec{0x0032};
    std::uint32_t update_calls{};
    std::uint16_t object_flags{};
};

struct PickupLifecycleUpdate {
    bool requested_glow{};
    bool cleared_active{};
    bool destroyed{};
};

[[nodiscard]] PickupLifecycleUpdate advance_pickup_lifecycle(
    PickupRawLifecycleState& state,
    std::uint8_t global_fade_flags) noexcept;

} // namespace opentony::trg
