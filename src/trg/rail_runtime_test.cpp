#include "rail_runtime.hpp"

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

void write_rail(std::vector<std::byte>& bytes, std::size_t offset,
    std::uint16_t type, std::uint16_t flags) {
    put16(bytes, offset, type);
    put16(bytes, offset + 2, 0);
    // With zero link words, FUN_004c8650 aligns begin+7 down to begin+4.
    put32(bytes, offset + 4, 1);
    put32(bytes, offset + 8, 2);
    put32(bytes, offset + 12, 3);
    put16(bytes, offset + 16, flags);
}

} // namespace

int main() {
    std::vector<std::byte> bytes(82, std::byte{0});
    put32(bytes, 0, 0x4752545fU);
    put32(bytes, 4, 2);
    put32(bytes, 8, 3);
    put32(bytes, 12, 24);
    put32(bytes, 16, 52);
    put32(bytes, 20, 80);
    write_rail(bytes, 24, 10, 0x1234);
    write_rail(bytes, 52, 11, 0x5678);
    put16(bytes, 80, 0xff);

    const opentony::trg::TrgFile file = opentony::trg::TrgFile::parse(bytes);
    opentony::trg::RailRuntimeList rails;
    rails.build(file);
    CHECK(rails.records().size() == 2);
    CHECK(rails.record_for_node(0) != nullptr);
    CHECK(rails.record_for_node(1) != nullptr);
    CHECK(rails.record_for_node(0)->trigger_word() == 0x1234);
    CHECK(rails.record_for_node(0)->state() == 0);
    CHECK(rails.record_for_node(2)->raw_record().size() ==
        opentony::trg::kRailRuntimeRecordMinimumSize);

    rails.pulse_node(0);
    CHECK(rails.record_for_node(0)->state() == 1);
    rails.kill_node(0);
    CHECK(rails.record_for_node(0)->state() == 0);
    CHECK(rails.record_for_node(1)->trigger_word() == 0x5678);

    return 0;
}
