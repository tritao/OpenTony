#include "action_sequence.hpp"

#include <algorithm>

namespace opentony::runtime {
namespace {

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::uint16_t>(bytes[offset])
        | static_cast<std::uint16_t>(bytes[offset + 1]) << 8U;
}

[[nodiscard]] std::int16_t read_i16(
    std::span<const std::uint8_t> bytes,
    std::size_t offset) noexcept {
    return static_cast<std::int16_t>(read_u16(bytes, offset));
}

[[nodiscard]] bool is_primary_action(std::uint16_t action) noexcept {
    return action >= 1 && action <= 8;
}

[[nodiscard]] std::size_t previous_index(std::size_t index) noexcept {
    return index == 0 ? RetailActionHistory::kCapacity - 1 : index - 1;
}

void consume_sequence(
    RetailActionHistory& history,
    const ActionSequenceTableRecord& record) noexcept {
    for (std::size_t index = 0; index < record.length; ++index) {
        history.consume(
            static_cast<std::uint8_t>(record.actions[index]),
            true);
    }
}

} // namespace

void RetailActionHistory::clear() noexcept {
    records_ = {};
    current_values_ = {};
    write_index_ = 0;
}

bool RetailActionHistory::publish(
    std::uint8_t action,
    bool pressed,
    std::uint32_t timestamp) noexcept {
    if (action == 0 || action >= current_values_.size()) {
        return false;
    }
    if (current_values_[action] == pressed) {
        return false;
    }
    current_values_[action] = pressed;
    records_[write_index_] = RetailActionHistoryRecord{
        action,
        pressed,
        timestamp,
    };
    write_index_ = (write_index_ + 1) % kCapacity;
    return true;
}

const RetailActionHistoryRecord& RetailActionHistory::record(
    std::size_t index) const noexcept {
    static const RetailActionHistoryRecord empty{};
    return index < kCapacity ? records_[index] : empty;
}

int RetailActionHistory::find_pressed(
    std::size_t start,
    int direction,
    std::uint16_t max_age,
    std::size_t stop,
    bool skip_special,
    std::uint32_t now) const noexcept {
    if (start >= kCapacity || stop >= kCapacity || (direction != 0 && direction != 1)) {
        return -1;
    }

    std::size_t index = direction == 0 ? start : previous_index(start);
    while (true) {
        if (direction == 1 && index == stop) {
            return -1;
        }
        const RetailActionHistoryRecord& candidate = records_[index];
        if (candidate.action != 0
            && candidate.pressed
            && !(skip_special && candidate.action >= 5 && candidate.action <= 8)) {
            if (now - candidate.timestamp > max_age) {
                // FUN_00492560 returns immediately once the backward scan
                // reaches an entry outside the age window.
                return -1;
            }
            return static_cast<int>(index);
        }
        index = previous_index(index);
        if (index == stop) {
            return -1;
        }
    }
}

void RetailActionHistory::consume(
    std::uint8_t action,
    bool pressed) noexcept {
    if (action == 0 || action >= current_values_.size()) {
        return;
    }
    std::size_t index = previous_index(write_index_);
    while (index != write_index_) {
        const RetailActionHistoryRecord& candidate = records_[index];
        if (candidate.action == action && candidate.pressed == pressed) {
            records_[index].action = 0;
            return;
        }
        index = previous_index(index);
    }
}

std::optional<ActionSequenceTableRecord> read_action_sequence_record(
    std::span<const std::uint8_t> table,
    std::size_t byte_offset) noexcept {
    if (byte_offset > table.size() || table.size() - byte_offset < 2) {
        return std::nullopt;
    }
    const std::int16_t signed_length = read_i16(table, byte_offset);
    if (signed_length == 0) {
        return ActionSequenceTableRecord{
            0,
            {},
            0,
            0,
            byte_offset,
        };
    }
    if (signed_length < 0 || signed_length > 12) {
        return std::nullopt;
    }
    const std::size_t length = static_cast<std::size_t>(signed_length);
    const std::size_t record_bytes = (length + 3) * sizeof(std::uint16_t);
    if (table.size() - byte_offset < record_bytes) {
        return std::nullopt;
    }
    ActionSequenceTableRecord result{};
    result.length = static_cast<std::uint16_t>(length);
    result.byte_offset = byte_offset;
    for (std::size_t index = 0; index < length; ++index) {
        result.actions[index] = read_u16(table, byte_offset + (index + 1) * 2);
    }
    result.stream_relative = read_i16(table, byte_offset + (length + 1) * 2);
    result.flags = read_u16(table, byte_offset + (length + 2) * 2);
    return result;
}

