#pragma once

#include "action_commands.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace opentony::runtime {

// FUN_00491c90 stores changed action states in a 32-entry ring at skater
// +0x2a14. The native record intentionally uses semantic names rather than
// reproducing the surrounding skater allocation.
struct RetailActionHistoryRecord final {
    std::uint8_t action{};
    bool pressed{};
    std::uint32_t timestamp{};
};

class RetailActionHistory final {
public:
    static constexpr std::size_t kCapacity = 0x20;

    void clear() noexcept;

    // Mirrors FUN_00491c90: a record is appended only when the action's
    // current pressed value changes. Action numbers are the retail 1..0x10
    // pad/action identifiers; 0 is reserved for an empty ring slot.
    [[nodiscard]] bool publish(
        std::uint8_t action,
        bool pressed,
        std::uint32_t timestamp) noexcept;

    [[nodiscard]] std::size_t write_index() const noexcept { return write_index_; }
    [[nodiscard]] const RetailActionHistoryRecord& record(
        std::size_t index) const noexcept;

    // Port of FUN_00492560. Direction zero includes `start`; direction one
    // starts at the previous ring entry and stops before `stop`.
    [[nodiscard]] int find_pressed(
        std::size_t start,
        int direction,
        std::uint16_t max_age,
        std::size_t stop,
        bool skip_special,
        std::uint32_t now) const noexcept;

    // Port of FUN_00492030's consumption side effect: clear the most recent
    // matching action/pressed record without changing the ring cursor.
    void consume(std::uint8_t action, bool pressed) noexcept;

private:
    std::array<RetailActionHistoryRecord, kCapacity> records_{};
    std::array<bool, 0x11> current_values_{};
    std::size_t write_index_{};
};

// The generated per-player sequence table is a short stream synthesized by
// FUN_004bcf00. Each record is laid out as:
//
//   [length][action_0 ... action_(length-1)][stream_relative][flags]
//
// where all fields are little-endian signed/unsigned 16-bit words. The next
// record begins immediately after flags; length zero terminates the table.
struct ActionSequenceTableRecord final {
    std::uint16_t length{};
    std::array<std::uint16_t, 12> actions{};
    std::int16_t stream_relative{};
    std::uint16_t flags{};
    std::size_t byte_offset{};
};

[[nodiscard]] std::optional<ActionSequenceTableRecord> read_action_sequence_record(
    std::span<const std::uint8_t> table,
    std::size_t byte_offset) noexcept;

struct ActionSequenceMatcherInput final {
    std::uint8_t selected_action{};
    std::uint32_t now{};
    std::uint32_t player_lock_value{};
    std::int32_t player_mode{};
};

struct ActionSequenceMatchResult final {
    bool matched{};
    std::int16_t stream_relative{};
    std::uint8_t trigger_action{};
    std::uint16_t flags{};
    std::uint32_t matched_timestamp{};
    std::size_t table_byte_offset{};
    std::size_t history_index{};
};

struct ActionSequenceExecutionResult final {
    ActionSequenceMatchResult match{};
    ActionStreamDispatchResult stream{};
    bool stream_resolved{};
    bool stream_started{};
    bool stream_resumed{};
    bool stream_active{};
};

// Bounded port of FUN_004925e0's sequence/history matcher. It scans records
// in table order and returns the first unambiguous match. The caller owns the
// action-stream asset and can resolve stream_relative through
// assets::TricksBinView::action_stream().
[[nodiscard]] ActionSequenceMatchResult match_action_sequence(
    std::span<const std::uint8_t> table,
    RetailActionHistory& history,
    const ActionSequenceMatcherInput& input,
    std::size_t max_records = 256) noexcept;

} // namespace opentony::runtime
