#pragma once

#include "queued_motion.hpp"

#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <span>

namespace opentony::runtime {

constexpr std::uint8_t kSetQueuedMotionOpcode = 0x2b;
constexpr std::uint8_t kWaitQueuedMotionOpcode = 0x2c;
constexpr std::uint8_t kEndActionStreamOpcode = 0x07;
// FUN_004be450 case 0x0f: three signed 16-bit local response components,
// shifted from the action stream's coarse units into the player's Q12
// response vector at +0x4c/+0x50/+0x54.
constexpr std::uint8_t kSetResponseVectorOpcode = 0x0f;

// Raw action-object registers written directly by the unambiguous dispatcher
// cases below. The names retain the retail offsets because their higher-level
// animation/state meaning is not yet established.
struct ActionCommandRuntimeState final {
    std::int16_t word_f4{};
    std::int16_t word_f6{};
    std::int16_t word_29c0{};
    std::int16_t word_2c64{};
    std::uint8_t mode_f8{};
    // Direct signed-argument writes in the retail dispatcher. Their semantic
    // owners are still unresolved, so retain the retail offsets.
    std::int32_t dword_29ec{};
    std::int32_t dword_2f00{};
    std::int32_t dword_2c0c{};
    std::int32_t dword_2e2c{};
};

struct ActionCommandDispatchResult final {
    std::size_t opcode_offset{};
    std::uint8_t opcode{};
    std::size_t bytes_consumed{};
    bool recognized{};
    bool malformed{};
    bool yielded{};
    bool completed{};

    friend bool operator==(
        const ActionCommandDispatchResult&,
        const ActionCommandDispatchResult&) = default;
};

struct ActionStreamDispatchResult final {
    std::size_t commands_executed{};
    bool yielded{};
    bool completed{};
    bool malformed{};
    bool budget_exhausted{};

    friend bool operator==(
        const ActionStreamDispatchResult&,
        const ActionStreamDispatchResult&) = default;
};

// Returns the bounded cursor width recovered from FUN_004bf6c0 for the
// command at `cursor`. This is also used while building the retail resource
// records: FUN_004bb7e0 walks the same command stream to find opcode 0x51's
// filter metadata before the gameplay dispatcher runs it.
[[nodiscard]] std::optional<std::size_t> retail_action_command_width(
    std::span<const std::uint8_t> stream,
    std::size_t cursor) noexcept;

// Reads one retail FUN_004be450 command from the byte stream. The stream
// cursor points at the opcode on entry and advances over the opcode and its
// arguments only when the command is structurally complete. Unknown opcodes
// are reported without assigning gameplay semantics to the remaining retail
// cases, but retain FUN_004bf6c0's recovered cursor widths.
[[nodiscard]] ActionCommandDispatchResult dispatch_action_command(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    QueuedMotionState& motion,
    std::array<std::int32_t, 3>* response_vector = nullptr,
    ActionCommandRuntimeState* runtime_state = nullptr) noexcept;

// Executes the same bounded command loop as FUN_004be450. The retail loop
// returns on 0x2c while queued motion is pending and on 0x07 at stream end.
// Unknown commands retain the retail cursor width from FUN_004bf6c0, while
// their gameplay semantics remain unimplemented and are reported as such.
[[nodiscard]] ActionStreamDispatchResult run_action_stream(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    QueuedMotionState& motion,
    std::array<std::int32_t, 3>* response_vector = nullptr,
    std::size_t max_commands = 256,
    ActionCommandRuntimeState* runtime_state = nullptr) noexcept;

} // namespace opentony::runtime