ActionSequenceMatchResult match_action_sequence(
    std::span<const std::uint8_t> table,
    RetailActionHistory& history,
    const ActionSequenceMatcherInput& input,
    std::size_t max_records) noexcept {
    ActionSequenceMatchResult result{};
    std::size_t table_offset = 0;
    const std::size_t newest = previous_index(history.write_index());

    for (std::size_t record_number = 0; record_number < max_records;) {
        const auto parsed = read_action_sequence_record(table, table_offset);
        if (!parsed.has_value() || parsed->length == 0) {
            return result;
        }
        const ActionSequenceTableRecord& record = *parsed;
        ++record_number;
        table_offset += (static_cast<std::size_t>(record.length) + 3) * 2;

        const std::uint16_t flags = record.flags;
        if ((flags & 0x2000U) == 0
            && (input.player_mode < 1 || input.player_mode > 3)) {
            continue;
        }
        if ((flags & 0x8000U) != 0
            && (input.player_lock_value & 0xfffff000U) == 0) {
            continue;
        }

        const std::uint16_t max_age = flags & 0x03ffU;
        const bool skip_special = (flags & 0x0400U) != 0;
        const bool force_dispatch = (flags & 0x2000U) != 0;
        const int first = history.find_pressed(
            newest,
            0,
            max_age,
            newest,
            skip_special,
            input.now);
        if (first < 0) {
            // Retail returns from the whole matcher in this case for a
            // generic record. A primary/secondary special pair gets the
            // same behavior after its fallback checks below.
            return result;
        }

        const std::uint16_t older = record.actions[record.length - 2];
        const std::uint16_t newer = record.actions[record.length - 1];
        const int second = history.find_pressed(
            static_cast<std::size_t>(first),
            1,
            max_age,
            newest,
            skip_special,
            input.now);

        int selected_history = -1;
        bool matched = false;

        // The compact two-action path has the selected directional action as
        // a third implicit candidate when the first field is 1..8 and the
        // second field is a non-primary action. This is the branch beginning
        // at 0x0049275e.
        if (record.length == 2 && is_primary_action(older) && !is_primary_action(newer)) {
            if (second >= 0
                && history.record(static_cast<std::size_t>(first)).action == older
                && history.record(static_cast<std::size_t>(second)).action == newer) {
                selected_history = second;
                matched = true;
            } else if (second >= 0
                && history.record(static_cast<std::size_t>(second)).action == older
                && history.record(static_cast<std::size_t>(first)).action == newer) {
                selected_history = first;
                matched = true;
            } else if (input.selected_action == older
                && history.record(static_cast<std::size_t>(first)).action == newer) {
                selected_history = first;
                matched = true;
            } else if (input.selected_action == older
                && second >= 0
                && history.record(static_cast<std::size_t>(second)).action == newer) {
                selected_history = second;
                matched = true;
            }
        } else {
            const bool older_primary = is_primary_action(older);
            const bool newer_primary = is_primary_action(newer);
            const bool same_domain = older_primary == newer_primary;
            if (second >= 0
                && history.record(static_cast<std::size_t>(second)).action == older
                && history.record(static_cast<std::size_t>(first)).action == newer) {
                selected_history = first;
                matched = true;
            } else if (!same_domain && second >= 0
                && history.record(static_cast<std::size_t>(first)).action == older
                && history.record(static_cast<std::size_t>(second)).action == newer) {
                selected_history = second;
                matched = true;
            }

            if (matched) {
                int search_cursor = second;
                for (std::size_t extra = record.length - 2; extra > 0; --extra) {
                    const int prior = history.find_pressed(
                        static_cast<std::size_t>(search_cursor),
                        1,
                        max_age,
                        newest,
                        skip_special,
                        input.now);
                    if (prior < 0
                        || history.record(static_cast<std::size_t>(prior)).action
                            != record.actions[extra - 1]) {
                        matched = false;
                        break;
                    }
                    // FUN_004925e0 continues the backward search from the
                    // action just matched for the next preceding value.
                    search_cursor = prior;
                }
            }
        }

        if (!matched || selected_history < 0) {
            continue;
        }

        if (!force_dispatch) {
            consume_sequence(history, record);
        }
        result.matched = true;
        result.stream_relative = record.stream_relative;
        result.trigger_action = static_cast<std::uint8_t>(newer);
        result.flags = flags;
        result.matched_timestamp = history.record(
            static_cast<std::size_t>(selected_history)).timestamp;
        result.table_byte_offset = record.byte_offset;
        result.history_index = static_cast<std::size_t>(selected_history);
        return result;
    }
    return result;
}

} // namespace opentony::runtime
