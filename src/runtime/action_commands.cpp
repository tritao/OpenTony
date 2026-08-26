#include "action_commands.hpp"

#include <array>
#include <optional>

namespace opentony::runtime {
namespace {

// FUN_004bf6c0 indexes this exact 0x59-byte table with opcode - 1. The
// values select one of ten small cursor-advance bodies in the retail binary;
// preserving the table lets recognized and not-yet-recognized commands share
// the real stream layout.
constexpr std::array<std::uint8_t, 0x59> kRetailActionWidthClass{
    0x00, 0x01, 0x01, 0x09, 0x09, 0x09, 0x01, 0x09,
    0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x01, 0x01, 0x00, 0x01, 0x04, 0x00,
    0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x01, 0x00, 0x04, 0x03,
    0x05, 0x00, 0x01, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x04, 0x04,
    0x00, 0x01, 0x00, 0x06, 0x01, 0x00, 0x07, 0x02,
    0x00, 0x08, 0x00, 0x00, 0x08, 0x01, 0x01, 0x00,
    0x08,
};

[[nodiscard]] std::int16_t read_little_endian_i16(
    std::span<const std::uint8_t> stream,
    std::size_t offset) noexcept {
    const std::uint16_t raw = static_cast<std::uint16_t>(stream[offset])
        | static_cast<std::uint16_t>(stream[offset + 1]) << 8;
    return static_cast<std::int16_t>(raw);
}

} // namespace

std::optional<std::size_t> retail_action_command_width(
    std::span<const std::uint8_t> stream,
    std::size_t cursor) noexcept {
    if (cursor >= stream.size()) {
        return std::nullopt;
    }

    const std::uint8_t opcode = stream[cursor];
    if (opcode == 0 || opcode > kRetailActionWidthClass.size()) {
        return std::size_t{1};
    }

    const std::uint8_t width_class = kRetailActionWidthClass[opcode - 1];
    switch (width_class) {
    case 0:
        return stream.size() - cursor >= 3 ? std::optional<std::size_t>(3) : std::nullopt;
    case 1:
        return std::size_t{1};
    case 2:
        for (std::size_t offset = cursor + 1; offset < stream.size(); ++offset) {
            if (stream[offset] == 0) {
                return offset - cursor + 1;
            }
        }
        return std::nullopt;
    case 3:
        return stream.size() - cursor >= 7 ? std::optional<std::size_t>(7) : std::nullopt;
    case 4:
        return stream.size() - cursor >= 5 ? std::optional<std::size_t>(5) : std::nullopt;
    case 5: {
        if (stream.size() - cursor < 3) {
            return std::nullopt;
        }
        const std::int16_t count = read_little_endian_i16(stream, cursor + 1);
        const std::int64_t next = static_cast<std::int64_t>(cursor) + 3
            + static_cast<std::int64_t>(count) * 2;
        if (next <= static_cast<std::int64_t>(cursor)
            || next > static_cast<std::int64_t>(stream.size())) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(next - static_cast<std::int64_t>(cursor));
    }
    case 6:
        return stream.size() - cursor >= 4 ? std::optional<std::size_t>(4) : std::nullopt;
    case 7: {
        constexpr std::size_t kStringOffset = 10;
        if (stream.size() - cursor < kStringOffset) {
            return std::nullopt;
        }
        for (std::size_t offset = cursor + kStringOffset; offset < stream.size(); ++offset) {
            if (stream[offset] == 0) {
                return offset - cursor + 1;
            }
        }
        return std::nullopt;
    }
    case 8:
        return stream.size() - cursor >= 2 ? std::optional<std::size_t>(2) : std::nullopt;
    default:
        // Class 9 is FUN_004bf6c0's diagnostic fallback. It advances one
        // byte after reporting the unsupported opcode.
        return std::size_t{1};
    }
}

ActionCommandDispatchResult dispatch_action_command(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    QueuedMotionState& motion,
    std::array<std::int32_t, 3>* response_vector,
    ActionCommandRuntimeState* runtime_state) noexcept {
    const std::size_t start = cursor;
    ActionCommandDispatchResult result{};
    result.opcode_offset = start;
    if (cursor >= stream.size()) {
        result.malformed = true;
        return result;
    }

    const std::uint8_t opcode = stream[cursor++];
    result.opcode = opcode;
    if (opcode == kEndActionStreamOpcode) {
        result.bytes_consumed = 1;
        result.recognized = true;
        result.completed = true;
        return result;
    }
    if (opcode == kWaitQueuedMotionOpcode) {
        const bool pending = motion.pending[0] != 0
            || motion.pending[1] != 0
            || motion.pending[2] != 0;
        result.recognized = true;
        if (pending) {
            // Retail 0x004bedc2 reaches 0x004bf544, which decrements the
            // stream pointer after the opcode has already been consumed.
            // The next invocation therefore retries this barrier.
            cursor = start;
            result.yielded = true;
            return result;
        }
        result.bytes_consumed = 1;
        return result;
    }
    if (opcode == 0x01 || opcode == 0x0a || opcode == 0x0e) {
        if (stream.size() - cursor < sizeof(std::int16_t)) {
            cursor = start;
            result.malformed = true;
            return result;
        }
        const std::int16_t value = read_little_endian_i16(stream, cursor);
        cursor += sizeof(std::int16_t);
        if (runtime_state != nullptr) {
            if (opcode == 0x01) {
                // FUN_004be47e writes the argument to +0xf6 and clears the
                // adjacent +0xf4 word from the dispatcher's zero register.
                runtime_state->word_f4 = 0;
                runtime_state->word_f6 = value;
            } else if (opcode == 0x0a) {
                // FUN_004be497 writes a signed 16-bit value to +0x29c0.
                runtime_state->word_29c0 = value;
            } else {
                // FUN_004be65b writes a signed 16-bit value to +0x2c64.
                runtime_state->word_2c64 = value;
            }
        }
        result.bytes_consumed = cursor - start;
        result.recognized = true;
        return result;
    }
    if (opcode == 0x13 || opcode == 0x14 || opcode == 0x1c) {
        if (runtime_state != nullptr) {
            // FUN_004be792/0x004be79e/0x004be8d8 assign 2/0/4 to +0xf8.
            runtime_state->mode_f8 = opcode == 0x13
                ? 2
                : (opcode == 0x14 ? 0 : 4);
        }
        result.bytes_consumed = 1;
        result.recognized = true;
        return result;
    }
    if (opcode == 0x1f || opcode == 0x23 || opcode == 0x29 || opcode == 0x2d) {
        if (stream.size() - cursor < sizeof(std::int16_t)) {
            cursor = start;
            result.malformed = true;
            return result;
        }
        const std::int32_t value = read_little_endian_i16(stream, cursor);
        cursor += sizeof(std::int16_t);
        if (runtime_state != nullptr) {
            // Direct FUN_004be450 cases at 0x004be91f, 0x004bebe8,
            // 0x004bed64, and 0x004bebff. The last case applies the retail
            // signed integer absolute-value sequence.
            if (opcode == 0x1f) {
                runtime_state->dword_29ec = value;
            } else if (opcode == 0x23) {
                runtime_state->dword_2f00 = value;
            } else if (opcode == 0x29) {
                runtime_state->dword_2c0c = value;
            } else {
                runtime_state->dword_2e2c = value < 0 ? -value : value;
            }
        }
        result.bytes_consumed = cursor - start;
        result.recognized = true;
        return result;
    }
    if (opcode != kSetQueuedMotionOpcode) {
        if (opcode == kSetResponseVectorOpcode) {
            constexpr std::size_t kArgumentBytes = 3 * sizeof(std::int16_t);
            if (stream.size() - cursor < kArgumentBytes) {
                cursor = start;
                result.malformed = true;
                return result;
            }

            for (std::size_t axis = 0; axis < 3; ++axis) {
                const std::int16_t value = read_little_endian_i16(
                    stream,
                    cursor + axis * sizeof(std::int16_t));
                if (response_vector != nullptr) {
                    // Retail case 0x0f performs a signed 16-bit read and a
                    // left shift by twelve before writing +0x4c/+0x50/+0x54.
                    (*response_vector)[axis] =
                        static_cast<std::int32_t>(value) << 12;
                }
            }
            cursor += kArgumentBytes;
            result.bytes_consumed = cursor - start;
            result.recognized = true;
            return result;
        }
        const std::optional<std::size_t> width = retail_action_command_width(
            stream,
            start);
        if (!width.has_value()) {
            cursor = start;
            result.malformed = true;
            return result;
        }
        cursor = start + *width;
        result.bytes_consumed = *width;
        return result;
    }

    constexpr std::size_t kArgumentBytes = 3 * sizeof(std::int16_t);
    if (stream.size() - cursor < kArgumentBytes) {
        cursor = start;
        result.malformed = true;
        return result;
    }

    const std::int16_t axis = read_little_endian_i16(stream, cursor);
    const std::int16_t amount = read_little_endian_i16(stream, cursor + 2);
    const std::int16_t rate = read_little_endian_i16(stream, cursor + 4);
    cursor += kArgumentBytes;
    result.bytes_consumed = cursor - start;
    result.recognized = set_queued_motion_command(
        motion,
        axis,
        amount,
        rate);
    return result;
}

ActionStreamDispatchResult run_action_stream(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    QueuedMotionState& motion,
    std::array<std::int32_t, 3>* response_vector,
    std::size_t max_commands,
    ActionCommandRuntimeState* runtime_state) noexcept {
    ActionStreamDispatchResult result{};
    while (result.commands_executed < max_commands) {
        const ActionCommandDispatchResult command = dispatch_action_command(
            stream,
            cursor,
            motion,
            response_vector,
            runtime_state);
        ++result.commands_executed;
        result.yielded = command.yielded;
        result.completed = command.completed;
        result.malformed = command.malformed;
        if (command.yielded || command.completed || command.malformed) {
            return result;
        }
    }
    result.budget_exhausted = true;
    return result;
}

} // namespace opentony::runtime
