#include "action_sequence_builder.hpp"

#include "action_commands.hpp"

#include <algorithm>

namespace opentony::runtime {
namespace {

struct StaticCombo final {
    std::uint8_t length{};
    std::array<std::uint16_t, 3> actions{};
    std::uint16_t filter_mask{};
    std::uint8_t resource_group{};
};

struct StaticMapping final {
    std::uint8_t length{};
    std::array<std::uint16_t, 3> actions{};
};

// DAT_00540cb8, 30 records, stride 0x0c. The last two words in each retail
// record are the mask at +0x08 and the runtime-resource group at +0x0a.
constexpr std::array<StaticCombo, 30> kStaticCombos{
    StaticCombo{2, {1, 12, 0}, 0x4000, 0},
    StaticCombo{2, {4, 12, 0}, 0x4000, 0},
    StaticCombo{2, {2, 12, 0}, 0x4000, 0},
    StaticCombo{2, {3, 12, 0}, 0x4000, 0},
    StaticCombo{3, {1, 1, 12}, 0x0800, 0},
    StaticCombo{3, {2, 2, 12}, 0x0800, 0},
    StaticCombo{2, {1, 10, 0}, 0x0300, 3},
    StaticCombo{2, {6, 10, 0}, 0x0300, 2},
    StaticCombo{2, {4, 10, 0}, 0x0300, 3},
    StaticCombo{2, {8, 10, 0}, 0x0300, 2},
    StaticCombo{2, {2, 10, 0}, 0x0300, 3},
    StaticCombo{2, {7, 10, 0}, 0x0300, 2},
    StaticCombo{2, {3, 10, 0}, 0x0300, 3},
    StaticCombo{2, {5, 10, 0}, 0x0300, 2},
    StaticCombo{2, {1, 9, 0}, 0x0300, 3},
    StaticCombo{2, {6, 9, 0}, 0x0300, 2},
    StaticCombo{2, {4, 9, 0}, 0x0300, 3},
    StaticCombo{2, {8, 9, 0}, 0x0300, 2},
    StaticCombo{2, {2, 9, 0}, 0x0300, 3},
    StaticCombo{2, {7, 9, 0}, 0x0300, 2},
    StaticCombo{2, {3, 9, 0}, 0x0300, 3},
    StaticCombo{2, {5, 9, 0}, 0x0300, 2},
    StaticCombo{3, {1, 1, 10}, 0x0300, 1},
    StaticCombo{3, {4, 4, 10}, 0x0300, 1},
    StaticCombo{3, {2, 2, 10}, 0x0300, 1},
    StaticCombo{3, {3, 3, 10}, 0x0300, 1},
    StaticCombo{3, {1, 1, 9}, 0x0300, 1},
    StaticCombo{3, {4, 4, 9}, 0x0300, 1},
    StaticCombo{3, {2, 2, 9}, 0x0300, 1},
    StaticCombo{3, {3, 3, 9}, 0x0300, 1},
};

// DAT_00540e30, 36 records, stride 0x0c. FUN_004bcdd0 consumes only the
// count and action words; the source record ID is the table byte itself.
constexpr std::array<StaticMapping, 36> kStaticMappings{
    StaticMapping{3, {1, 4, 12}},
    StaticMapping{3, {1, 2, 12}},
    StaticMapping{3, {1, 3, 12}},
    StaticMapping{3, {4, 1, 12}},
    StaticMapping{3, {4, 2, 12}},
    StaticMapping{3, {4, 3, 12}},
    StaticMapping{3, {2, 1, 12}},
    StaticMapping{3, {2, 4, 12}},
    StaticMapping{3, {2, 3, 12}},
    StaticMapping{3, {3, 1, 12}},
    StaticMapping{3, {3, 4, 12}},
    StaticMapping{3, {3, 2, 12}},
    StaticMapping{3, {1, 4, 10}},
    StaticMapping{3, {1, 2, 10}},
    StaticMapping{3, {1, 3, 10}},
    StaticMapping{3, {4, 1, 10}},
    StaticMapping{3, {4, 2, 10}},
    StaticMapping{3, {4, 3, 10}},
    StaticMapping{3, {2, 1, 10}},
    StaticMapping{3, {2, 4, 10}},
    StaticMapping{3, {2, 3, 10}},
    StaticMapping{3, {3, 1, 10}},
    StaticMapping{3, {3, 4, 10}},
    StaticMapping{3, {3, 2, 10}},
    StaticMapping{3, {1, 4, 9}},
    StaticMapping{3, {1, 2, 9}},
    StaticMapping{3, {1, 3, 9}},
    StaticMapping{3, {4, 1, 9}},
    StaticMapping{3, {4, 2, 9}},
    StaticMapping{3, {4, 3, 9}},
    StaticMapping{3, {2, 1, 9}},
    StaticMapping{3, {2, 4, 9}},
    StaticMapping{3, {2, 3, 9}},
    StaticMapping{3, {3, 1, 9}},
    StaticMapping{3, {3, 4, 9}},
    StaticMapping{3, {3, 2, 9}},
};

constexpr std::array<std::uint16_t, 5> kBuilderMasks{
    0x1000, 0x0800, 0x2000, 0x4000, 0x0300};

void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
    bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void append_record(
    std::vector<std::uint8_t>& bytes,
    std::span<const std::uint16_t> actions,
    std::int16_t stream_relative,
    std::uint16_t flags) {
    append_u16(bytes, static_cast<std::uint16_t>(actions.size()));
    for (const std::uint16_t action : actions) {
        append_u16(bytes, action);
    }
    append_u16(bytes, static_cast<std::uint16_t>(stream_relative));
    append_u16(bytes, flags);
}

[[nodiscard]] bool same_actions(
    std::span<const std::uint16_t> lhs,
    std::span<const std::uint16_t> rhs) noexcept {
    return lhs.size() == rhs.size()
        && std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

[[nodiscard]] bool is_static_combo(
    std::span<const std::uint16_t> actions) noexcept {
    return std::any_of(
        kStaticCombos.begin(),
        kStaticCombos.end(),
        [actions](const StaticCombo& combo) {
            return combo.length == actions.size()
                && same_actions(
                    actions,
                    std::span<const std::uint16_t>(
                        combo.actions.data(), combo.length));
        });
}

[[nodiscard]] const RetailActionResourceRecord* resource_at(
    std::span<const RetailActionResourceRecord> resources,
    std::uint8_t resource_id) noexcept {
    if (resource_id == 0xff || resource_id >= resources.size()) {
        return nullptr;
    }
    return &resources[resource_id];
}

[[nodiscard]] std::uint16_t source_type_mask(
    std::span<const std::uint16_t> actions) noexcept {
    // FUN_004bb4f0 derives the source record's initial +0x24 classification
    // from the final action through the 12-byte class table at 0x004bb760.
    // The 0x51 value parsed by FUN_004bb7e0 is stored in the neighboring
    // +0x22 field and is not the filter used by FUN_004bcf00.
    if (actions.empty()) {
        return 0;
    }
    switch (actions.back()) {
    case 1:
    case 2:
        return 0x2000;
    case 9:
        return 0x0100;
    case 10:
        return 0x0200;
    case 12:
        return 0x0800;
    default:
        return 0;
    }
}

[[nodiscard]] std::uint16_t processed_filter_flags(
    std::span<const std::uint16_t> actions,
    std::uint16_t raw_flags) noexcept {
    std::uint16_t filter_flags = source_type_mask(actions);

    // FUN_004bba50 promotes only these source-record bits into the processed
    // +0x24 mask.  0x4000 first removes a provisional 0x0800 classification.
    if ((raw_flags & 0x0800U) != 0) {
        filter_flags = static_cast<std::uint16_t>(filter_flags | 0x0800U);
    }
    if ((raw_flags & 0x4000U) != 0) {
        filter_flags = static_cast<std::uint16_t>(
            (filter_flags & ~0x0800U) | 0x4000U);
    }
    if ((raw_flags & 0x8000U) != 0) {
        filter_flags = static_cast<std::uint16_t>(filter_flags | 0x8000U);
    }
    if ((raw_flags & 0x2000U) != 0) {
        filter_flags = static_cast<std::uint16_t>(filter_flags | 0x2000U);
    }
    if ((raw_flags & 0x1000U) != 0) {
        filter_flags = static_cast<std::uint16_t>(filter_flags | 0x1000U);
    }
    return filter_flags;
}

} // namespace

std::optional<std::uint8_t>
RetailActionResourceSelection::mapping_index_for_resource(
    std::uint8_t resource_id) const noexcept {
    if (resource_id == 0xff) {
        return std::nullopt;
    }
    for (std::size_t slot = 0; slot < mapped_resource_ids.size(); ++slot) {
        if (mapped_resource_ids[slot] != 0xff
            && mapped_resource_ids[slot] == resource_id) {
            return mapped_mapping_indices[slot];
        }
    }
    return std::nullopt;
}

bool RetailActionResourceSelection::update_mapped_resource(
    std::int32_t resource_id,
    std::int32_t mapping_index) noexcept {
    if (mapping_index < 0 || mapping_index > 0xff) {
        return false;
    }

    const auto mapping = static_cast<std::uint8_t>(mapping_index);
    if (resource_id == -1 || resource_id == 0xff) {
        for (std::size_t slot = 0; slot < mapped_mapping_indices.size(); ++slot) {
            if (mapped_mapping_indices[slot] == mapping) {
                mapped_resource_ids[slot] = 0xff;
                mapped_mapping_indices[slot] = 0xff;
                return true;
            }
        }
        return true;
    }

    if (resource_id < 0 || resource_id >= 0x80) {
        return false;
    }
    const auto resource = static_cast<std::uint8_t>(resource_id);

    // FUN_00416380 removes an existing mapping-index pair before checking
    // for a duplicate resource ID, so preserve that observable ordering.
    for (std::size_t slot = 0; slot < mapped_mapping_indices.size(); ++slot) {
        if (mapped_mapping_indices[slot] == mapping) {
            mapped_resource_ids[slot] = 0xff;
            mapped_mapping_indices[slot] = 0xff;
        }
    }

    if (mapping_index_for_resource(resource).has_value()) {
        return false;
    }

    for (std::size_t slot = 0; slot < mapped_resource_ids.size(); ++slot) {
        if (mapped_resource_ids[slot] == 0xff) {
            mapped_resource_ids[slot] = resource;
            mapped_mapping_indices[slot] = mapping;
            return true;
        }
    }
    return false;
}

std::uint16_t retail_action_resource_type_mask(
    std::int32_t type_value) noexcept {
    switch (type_value) {
    case 6:
        return 0x0200;
    case 7:
        return 0x0100;
    case 8:
        return 0x0800;
    case 9:
        return 0x4000;
    case 10:
        return 0x8000;
    default:
        return 0;
    }
}

std::optional<std::size_t> retail_action_resource_id_for_filter(
    std::span<const RetailActionResourceRecord> resources,
    std::uint16_t filter_flags) noexcept {
    for (std::size_t resource_id = 0;
         resource_id < resources.size();
         ++resource_id) {
        if (resources[resource_id].filter_flags == filter_flags) {
            return resource_id;
        }
    }
    return std::nullopt;
}

RetailActionResourceBindingResult bind_loaded_action_resource(
    RetailActionResourceSelection& selection,
    std::int32_t source_resource_id,
    std::int32_t resource_key,
    std::uint16_t type_mask,
    std::span<const RetailLoadedActionResourceRecord> loaded_resources) noexcept {
    RetailActionResourceBindingResult result{};
    result.source_resource_id = source_resource_id;
    if ((type_mask & 0x8000U) == 0) {
        result.status = RetailActionResourceBindingStatus::TypeRejected;
        return result;
    }

    const std::size_t loaded_count = std::min<std::size_t>(
        loaded_resources.size(),
        0x60);
    for (std::size_t loaded_index = 0;
         loaded_index < loaded_count;
         ++loaded_index) {
        const RetailLoadedActionResourceRecord& loaded =
            loaded_resources[loaded_index];
        if (!loaded.present || loaded.mapping_index != resource_key) {
            continue;
        }

        result.mapping_index = loaded.mapping_index;
        if (loaded.mapping_index < 0 || loaded.mapping_index > 0x2a) {
            result.status = RetailActionResourceBindingStatus::InvalidMappingIndex;
            return result;
        }
        if (source_resource_id >= 0 && source_resource_id < 0x80) {
            const auto existing = selection.mapping_index_for_resource(
                static_cast<std::uint8_t>(source_resource_id));
            if (existing.has_value()) {
                result.status = *existing == loaded.mapping_index
                    ? RetailActionResourceBindingStatus::AlreadyInstalled
                    : RetailActionResourceBindingStatus::ExistingMappingConflict;
                return result;
            }
        }
        if (selection.update_mapped_resource(
                source_resource_id,
                loaded.mapping_index)) {
            result.status = RetailActionResourceBindingStatus::Installed;
        } else {
            result.status = RetailActionResourceBindingStatus::SelectionRejected;
        }
        return result;
    }

    result.status = RetailActionResourceBindingStatus::ResourceNotFound;
    return result;
}

std::optional<std::vector<RetailActionResourceRecord>>
parse_retail_action_resources(
    std::span<const std::uint8_t> source_table,
    std::size_t max_records) noexcept {
    return parse_retail_action_resources(source_table, {}, max_records);
}

std::optional<std::vector<RetailActionResourceRecord>>
parse_retail_action_resources(
    std::span<const std::uint8_t> source_table,
    std::span<const std::uint8_t> image,
    std::size_t max_records) noexcept {
    std::vector<RetailActionResourceRecord> result;
    std::size_t cursor = 0;
    for (std::size_t record_number = 0; record_number < max_records; ++record_number) {
        const auto record = read_action_sequence_record(source_table, cursor);
        if (!record.has_value()) {
            return std::nullopt;
        }
        if (record->length == 0) {
            return result;
        }
        result.push_back(RetailActionResourceRecord{
            record->stream_relative,
            processed_filter_flags(
                std::span<const std::uint16_t>(
                    record->actions.data(), record->length),
                record->flags),
            record->flags,
        });
        cursor += (static_cast<std::size_t>(record->length) + 3U) * 2U;
    }
    return std::nullopt;
}

std::optional<RetailActionSequenceBuildResult>
build_retail_action_sequence_table(
    const RetailActionSequenceBuilderInput& input,
    std::size_t max_input_records) noexcept {
    if (input.resources.empty()) {
        return std::nullopt;
    }

    struct InputRecord final {
        ActionSequenceTableRecord sequence{};
        const RetailActionResourceRecord* resource{};
    };
    std::vector<InputRecord> records;
    std::size_t cursor = 0;
    for (std::size_t record_number = 0;
         record_number < max_input_records;
         ++record_number) {
        const auto sequence = read_action_sequence_record(
            input.player_input_table,
            cursor);
        if (!sequence.has_value()) {
            return std::nullopt;
        }
        if (sequence->length == 0) {
            break;
        }
        const auto resource = std::find_if(
            input.resources.begin(),
            input.resources.end(),
            [&sequence](const RetailActionResourceRecord& candidate) {
                return candidate.stream_relative == sequence->stream_relative;
            });
        if (resource == input.resources.end()) {
            return std::nullopt;
        }
        records.push_back(InputRecord{*sequence, &*resource});
        cursor += (static_cast<std::size_t>(sequence->length) + 3U) * 2U;
    }
    if (records.size() == max_input_records) {
        return std::nullopt;
    }

    RetailActionSequenceBuildResult result{};
    result.input_record_count = records.size();
    const RetailActionResourceSelection selection = input.resource_selection();

    for (const std::uint16_t mask : kBuilderMasks) {
        for (const InputRecord& record : records) {
            const auto actions = std::span<const std::uint16_t>(
                record.sequence.actions.data(), record.sequence.length);
            if ((record.resource->filter_flags & mask) == 0
                || is_static_combo(actions)) {
                continue;
            }
            append_record(
                result.table,
                actions,
                record.resource->stream_relative,
                record.resource->flags);
            ++result.ordinary_record_count;
        }

        for (const StaticCombo& combo : kStaticCombos) {
            if ((combo.filter_mask & mask) == 0
                || combo.resource_group >= selection.direct_resource_ids.size()) {
                continue;
            }
            const RetailActionResourceRecord* resource = resource_at(
                input.resources,
                selection.direct_resource_ids[combo.resource_group]);
            if (resource == nullptr) {
                continue;
            }
            append_record(
                result.table,
                std::span<const std::uint16_t>(
                    combo.actions.data(), combo.length),
                resource->stream_relative,
                resource->flags);
            ++result.static_record_count;
        }
    }

    for (std::size_t mapping_slot = 0;
         mapping_slot < selection.mapped_resource_ids.size();
         ++mapping_slot) {
        const std::uint8_t resource_id =
            selection.mapped_resource_ids[mapping_slot];
        if (resource_id == 0xff) {
            break;
        }
        const std::uint8_t mapping_id =
            selection.mapped_mapping_indices[mapping_slot];
        if (mapping_id == 0xff || mapping_id >= kStaticMappings.size()) {
            return std::nullopt;
        }
        const RetailActionResourceRecord* resource = resource_at(
            input.resources,
            resource_id);
        if (resource == nullptr) {
            return std::nullopt;
        }
        const StaticMapping& mapping = kStaticMappings[mapping_id];
        append_record(
            result.table,
            std::span<const std::uint16_t>(
                mapping.actions.data(), mapping.length),
            resource->stream_relative,
            resource->flags);
        ++result.mapped_record_count;
    }

    append_u16(result.table, 0);
    return result;
}

} // namespace opentony::runtime
