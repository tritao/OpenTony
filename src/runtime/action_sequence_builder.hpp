#pragma once

#include "action_sequence.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace opentony::runtime {

// FUN_004bb4f0 turns the section-5 source table into 0x28-byte heap records.
// The native representation keeps only the fields consumed by
// FUN_004bcf00/0x004bcdd0. `filter_flags` is the processed +0x24 field;
// `flags` is the copied +0x26 field.
struct RetailActionResourceRecord final {
    std::int16_t stream_relative{};
    std::uint16_t filter_flags{};
    std::uint16_t flags{};
};

// Runtime selection state populated by the skater/resource setup path rather
// than stored in TRICKS.BIN. This is the portable representation of the
// selection view consumed by FUN_004bcc90 and FUN_004bcdd0. In the normal
// player slot FUN_004416050(index), the view begins at slot +0xcc; therefore
// the mapped fields' physical slot offsets are +0xf7/+0xfc, while the retail
// helper functions address them as view-relative +0x2b/+0x30.
// The two mapped arrays are intentionally separate: a resource ID selects a
// source stream while its mapping index selects DAT_00540e30.
struct RetailActionResourceSelection final {
    std::array<std::uint8_t, 4> direct_resource_ids{
        0xff, 0xff, 0xff, 0xff};
    std::array<std::uint8_t, 5> mapped_resource_ids{
        0xff, 0xff, 0xff, 0xff, 0xff};
    std::array<std::uint8_t, 5> mapped_mapping_indices{
        0xff, 0xff, 0xff, 0xff, 0xff};

    // Equivalent to FUN_00416340: return the mapping byte associated with a
    // mapped resource, or no value when the resource is not installed.
    [[nodiscard]] std::optional<std::uint8_t>
    mapping_index_for_resource(std::uint8_t resource_id) const noexcept;

    // Equivalent to the pair-management portion of FUN_00416380. A resource
    // ID of -1/0xff removes the pair for mapping_index. Otherwise resource
    // IDs 0..0x7f can be installed into the first empty slot, and the mapping
    // byte is stored verbatim (including 0xff). Existing pairs with the same
    // mapping index are replaced; duplicate resource IDs are rejected. The
    // signed arguments preserve the retail remove sentinel. The trick-menu
    // caller separately limits mapping indices to 0..0x2a before invoking
    // this helper.
    [[nodiscard]] bool update_mapped_resource(
        std::int32_t resource_id,
        std::int32_t mapping_index) noexcept;
};

// FUN_004c36d0 examines at most 0x60 loaded-resource pointers. Only the
// value at loaded entry +0x08 is needed at this boundary: it is compared with
// the current resource key and, when absent from the selection view, reused as
// the mapped-action index passed to FUN_00416380. A null retail pointer is
// represented by `present == false`.
struct RetailLoadedActionResourceRecord final {
    bool present{};
    std::int32_t mapping_index{-1};
};

enum class RetailActionResourceBindingStatus {
    TypeRejected,
    ResourceNotFound,
    InvalidMappingIndex,
    Installed,
    AlreadyInstalled,
    ExistingMappingConflict,
    SelectionRejected,
};

struct RetailActionResourceBindingResult final {
    RetailActionResourceBindingStatus status{
        RetailActionResourceBindingStatus::ResourceNotFound};
    std::int32_t source_resource_id{-1};
    std::int32_t mapping_index{-1};
};

// Exact FUN_004c3350 values used by the loaded-resource setup path. The
// caller then tests the returned mask's 0x8000 bit before entering the
// selection bridge.
[[nodiscard]] std::uint16_t retail_action_resource_type_mask(
    std::int32_t type_value) noexcept;

// Exact FUN_004bc330 scan over constructor record +0x24. The first matching
// record index is the resource ID later passed to FUN_004c36d0.
[[nodiscard]] std::optional<std::size_t>
retail_action_resource_id_for_filter(
    std::span<const RetailActionResourceRecord> resources,
    std::uint16_t filter_flags) noexcept;

