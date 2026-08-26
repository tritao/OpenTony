#include "gap_table.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace opentony::trg {
namespace {

[[nodiscard]] std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 2) {
        throw FormatError("gap table record is truncated");
    }
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset]))
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U));
}

[[nodiscard]] std::int16_t read_i16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int16_t>(read_u16(bytes, offset));
}

} // namespace

GapTable GapTable::parse(std::span<const std::byte> records) {
    std::vector<TriggerGapDefinition> definitions;
    constexpr std::size_t record_size = 0x2c;
    for (std::size_t offset = 0; offset < records.size(); offset += record_size) {
        if (records.size() - offset < record_size) {
            throw FormatError("gap table has a partial record");
        }
        const std::uint16_t divider = read_u16(records, offset + 4);
        if (divider == 0xffffU) {
            return GapTable(std::move(definitions));
        }
        std::string name;
        for (std::size_t index = 0; index < 36; ++index) {
            const std::uint8_t value = std::to_integer<std::uint8_t>(records[offset + 8 + index]);
            if (value == 0) {
                break;
            }
            name.push_back(static_cast<char>(value));
        }
        definitions.push_back(TriggerGapDefinition{
            read_u16(records, offset),
            divider,
            read_i16(records, offset + 6),
            std::move(name),
        });
    }
    throw FormatError("gap table has no 0xffff terminator");
}

GapTable GapTable::from_definitions(std::vector<TriggerGapDefinition> definitions) {
    return GapTable(std::move(definitions));
}

const TriggerGapDefinition* GapTable::find(std::uint16_t divider) const noexcept {
    const auto found = std::find_if(
        definitions_.begin(),
        definitions_.end(),
        [divider](const TriggerGapDefinition& definition) {
            return definition.divider == divider;
        });
    return found == definitions_.end() ? nullptr : &*found;
}

} // namespace opentony::trg
