#include "trigger_factory_runtime.hpp"

#include <array>
#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

void put16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void put32(std::vector<std::byte>& bytes, std::size_t offset, std::int32_t value) {
    put16(bytes, offset, static_cast<std::uint16_t>(value));
    put16(bytes, offset + 2, static_cast<std::uint16_t>(
        static_cast<std::uint32_t>(value) >> 16U));
}

opentony::trg::TrgFile fixture(std::uint16_t subtype) {
    std::vector<std::byte> bytes(54, std::byte{0});
    bytes[0] = std::byte{'_'};
    bytes[1] = std::byte{'T'};
    bytes[2] = std::byte{'R'};
    bytes[3] = std::byte{'G'};
    put32(bytes, 4, 2);
    put32(bytes, 8, 2);
    put32(bytes, 12, 20);
    put32(bytes, 16, 52);
    put16(bytes, 20, 1);
    put16(bytes, 22, subtype);
    put16(bytes, 26, 0);
    bytes[28] = std::byte{0xff};
    put32(bytes, 32, 10);
    put32(bytes, 36, 20);
    put32(bytes, 40, -30);
    put16(bytes, 44, 0x1111);
    put16(bytes, 46, 0x2222);
    put16(bytes, 48, 0x3333);
    put16(bytes, 52, 0xff);
    return opentony::trg::TrgFile::parse(bytes);
}

} // namespace

int main() {
    const auto cb_file = fixture(0xcb);
    opentony::trg::TriggerFactoryRuntimeList cb_list;
    cb_list.build(cb_file);
    CHECK(cb_list.records().size() == 1);
    const auto& cb = cb_list.records().front();
    CHECK(cb.kind() == opentony::trg::TriggerFactoryRuntimeKind::ObjectCb);
    CHECK(cb.allocation_size() == 0x1f4);
    CHECK(cb.raw_record().size() == 0x1f4);
    CHECK(cb.subtype() == 0xcb);
    CHECK(cb.object_flags() == 0x41);
    CHECK(cb.active_byte() == 1);
    const std::array<std::int32_t, 3> expected_position{
        40960, 81920, -122880};
    CHECK(cb.position() == expected_position);
    const std::array<std::uint16_t, 3> expected_parameters{
        0x1111, 0x2222, 0x3333};
    CHECK(cb.constructor_parameters() == expected_parameters);
    CHECK(cb_list.record_for_node(0) != nullptr);
    CHECK(cb.activation_argument() == 0);
    CHECK(cb.activation_flags() == 0);
    CHECK(cb_list.activate_node(0, 0x10203040U));
    CHECK(cb.activation_argument() == 0x10203040U);
    CHECK((cb.activation_flags() & 1U) != 0);
    CHECK(cb_list.deactivate_node(0));
    CHECK((cb.activation_flags() & 1U) == 0);

    const auto object_file = fixture(0x192);
    opentony::trg::TriggerFactoryRuntimeList object_list;
    object_list.build(object_file);
    const auto& object = object_list.records().front();
    CHECK(object.kind() == opentony::trg::TriggerFactoryRuntimeKind::Object192);
    CHECK(object.allocation_size() == 0x218);
    CHECK(object.raw_record().size() == 0x218);
    CHECK(object.subtype() == 0x192);
    CHECK(object.object_flags() == 0x111);
    CHECK(object.active_byte() == 1);
    CHECK(object.mode_word() == 0x20);

    opentony::trg::LevelTriggerState state;
    state.on_spawn_node(0, 1, 0xcb, {10 * 4096, 20 * 4096, -30 * 4096}, {});
    cb_list.synchronize(state);
    CHECK(cb_list.records().front().object_flags() == 0x41);

    std::cout << "Trigger factory runtime tests passed\n";
}
