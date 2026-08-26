#include "powerup_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

void put16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    put16(bytes, offset, static_cast<std::uint16_t>(value));
    put16(bytes, offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

} // namespace

int main() {
    // A type-5 node with zero payload words: its aligned position begins at
    // node +8 and the node ends immediately after the three coordinates.
    std::vector<std::byte> bytes(42, std::byte{0});
    put32(bytes, 0, 0x4752545fU);
    put32(bytes, 4, 2);
    put32(bytes, 8, 2);
    put32(bytes, 12, 20);
    put32(bytes, 16, 40);
    put16(bytes, 20, 5);
    put16(bytes, 22, 6);
    put16(bytes, 24, 0);
    put32(bytes, 28, 7);
    put32(bytes, 32, 8);
    put32(bytes, 36, 9);
    put16(bytes, 40, 0xff);

    const opentony::trg::TrgFile file = opentony::trg::TrgFile::parse(bytes);
    opentony::trg::PowerupRuntimeList list;
    list.build(file);
    CHECK(list.records().size() == 1);
    const auto* record = list.record_for_node(0);
    CHECK(record != nullptr);
    CHECK(record->subtype() == 6);
    CHECK(record->resource() == "items");
    CHECK(record->model_name_checksum() == 0x2ebf22caU);
    CHECK(!record->model_index().has_value());
    const std::array<std::int32_t, 3> expected_position{7 << 12, 8 << 12, 9 << 12};
    CHECK(record->position() == expected_position);
    CHECK(record->raw_record().size() == opentony::trg::kPowerupRuntimeRecordSize);
    CHECK(record->raw_record()[0xb0] == std::byte{0});
    CHECK(record->raw_record()[0xb1] == std::byte{0});
    return 0;
}