// Bounded native equivalent of the pair-changing branch in FUN_004c36d0.
// `source_resource_id` is the constructor-record index, `resource_key` is
// the current FUN_00470ab0-derived key, and the loaded record carrying that
// key supplies the mapping index. Existing equal pairs are left untouched;
// existing unequal pairs are reported as a conflict because retail routes
// that case through its UI/configuration path instead of silently replacing
// it. The native helper does not inspect more than retail's 0x60 entries.
[[nodiscard]] RetailActionResourceBindingResult
bind_loaded_action_resource(
    RetailActionResourceSelection& selection,
    std::int32_t source_resource_id,
    std::int32_t resource_key,
    std::uint16_t type_mask,
    std::span<const RetailLoadedActionResourceRecord> loaded_resources) noexcept;

struct RetailActionSequenceBuilderInput final {
    // The short-record table selected from section 0 by the player/resource
    // index. Its records use the same four trailing words as the shipped
    // sequence tables; the last word is ignored by FUN_004bc3a0.
    std::span<const std::uint8_t> player_input_table{};
    std::span<const RetailActionResourceRecord> resources{};

    // FUN_004bcc90 reads four resource IDs from the selection view's first
    // four bytes (the normal slot supplies this view at +0xcc). 0xff means
    // no resource for that static group. FUN_004bcdd0 reads the five mapped
    // resource IDs at view-relative +0x2b and stops at 0xff.
    std::array<std::uint8_t, 4> direct_resource_ids{
        0xff, 0xff, 0xff, 0xff};
    std::array<std::uint8_t, 5> mapped_resource_ids{
        0xff, 0xff, 0xff, 0xff, 0xff};

    // FUN_004bcdd0 reads the associated five mapping indices at view-relative
    // +0x30. They select entries in DAT_00540e30 independently of the
    // resource IDs.
    std::array<std::uint8_t, 5> mapped_mapping_indices{
        0xff, 0xff, 0xff, 0xff, 0xff};

    // Materialize the three legacy input fields as the runtime selection
    // object used by the native setup boundary.
    [[nodiscard]] RetailActionResourceSelection resource_selection() const noexcept {
        return RetailActionResourceSelection{
            direct_resource_ids,
            mapped_resource_ids,
            mapped_mapping_indices};
    }
};

struct RetailActionSequenceBuildResult final {
    std::vector<std::uint8_t> table{};
    std::size_t input_record_count{};
    std::size_t ordinary_record_count{};
    std::size_t static_record_count{};
    std::size_t mapped_record_count{};
};

// Parses the section-5 [length][actions][stream-relative][flags] database
// into the compact fields needed by the generated-table builder. The result
// is in retail record order, so a byte resource ID used by 0x004bd170 maps
// directly to an element in this vector.
[[nodiscard]] std::optional<std::vector<RetailActionResourceRecord>>
parse_retail_action_resources(
    std::span<const std::uint8_t> source_table,
    std::size_t max_records = 4096) noexcept;

// Exact variant used when the loaded TRICKS image is available. It walks each
// record's direct image-relative stream, recovering FUN_004bb7e0's provisional
// filter value from opcode 0x51 before applying FUN_004bba50's raw-flag
// promotions. The two-argument overload above remains useful for fixtures
// where only the database record fields are available.
[[nodiscard]] std::optional<std::vector<RetailActionResourceRecord>>
parse_retail_action_resources(
    std::span<const std::uint8_t> source_table,
    std::span<const std::uint8_t> image,
    std::size_t max_records = 4096) noexcept;

// Bounded native port of the deterministic table work in FUN_004bcf00.
// The output is the retail [length][actions...][stream-relative][flags]
// table, terminated by a zero length. Player/resource selection is explicit
// in the input because the selection-view bytes (normal slot +0xcc, mapped
// fields at physical slot +0xf7/+0xfc) are populated by the skater setup
// outside TRICKS.BIN.
[[nodiscard]] std::optional<RetailActionSequenceBuildResult>
build_retail_action_sequence_table(
    const RetailActionSequenceBuilderInput& input,
    std::size_t max_input_records = 256) noexcept;

} // namespace opentony::runtime
