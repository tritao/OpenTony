#pragma once

#include "position_commit.hpp"

#include <array>
#include <cstdint>

namespace opentony::runtime {

struct VelocityDampingInput {
    FixedPosition velocity{};
    // When negative, the native producer derives the Q12 magnitude from the
    // vector. Supplying the retail helper's result is useful for parity
    // traces because the original sqrt helper is a separate call boundary.
    std::int32_t magnitude_q12 = -1;
    std::int32_t frame_scale_q8 = 0x100;
    // Explicit outputs of the retail random calls. Keeping these as inputs
    // avoids inventing a replacement RNG stream in the physics library.
    std::int32_t rescale_roll = 0;             // FUN_0048f3a0(3)
    std::int32_t decay_roll = 0;               // FUN_0048f3a0(3)
    // FUN_004f5fc0(100, component) outputs captured at the retail helper
    // boundary. When unavailable, apply() reproduces that helper with the
    // retail constant so ordinary native callers remain self-contained.
    std::array<std::int32_t, 3> decay_component_outputs{};
    bool decay_component_outputs_available = false;
    bool apply_idle_decay = true;             // DAT_0056a3d8 mode table == 0
};

struct VelocityDampingResult {
    FixedPosition velocity{};
    std::int32_t magnitude_q12 = 0;
    std::int32_t speed_metric = 0;
    bool rescaled = false;
    bool randomized_decay = false;
    bool fine_decay = false;
    bool coarse_decay = false;

    friend bool operator==(
        const VelocityDampingResult&,
        const VelocityDampingResult&) = default;
};

// Pure fixed-point portion of retail FUN_0049d480. The surrounding frame
// decides when the routine is enabled and supplies random/mode-table values.
class VelocityDamping final {
public:
    [[nodiscard]] static VelocityDampingResult apply(
        const VelocityDampingInput& input) noexcept;
};

} // namespace opentony::runtime
