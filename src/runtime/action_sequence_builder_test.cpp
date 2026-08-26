#include "action_sequence_builder.hpp"

#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace {

void put_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void put_record(
    std::vector<std::uint8_t>& bytes,
    std::initializer_list<std::uint16_t> actions,
    std::uint16_t stream_relative,
    std::uint16_t flags) {
    put_u16(bytes, static_cast<std::uint16_t>(actions.size()));
    for (const std::uint16_t action : actions) {
        put_u16(bytes, action);
    }
    put_u16(bytes, stream_relative);
    put_u16(bytes, flags);
}

} // namespace

int main() {
    using namespace opentony::runtime;

    RetailActionResourceSelection selection{};
    assert(!selection.mapping_index_for_resource(3).has_value());
    assert(!selection.mapping_index_for_resource(0xff).has_value());
    assert(selection.update_mapped_resource(3, 7));
    assert(selection.mapping_index_for_resource(3).has_value());
    assert(*selection.mapping_index_for_resource(3) == 7);
    // Reusing a mapping index replaces its resource in place.
    assert(selection.update_mapped_resource(4, 7));
    assert(!selection.mapping_index_for_resource(3).has_value());
    assert(*selection.mapping_index_for_resource(4) == 7);
    // A resource cannot be installed twice under another mapping index.
    assert(!selection.update_mapped_resource(4, 8));
    assert(*selection.mapping_index_for_resource(4) == 7);
    // Retail's -1 remove sentinel clears the pair by mapping index.
    assert(selection.update_mapped_resource(-1, 7));
    assert(!selection.mapping_index_for_resource(4).has_value());
    assert(selection.update_mapped_resource(0, 1));
    // FUN_00416380 treats 0xff as the remove sentinel only in its resource
    // argument; the mapping byte is stored verbatim when the resource is set.
    assert(selection.update_mapped_resource(1, 0xff));
    assert(selection.mapping_index_for_resource(1).has_value());
    assert(*selection.mapping_index_for_resource(1) == 0xff);
    assert(!selection.update_mapped_resource(0x80, 2));

    const std::array<RetailActionResourceRecord, 2> filter_records{
        RetailActionResourceRecord{0x111, 0x4000, 0xaaaa},
        RetailActionResourceRecord{0x222, 0x8000, 0xbbbb},
    };
    assert(retail_action_resource_type_mask(6) == 0x0200);
    assert(retail_action_resource_type_mask(7) == 0x0100);
    assert(retail_action_resource_type_mask(8) == 0x0800);
    assert(retail_action_resource_type_mask(9) == 0x4000);
    assert(retail_action_resource_type_mask(10) == 0x8000);
    assert(retail_action_resource_type_mask(11) == 0);
    assert(retail_action_resource_id_for_filter(filter_records, 0x8000)
        == std::optional<std::size_t>(1));
    assert(!retail_action_resource_id_for_filter(filter_records, 0x1234).has_value());

    const auto loaded_resources = [] {
        std::array<RetailLoadedActionResourceRecord, 0x61> records{};
        records[0] = RetailLoadedActionResourceRecord{true, 7};
        records[0x60] = RetailLoadedActionResourceRecord{true, 8};
        return records;
    }();
    RetailActionResourceSelection loaded_selection{};
    const auto installed = bind_loaded_action_resource(
        loaded_selection,
        3,
        7,
        retail_action_resource_type_mask(10),
        loaded_resources);
    assert(installed.status == RetailActionResourceBindingStatus::Installed);
    assert(installed.source_resource_id == 3);
    assert(installed.mapping_index == 7);
    assert(*loaded_selection.mapping_index_for_resource(3) == 7);
    const auto already_installed = bind_loaded_action_resource(
        loaded_selection,
        3,
        7,
        0x8000,
        loaded_resources);
    assert(already_installed.status == RetailActionResourceBindingStatus::AlreadyInstalled);
    RetailActionResourceSelection conflict_selection{};
    assert(conflict_selection.update_mapped_resource(3, 6));
    const auto conflicting = bind_loaded_action_resource(
        conflict_selection,
        3,
        7,
        0x8000,
        loaded_resources);
    assert(conflicting.status == RetailActionResourceBindingStatus::ExistingMappingConflict);
    const auto rejected_type = bind_loaded_action_resource(
        loaded_selection,
        4,
        7,
        0x4000,
        loaded_resources);
    assert(rejected_type.status == RetailActionResourceBindingStatus::TypeRejected);
    const std::array<RetailLoadedActionResourceRecord, 1> invalid_mapping{
        RetailLoadedActionResourceRecord{true, 0x2b}};
    const auto invalid = bind_loaded_action_resource(
        loaded_selection,
        4,
        0x2b,
        0x8000,
        invalid_mapping);
    assert(invalid.status == RetailActionResourceBindingStatus::InvalidMappingIndex);

    std::vector<std::uint8_t> input;
    put_record(input, {1, 12}, 0x111, 0);
    put_record(input, {5, 12}, 0x222, 0);
    put_u16(input, 0);

    const std::array<RetailActionResourceRecord, 2> resources{
        RetailActionResourceRecord{0x111, 0x4000, 0xaaaa},
        RetailActionResourceRecord{0x222, 0x4000, 0xbbbb},
    };
    RetailActionSequenceBuilderInput builder_input{
        input,
        resources,
        {0, 0xff, 0xff, 0xff},
        {1, 0xff, 0xff, 0xff, 0xff},
        {0, 0xff, 0xff, 0xff, 0xff},
    };
    const auto built = build_retail_action_sequence_table(builder_input);
    assert(built.has_value());
    assert(built->input_record_count == 2);
    // The first input is a static 0x4000 combo and is emitted by the static
    // pass. The second is an ordinary record; the four static 0x4000 group
    // entries and the mapping pass are then appended. The mapped pass uses
    // resource 1 with mapping index 0, exercising the two independent retail
    // arrays rather than treating the resource ID as the mapping index.
    assert(built->static_record_count == 6);
    assert(built->ordinary_record_count == 1);
    assert(built->mapped_record_count == 1);

    bool found_ordinary = false;
    bool found_mapped = false;
    std::size_t record_offset = 0;
    for (std::size_t record_number = 0; record_number < 32; ++record_number) {
        const auto record = read_action_sequence_record(
            built->table,
            record_offset);
        assert(record.has_value());
        if (record->length == 0) {
            break;
        }
        if (record->length == 2
            && record->actions[0] == 5
            && record->actions[1] == 12) {
            assert(record->stream_relative == 0x222);
            assert(record->flags == 0xbbbb);
            found_ordinary = true;
        }
        if (record->length == 3
            && record->actions[0] == 1
            && record->actions[1] == 4
            && record->actions[2] == 12) {
            assert(record->stream_relative == 0x222);
            assert(record->flags == 0xbbbb);
            found_mapped = true;
        }
        record_offset +=
            (static_cast<std::size_t>(record->length) + 3U) * 2U;
    }
    assert(found_ordinary);
    assert(found_mapped);

    const auto parsed_resources = parse_retail_action_resources(
        built->table);
    assert(parsed_resources.has_value());
    assert(parsed_resources->size() == 8);

    std::vector<std::uint8_t> source_table;
    put_record(source_table, {1}, 0x10, 0x0800);
    put_record(source_table, {2}, 0x14, 0x4000);
    put_u16(source_table, 0);
    std::vector<std::uint8_t> image(0x20, 0);
    image[0x10] = 0x51;
    image[0x11] = 0x34;
    image[0x12] = 0x12;
    image[0x13] = kEndActionStreamOpcode;
    image[0x14] = 0x51;
    image[0x15] = 0x00;
    image[0x16] = 0x08;
    image[0x17] = kEndActionStreamOpcode;

    const auto exact_resources = parse_retail_action_resources(
        source_table,
        image);
    assert(exact_resources.has_value());
    assert(exact_resources->size() == 2);
    // 0x51 replaces the provisional value, then raw 0x0800 is promoted.
    assert((*exact_resources)[0].filter_flags == 0x1a34);
    // Raw 0x4000 removes a provisional 0x0800 and promotes 0x4000.
    assert((*exact_resources)[1].filter_flags == 0x4000);
    return 0;
}
