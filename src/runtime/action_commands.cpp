#include "action_commands.hpp"

namespace opentony::runtime {
namespace {

[[nodiscard]] std::int16_t read_little_endian_i16(
    std::span<const std::uint8_t> stream,
    std::size_t offset) noexcept {
    const std::uint16_t raw = static_cast<std::uint16_t>(stream[offset])
        | static_cast<std::uint16_t>(stream[offset + 1]) << 8;
    return static_cast<std::int16_t>(raw);
}

} // namespace

ActionCommandDispatchResult dispatch_action_command(
    std::span<const std::uint8_t> stream,
    std::size_t& cursor,
    QueuedMotionState& motion) noexcept {
    const std::size_t start = cursor;
    ActionCommandDispatchResult result{};
    result.opcode_offset = start;
    if (cursor >= stream.size()) {
        result.malformed = true;
        return result;
    }

    const std::uint8_t opcode = stream[cursor++];
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
    if (opcode != kSetQueuedMotionOpcode) {
        result.bytes_consumed = 1;
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

} // namespace opentony::runtime
