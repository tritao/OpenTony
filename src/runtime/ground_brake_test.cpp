#include "ground_brake.hpp"

#include "tests/test_check.hpp"
#include <cstdint>
#include <iostream>

using opentony::runtime::FixedPosition;
using opentony::runtime::GroundBrake;
using opentony::runtime::GroundBrakeInput;
using opentony::runtime::GroundBrakeResult;

int main() {
    const GroundBrakeResult unused{};
    static_cast<void>(unused);

    const auto flat = GroundBrake::apply(GroundBrakeInput{
        FixedPosition{0x20000, 0, 0},
        -1,
        -0x1000,
        0x100,
        0,
        0,
        true,
    });
    CHECK(flat.speed_threshold == 0xa000);
    CHECK(flat.magnitude_q12 == 0x800);
    CHECK(flat.speed_metric == 0x20000);
    CHECK(flat.decelerated);
    CHECK(flat.response == FixedPosition({0x1e000, 0, 0}));
    CHECK(!flat.requested_state7);

    const auto parameterized = GroundBrake::apply(GroundBrakeInput{
        FixedPosition{0x10000, -0x10000, 0x08000},
        0x18000,
        -0x1000,
        0x80,
        0x10,
        0,
        true,
    });
    CHECK(parameterized.decelerated);
    // brake_parameter 0x10 gives factor one, then Q8 frame scaling halves it.
    CHECK(parameterized.response == FixedPosition({0x0ff00, -0x0ff00, 0x07f80}));

    const auto stopped = GroundBrake::apply(GroundBrakeInput{
        FixedPosition{0, 0, 0},
        0,
        -0x1000,
        0x100,
        0,
        0,
        true,
    });
    CHECK(stopped.stopped);
    CHECK(stopped.requested_state7);
    CHECK(stopped.response == FixedPosition({0, 0, 0}));

    const auto ineligible = GroundBrake::apply(GroundBrakeInput{
        FixedPosition{0x20000, 0, 0},
        0x20000,
        -0x1000,
        0x100,
        0,
        0,
        false,
    });
    CHECK(!ineligible.decelerated);
    CHECK(!ineligible.requested_state7);
    CHECK(ineligible.response == FixedPosition({0x20000, 0, 0}));

    std::cout << "ground brake tests passed\n";
}
