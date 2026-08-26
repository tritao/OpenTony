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
    assert(dispatched.opcode == kSetQueuedMotionOpcode);
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

    ActionCommandRuntimeState raw_state{};
    const std::vector<std::uint8_t> raw_register_stream{
        0x01, 0x34, 0x12,
        0x0a, 0xfe, 0xff,
        0x0e, 0x78, 0x56,
        0x13,
        0x14,
        0x1c,
        0x1f, 0x34, 0x12,
        0x23, 0xfe, 0xff,
        0x29, 0x78, 0x56,
        0x2d, 0x00, 0x80,
    };
    cursor = 0;
    for (int command_number = 0; command_number < 10; ++command_number) {
        const auto raw_result = dispatch_action_command(
            raw_register_stream,
            cursor,
            reverse,
            nullptr,
            &raw_state);
        assert(raw_result.recognized);
        assert(!raw_result.malformed);
    }
    assert(raw_state.word_f4 == 0);
    assert(raw_state.word_f6 == 0x1234);
    assert(raw_state.word_29c0 == -2);
    assert(raw_state.word_2c64 == 0x5678);
    assert(raw_state.mode_f8 == 4);
    assert(raw_state.dword_29ec == 0x1234);
    assert(raw_state.dword_2f00 == -2);
    assert(raw_state.dword_2c0c == 0x5678);
    assert(raw_state.dword_2e2c == 0x8000);

    const std::vector<std::uint8_t> response_command{
        kSetResponseVectorOpcode,
        0x02, 0x00,
        0xff, 0xff,
        0x10, 0x00,
    };
    std::array<std::int32_t, 3> response{};
    cursor = 0;
    const auto response_result = dispatch_action_command(
        response_command,
        cursor,
        reverse,
        &response);
    assert(response_result.recognized);
    assert(!response_result.malformed);
    assert(response_result.bytes_consumed == 7);
    assert(cursor == response_command.size());
    const std::array<std::int32_t, 3> expected_response{
        0x2000,
        -0x1000,
        0x10000,
    };
    assert(response == expected_response);

    // The now-recognized opcode 0x01 is a three-byte form; the runner must
    // reach the following terminator instead of treating its arguments as
    // opcodes.
    const std::vector<std::uint8_t> fixed_unknown_stream{
        0x01,
        0x12, 0x34,
        kEndActionStreamOpcode,
    };
    cursor = 0;
    const auto fixed_unknown = dispatch_action_command(
        fixed_unknown_stream,
        cursor,
        reverse);
    assert(fixed_unknown.opcode == 0x01);
    assert(fixed_unknown.recognized);
    assert(!fixed_unknown.malformed);
    assert(fixed_unknown.bytes_consumed == 3);
    assert(cursor == 3);
    cursor = 0;
    const auto fixed_unknown_run = run_action_stream(
        fixed_unknown_stream,
        cursor,
        reverse);
    assert(fixed_unknown_run.completed);
    assert(fixed_unknown_run.commands_executed == 2);
    assert(cursor == fixed_unknown_stream.size());

    // Opcode 0x0b uses the retail NUL-terminated string form beginning at
    // the byte after the opcode.
    const std::vector<std::uint8_t> string_unknown_stream{
        0x0b, 'a', 'b', 0,
        kEndActionStreamOpcode,
    };
    cursor = 0;
    const auto string_unknown = dispatch_action_command(
        string_unknown_stream,
        cursor,
        reverse);
    assert(string_unknown.opcode == 0x0b);
    assert(!string_unknown.recognized);
    assert(!string_unknown.malformed);
    assert(string_unknown.bytes_consumed == 4);
    assert(cursor == 4);

    const std::vector<std::uint8_t> malformed_response{
        kSetResponseVectorOpcode,
        0x00, 0x00,
    };
    cursor = 0;
    const auto malformed_response_result = dispatch_action_command(
        malformed_response,
        cursor,
        reverse,
        &response);
    assert(malformed_response_result.malformed);
    assert(cursor == 0);

    const std::vector<std::uint8_t> action_stream{
        kSetResponseVectorOpcode,
        0x01, 0x00,
        0x02, 0x00,
        0x03, 0x00,
        kEndActionStreamOpcode,
    };
    response = {};
    cursor = 0;
    const auto stream_result = run_action_stream(
        action_stream,
        cursor,
        reverse,
        &response);
    assert(stream_result.completed);
    assert(!stream_result.malformed);
    assert(stream_result.commands_executed == 2);
    assert(cursor == action_stream.size());
    const std::array<std::int32_t, 3> expected_stream_response{
        0x1000,
        0x2000,
        0x3000,
    };
    assert(response == expected_stream_response);

    const std::vector<std::uint8_t> yielding_stream{
        kSetQueuedMotionOpcode,
        0x00, 0x00,
        0x04, 0x00,
        0x01, 0x00,
        kWaitQueuedMotionOpcode,
    };
    cursor = 0;
    const auto yielding_result = run_action_stream(
        yielding_stream,
        cursor,
        reverse);
    assert(yielding_result.yielded);
    assert(!yielding_result.completed);
    assert(yielding_result.commands_executed == 2);
    assert(cursor == 7);

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
