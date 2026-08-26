#include "action_commands.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    using namespace opentony::runtime;

    // Retail opcode 0x2b: signed axis, amount, rate, all read through
    // FUN_004be3c0 as little-endian 16-bit values.
    const std::vector<std::uint8_t> command{
        kSetQueuedMotionOpcode,
        0x01, 0x00,
        0x10, 0x00,
        0x04, 0x00,
    };
    QueuedMotionState motion{};
    std::size_t cursor = 0;
    const auto dispatched = dispatch_action_command(command, cursor, motion);
    assert(dispatched.recognized);
    assert(!dispatched.malformed);
    assert(dispatched.bytes_consumed == 7);
    assert(cursor == command.size());
    assert(motion.pending[1] == 0x10);
    assert(motion.rate[1] == 4);

    std::size_t wait_cursor = 0;
    const std::vector<std::uint8_t> wait_command{kWaitQueuedMotionOpcode};
    const auto waiting = dispatch_action_command(
        wait_command,
        wait_cursor,
        motion);
    assert(waiting.recognized);
    assert(waiting.yielded);
    assert(wait_cursor == 0);

    const auto first = drain_queued_motion(motion);
    assert(first.moved);
    assert(first.local_delta[1] == 4);
    assert(motion.pending[1] == 0xc);
    assert(motion.accumulated[1] == 4);

    const auto second = drain_queued_motion(motion, 0x80);
    assert(second.local_delta[1] == 2);
    assert(motion.pending[1] == 0xa);
    assert(motion.accumulated[1] == 6);

    while (motion.pending[1] != 0) {
        static_cast<void>(drain_queued_motion(motion));
    }
    wait_cursor = 0;
    const auto released = dispatch_action_command(
        wait_command,
        wait_cursor,
        motion);
    assert(released.recognized);
    assert(!released.yielded);
    assert(released.bytes_consumed == 1);
    assert(wait_cursor == 1);

    QueuedMotionState reverse{};
    assert(set_queued_motion_command(reverse, 0, 3, -4));
    const auto reverse_delta = drain_queued_motion(reverse);
    assert(reverse_delta.local_delta[0] == -3);
    assert(reverse.pending[0] == 0);

    assert(set_queued_motion_command(reverse, 0, 10, 0));
    assert(reverse.pending[0] == 0);
    assert(reverse.accumulated[0] == 0);
    assert(!set_queued_motion_command(reverse, 3, 1, 1));

    QueuedMotionState malformed_motion{};
    const std::vector<std::uint8_t> malformed{
        kSetQueuedMotionOpcode, 0x00, 0x00,
    };
    cursor = 0;
    const auto malformed_result = dispatch_action_command(
        malformed,
        cursor,
        malformed_motion);
    assert(malformed_result.malformed);
    assert(cursor == 0);

    return 0;
}
