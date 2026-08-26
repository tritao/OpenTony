#pragma once

#include "position_commit.hpp"

#include <cstdint>

namespace opentony::runtime {

// Inputs corresponding to the proven scalar/vector portion of retail
// FUN_0049df00. Surface/material eligibility and the raw slope normal are
// supplied by the caller because their owning object fields are not yet
// named. The response vector remains in the retail fixed-point units.
struct GroundBrakeInput {
    FixedPosition response{};
    std::int32_t magnitude_q12{-1};
    std::int32_t slope_normal_y_q12{};
    std::int32_t frame_scale_q8{0x100};
    std::int32_t brake_parameter{};
    std::int32_t physics_state{};
    bool surface_allows_brake{};
};

struct GroundBrakeResult {
    FixedPosition response{};
    std::int32_t magnitude_q12{};
    std::int32_t speed_metric{};
    std::int32_t speed_threshold{};
    bool decelerated{};
    bool stopped{};
    bool requested_state7{};

    friend bool operator==(
        const GroundBrakeResult&,
        const GroundBrakeResult&) = default;
};

// Fixed-point reconstruction of the grounded threshold/deceleration branch
// in FUN_0049df00. The separate brake-mode state machine remains outside this
// producer; this class only owns the observed slope threshold, component
// braking, and state-7 stop condition.
class GroundBrake final {
public:
    [[nodiscard]] static GroundBrakeResult apply(
        const GroundBrakeInput& input) noexcept;
};

} // namespace opentony::runtime
