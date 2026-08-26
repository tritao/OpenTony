#pragma once

#include "queued_motion.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace opentony::runtime {

constexpr std::uint8_t kSetQueuedMotionOpcode = 0x2b;
constexpr std::uint8_t kWaitQueuedMotionOpcode = 0x2c;

struct ActionCommandDispatchResult final {
    std::size_t opcode_offset{};
    std::size_t bytes_consumed{};
    bool recognized{};
    bool malformed{};
    bool yielded{};

    friend bool operator==(
        const ActionCommandDispatchResult&,
        const ActionCommandDispatchResult&) = default;
};

// Reads one retail FUN_004be450 command from the byte stream. The stream
// cursor points at the opcode on entry and advances over the opcode and its
// arguments only when the command is structurally complete. Unknown opcodes
// are reported without trying to assign semantics to the remaining 0x58
// retail cases.
[[nodiscard]] ActionCommandDispatchResult dispatch_action_command(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    QueuedMotionState& motion) noexcept;

} // namespace opentony::runtime
