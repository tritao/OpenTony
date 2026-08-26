#include "platform_response.hpp"

namespace opentony::runtime {

FixedPosition PlatformResponse::bouncy_velocity(
    std::int32_t platform_type,
    const FixedPosition& source_vector,
    std::int32_t source_magnitude_q12) noexcept {
    switch (platform_type) {
    case 2:
    case 4:
    case 5:
        // FUN_0049f4c0 uses (component >> 1) + component for these
        // platform types, preserving the arithmetic shift on negatives.
        return FixedPosition{
            source_vector[0] + (source_vector[0] >> 1),
            -0x50000,
            source_vector[2] + (source_vector[2] >> 1),
        };
    case 1:
    case 3:
        return FixedPosition{
            source_vector[0],
            source_magnitude_q12 < 0x28 ? -0x20000 : -0x40000,
            source_vector[2],
        };
    default:
        // The retail diagnostic fallback keeps X/Z and uses -0x40000 for Y.
        return FixedPosition{
            source_vector[0],
            -0x40000,
            source_vector[2],
        };
    }
}

} // namespace opentony::runtime
