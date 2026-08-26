#pragma once

#include "trg_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace opentony::trg {

struct TriggerGapDefinition {
    std::uint16_t flags{};
    std::uint16_t divider{};
    std::int16_t score{};
    std::string name;
};

// The PC executable uses 0x2c-byte records. The first four fields are two
// flag/unknown words, a divider ID, and a signed score; the remaining 36 bytes
// are the display name. A divider of 0xffff terminates a table.
class GapTable final {
public:
    static GapTable parse(std::span<const std::byte> records);
    static GapTable from_definitions(std::vector<TriggerGapDefinition> definitions);

    [[nodiscard]] const TriggerGapDefinition* find(std::uint16_t divider) const noexcept;
    [[nodiscard]] const std::vector<TriggerGapDefinition>& definitions() const noexcept {
        return definitions_;
    }

private:
    explicit GapTable(std::vector<TriggerGapDefinition> definitions)
        : definitions_(std::move(definitions)) {}

    std::vector<TriggerGapDefinition> definitions_;
};

// This is the retail general/warehouse checklist table selected by the PC
// level loader for the Warehouse scripts. Career/editor tables remain
// loadable through GapTable::parse until their mode-selection boundary is
// connected to the native career runtime.
[[nodiscard]] const GapTable& retail_warehouse_gap_table();

} // namespace opentony::trg
