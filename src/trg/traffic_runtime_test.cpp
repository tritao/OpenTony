#include "traffic_runtime.hpp"

#include "tests/test_check.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
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

opentony::trg::TrgFile one_traffic_fixture(std::uint16_t subtype) {
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
    put32(bytes, 32, 1);
    put32(bytes, 36, 2);
    put32(bytes, 40, 3);
    put16(bytes, 44, 0x1111);
    put16(bytes, 46, 0x2222);
    put16(bytes, 48, 0x3333);
    put16(bytes, 52, 0xff);
    return opentony::trg::TrgFile::parse(bytes);
}

} // namespace

int main() {
    const opentony::trg::TrgFile file = one_traffic_fixture(0xd5);
    opentony::trg::TrafficRuntimeList list;
    list.build(file);
    CHECK(list.records().size() == 1);
    CHECK(list.regions().size() == 1);
    const auto& record = list.records().front();
    CHECK(record.source_node() == 0);
    CHECK(record.subtype() == 0xd5);
    CHECK(record.resource() == "c_taxi");
    CHECK(record.sound_id() == 0x71);
    CHECK(!record.sound_setup_disabled());
    CHECK(record.region_slot() == opentony::trg::kTrafficNoRegionSlot);
    CHECK(record.model_index() == 0);
    const std::array<std::int32_t, 3> expected_position{
        4096, 8192 - 0x6e000, 12288};
    CHECK(record.position() == expected_position);
    const std::array<std::uint16_t, 3> expected_parameters{
        0x1111, 0x2222, 0x3333};
    CHECK(record.constructor_parameters() == expected_parameters);
    CHECK(record.raw_record().size() == opentony::trg::kTrafficRuntimeRecordSize);
    CHECK(list.region(record.region_index()).resource == "c_taxi");
    CHECK(!list.region(record.region_index()).asset_available);
    CHECK(record.activation_argument() == 0);
    CHECK(record.activation_flags() == 0);
    CHECK(list.activate_node(0, 0x12345678U));
    CHECK(record.activation_argument() == 0x12345678U);
    CHECK((record.activation_flags() & 1U) != 0);
    CHECK(list.deactivate_node(0));
    CHECK((record.activation_flags() & 1U) == 0);
    CHECK(!list.activate_node(1, 0));
    const std::array<std::int32_t, 3> zero_response{0, 0, 0};
    CHECK(record.motion_response() == zero_response);
    CHECK(record.interaction_latch() == 0);

    auto& mutable_record = list.records().front();
    mutable_record.set_motion_response({1, -2, 3});
    mutable_record.set_interaction_latch(1);
    const std::array<std::int32_t, 3> expected_response{1, -2, 3};
    CHECK(record.motion_response() == expected_response);
    CHECK(record.interaction_latch() == 1);

    opentony::trg::TrafficRuntimeList variants;
    variants.build(one_traffic_fixture(0xd6));
    CHECK(variants.records().front().resource() == "c_police");
    CHECK(variants.records().front().sound_setup_disabled());
    variants.build(one_traffic_fixture(0xdc));
    CHECK(variants.records().front().resource() == "c_gull");
    CHECK(variants.records().front().sound_id() == -1);
    CHECK(variants.record_for_node(0) != nullptr);
    CHECK(variants.record_for_node(1) == nullptr);

    const std::filesystem::path asset_root =
        "/home/joao/dev/OpenTony/build/assets/all-pkr/files/data";
    if (std::filesystem::is_directory(asset_root)) {
        const auto catalog = opentony::assets::PsxAssetCatalog::scan(asset_root.string());
        opentony::trg::TrafficRuntimeList loaded;
        loaded.build(one_traffic_fixture(0xd5), &catalog);
        const auto& loaded_record = loaded.records().front();
        if (catalog.contains("c_taxi")) {
            CHECK(loaded.region(loaded_record.region_index()).asset_available);
            CHECK(loaded_record.region_slot() == 0);
            CHECK(loaded.region(loaded_record.region_index()).runtime.has_value());
            CHECK(loaded.region(loaded_record.region_index()).runtime->object_count() > 0);
        }
    }

    const std::filesystem::path package_path =
        "/home/joao/dev/OpenTony/build/disc/files/SETUP/data/ALL.PKR";
    if (std::filesystem::is_regular_file(package_path)) {
        const auto package = opentony::assets::PkrArchive::load(package_path.string());
        opentony::trg::TrafficRuntimeList packaged;
        packaged.build(one_traffic_fixture(0xdb), package);
        const auto& packaged_record = packaged.records().front();
        CHECK(packaged_record.resource() == "c_bull");
        if (packaged.region(packaged_record.region_index()).asset_available) {
            CHECK(packaged_record.region_slot() == 0);
            CHECK(packaged.region(packaged_record.region_index()).runtime.has_value());
            CHECK(packaged.region(packaged_record.region_index()).runtime->object_count() > 0);
        }
    }

    std::cout << "Traffic runtime tests passed\n";
}
