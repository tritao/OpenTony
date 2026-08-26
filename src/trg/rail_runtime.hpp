#pragma once

#include "trg_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opentony::trg {

class LevelTriggerState;

inline constexpr std::size_t kRailRuntimeRecordMinimumSize = 0x28;

// The rail constructor's allocation size is not yet independently proven,
// but its next-link read at +0x24 proves that the record reaches 0x28 bytes.
// Keep a minimum raw record and expose only the fields proven by constructor,
// pulse, and kill consumers.
class RailRuntimeRecord final {
public:
    [[nodiscard]] std::size_t source_node() const noexcept { return source_node_; }
    [[nodiscard]] std::uint16_t trigger_word() const noexcept { return trigger_word_; }
    [[nodiscard]] std::uint8_t state() const noexcept {
        return std::to_integer<std::uint8_t>(raw_[0x04]);
    }
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept { return raw_; }
    void set_state(std::uint8_t state) noexcept { raw_[0x04] = static_cast<std::byte>(state); }

private:
    friend class RailRuntimeList;
    std::array<std::byte, kRailRuntimeRecordMinimumSize> raw_{};
    std::size_t source_node_{};
    std::uint16_t trigger_word_{};
};

class RailRuntimeList final {
public:
    void build(const TrgFile& file);
    void synchronize_states(const LevelTriggerState& state);
    void pulse_node(std::size_t node);
    void kill_node(std::size_t node);

    [[nodiscard]] const std::vector<RailRuntimeRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] const RailRuntimeRecord* record_for_node(
        std::size_t node) const noexcept;

private:
    std::vector<RailRuntimeRecord> records_;
};

} // namespace opentony::trg
