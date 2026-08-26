#pragma once

#include "position_commit.hpp"

#include <cstdint>

namespace opentony::runtime {

// Values recovered from the platform object's +0x1f2 type field in
// FUN_0049f4c0. Unknown values retain the retail fallback response.
class PlatformResponse final {
public:
    [[nodiscard]] static FixedPosition bouncy_velocity(
        std::int32_t platform_type,
        const FixedPosition& source_vector,
        std::int32_t source_magnitude_q12) noexcept;
};

} // namespace opentony::runtime
