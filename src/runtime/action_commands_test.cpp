#include "action_commands.hpp"

#include "tests/test_check.hpp"
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
    CHECK(dispatched.recognized);
    CHECK(dispatched.opcode == kSetQueuedMotionOpcode);
    CHECK(!dispatched.malformed);
    CHECK(dispatched.bytes_consumed == 7);
    CHECK(cursor == command.size());
    CHECK(motion.pending[1] == 0x10);
    CHECK(motion.rate[1] == 4);

    std::size_t wait_cursor = 0;
    const std::vector<std::uint8_t> wait_command{kWaitQueuedMotionOpcode};
    const auto waiting = dispatch_action_command(
        wait_command,
        wait_cursor,
        motion);
    CHECK(waiting.recognized);
    CHECK(waiting.yielded);
    CHECK(wait_cursor == 0);

    const auto first = drain_queued_motion(motion);
    CHECK(first.moved);
    CHECK(first.local_delta[1] == 4);
    CHECK(motion.pending[1] == 0xc);
    CHECK(motion.accumulated[1] == 4);

    const auto second = drain_queued_motion(motion, 0x80);
    CHECK(second.local_delta[1] == 2);
    CHECK(motion.pending[1] == 0xa);
    CHECK(motion.accumulated[1] == 6);

    while (motion.pending[1] != 0) {
        static_cast<void>(drain_queued_motion(motion));
    }
    wait_cursor = 0;
    const auto released = dispatch_action_command(
        wait_command,
        wait_cursor,
        motion);
    CHECK(released.recognized);
    CHECK(!released.yielded);
    CHECK(released.bytes_consumed == 1);
    CHECK(wait_cursor == 1);

    QueuedMotionState reverse{};
    CHECK(set_queued_motion_command(reverse, 0, 3, -4));
    const auto reverse_delta = drain_queued_motion(reverse);
    CHECK(reverse_delta.local_delta[0] == -3);
    CHECK(reverse.pending[0] == 0);

    CHECK(set_queued_motion_command(reverse, 0, 10, 0));
    CHECK(reverse.pending[0] == 0);
    CHECK(reverse.accumulated[0] == 0);
    CHECK(!set_queued_motion_command(reverse, 3, 1, 1));

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
        CHECK(raw_result.recognized);
        CHECK(!raw_result.malformed);
    }
    CHECK(raw_state.word_f4 == 0);
    CHECK(raw_state.word_f6 == 0x1234);
    CHECK(raw_state.word_29c0 == -2);
    CHECK(raw_state.word_2c64 == 0x5678);
    CHECK(raw_state.mode_f8 == 4);
    CHECK(raw_state.dword_29ec == 0x1234);
    CHECK(raw_state.dword_2f00 == -2);
    CHECK(raw_state.dword_2c0c == 0x5678);
    CHECK(raw_state.dword_2e2c == 0x8000);

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
    CHECK(response_result.recognized);
    CHECK(!response_result.malformed);
    CHECK(response_result.bytes_consumed == 7);
    CHECK(cursor == response_command.size());
    const std::array<std::int32_t, 3> expected_response{
        0x2000,
        -0x1000,
        0x10000,
    };
    CHECK(response == expected_response);

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
    CHECK(fixed_unknown.opcode == 0x01);
    CHECK(fixed_unknown.recognized);
    CHECK(!fixed_unknown.malformed);
    CHECK(fixed_unknown.bytes_consumed == 3);
    CHECK(cursor == 3);
    cursor = 0;
    const auto fixed_unknown_run = run_action_stream(
        fixed_unknown_stream,
        cursor,
        reverse);
    CHECK(fixed_unknown_run.completed);
    CHECK(fixed_unknown_run.commands_executed == 2);
    CHECK(cursor == fixed_unknown_stream.size());

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
    CHECK(string_unknown.opcode == 0x0b);
    CHECK(!string_unknown.recognized);
    CHECK(!string_unknown.malformed);
    CHECK(string_unknown.bytes_consumed == 4);
    CHECK(cursor == 4);

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
    CHECK(malformed_response_result.malformed);
    CHECK(cursor == 0);

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
    CHECK(stream_result.completed);
    CHECK(!stream_result.malformed);
    CHECK(stream_result.commands_executed == 2);
    CHECK(cursor == action_stream.size());
    const std::array<std::int32_t, 3> expected_stream_response{
        0x1000,
        0x2000,
        0x3000,
    };
    CHECK(response == expected_stream_response);

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
    CHECK(yielding_result.yielded);
    CHECK(!yielding_result.completed);
    CHECK(yielding_result.commands_executed == 2);
    CHECK(cursor == 7);

    QueuedMotionState malformed_motion{};
    const std::vector<std::uint8_t> malformed{
        kSetQueuedMotionOpcode, 0x00, 0x00,
    };
    cursor = 0;
    const auto malformed_result = dispatch_action_command(
        malformed,
        cursor,
        malformed_motion);
    CHECK(malformed_result.malformed);
    CHECK(cursor == 0);

    return 0;
}
