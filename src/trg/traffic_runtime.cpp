#include "traffic_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>

namespace opentony::trg {
namespace {

[[nodiscard]] std::string upper_ascii(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return value;
}

[[nodiscard]] std::optional<std::size_t> package_psx_entry(
    const assets::PkrArchive& package,
    std::string_view resource) {
    const std::string wanted = upper_ascii(std::string(resource) + ".PSX");
    for (std::size_t index = 0; index < package.entries().size(); ++index) {
        const assets::PkrFileEntry& entry = package.entries()[index];
        if (upper_ascii(entry.name) == wanted) {
            return index;
        }
    }
    return std::nullopt;
}

} // namespace

std::uint16_t TrafficRuntimeRecord::u16(std::size_t offset) const noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U));
}

std::uint32_t TrafficRuntimeRecord::u32(std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 3])) << 24U));
}

std::int32_t TrafficRuntimeRecord::s32(std::size_t offset) const noexcept {
    return static_cast<std::int32_t>(u32(offset));
}

void TrafficRuntimeRecord::put16(std::size_t offset, std::uint16_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value & 0xffU);
    raw_[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void TrafficRuntimeRecord::put32(std::size_t offset, std::uint32_t value) noexcept {
    put16(offset, static_cast<std::uint16_t>(value));
    put16(offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

std::uint16_t TrafficRuntimeRecord::subtype() const noexcept {
    return u16(0x3c);
}

std::uint8_t TrafficRuntimeRecord::region_slot() const noexcept {
    return std::to_integer<std::uint8_t>(raw_[0x1f]);
}

std::uint16_t TrafficRuntimeRecord::model_index() const noexcept {
    return u16(0x1a);
}

std::array<std::int32_t, 3> TrafficRuntimeRecord::position() const noexcept {
    return {s32(0x08), s32(0x0c), s32(0x10)};
}

std::array<std::uint16_t, 3> TrafficRuntimeRecord::constructor_parameters() const noexcept {
    return {u16(0x14), u16(0x16), u16(0x18)};
}

std::array<std::int32_t, 3> TrafficRuntimeRecord::motion_response() const noexcept {
    return {s32(0x4c), s32(0x50), s32(0x54)};
}

std::uint32_t TrafficRuntimeRecord::activation_argument() const noexcept {
    return u32(0x64);
}

std::uint16_t TrafficRuntimeRecord::activation_flags() const noexcept {
    return u16(0x6a);
}

std::uint32_t TrafficRuntimeRecord::interaction_latch() const noexcept {
    return u32(0x1e4);
}

void TrafficRuntimeRecord::set_motion_response(
    std::array<std::int32_t, 3> response) noexcept {
    put32(0x4c, static_cast<std::uint32_t>(response[0]));
    put32(0x50, static_cast<std::uint32_t>(response[1]));
    put32(0x54, static_cast<std::uint32_t>(response[2]));
}

void TrafficRuntimeRecord::activate(std::uint32_t argument) noexcept {
    put32(0x64, argument);
    put16(0x6a, static_cast<std::uint16_t>(u16(0x6a) | 1U));
}

void TrafficRuntimeRecord::deactivate() noexcept {
    put16(0x6a, static_cast<std::uint16_t>(u16(0x6a) & ~1U));
}

void TrafficRuntimeRecord::set_interaction_latch(std::uint32_t value) noexcept {
    put32(0x1e4, value);
}

std::optional<TrafficRuntimeList::Selection> TrafficRuntimeList::selection_for(
    std::uint16_t subtype) noexcept {
    switch (subtype) {
    case 0xd5:
        return Selection{"c_taxi", 0x71, false};
    case 0xd6:
        return Selection{"c_police", -1, true};
    case 0xd7:
        return Selection{"c_bus", 0x121, false};
    case 0xd8:
        return Selection{"c_cable", 0x86, false};
    case 0xd9:
        return Selection{"c_kart", 0x111, false};
    case 0xda:
        return Selection{"c_mar", 0xdd, false};
    case 0xdb:
        return Selection{"c_bull", 0x145, false};
    case 0xdc:
        return Selection{"c_gull", -1, false};
    default:
        return std::nullopt;
    }
}

std::size_t TrafficRuntimeList::ensure_region(
    std::string resource,
    const assets::PsxAssetCatalog* catalog,
    const assets::PkrArchive* package) {
    const auto existing = std::find_if(
        regions_.begin(), regions_.end(),
        [&resource](const TrafficAssetRegion& region) {
            return region.resource == resource;
        });
    if (existing != regions_.end()) {
        return static_cast<std::size_t>(
            std::distance(regions_.begin(), existing));
    }

    if (regions_.size() >= kTrafficNoRegionSlot) {
        throw FormatError("traffic PSX region table exceeds the u8 slot range");
    }
    TrafficAssetRegion region{};
    region.resource = std::move(resource);
    region.slot = static_cast<std::uint8_t>(regions_.size());
    if (catalog != nullptr && catalog->contains(region.resource)) {
        region.asset_path = *catalog->path_for(region.resource);
        region.asset_available = true;
        region.runtime = assets::PsxRuntimeEnvironment::build(
            catalog->load(region.resource), region.slot);
    } else if (package != nullptr) {
        if (const auto entry = package_psx_entry(*package, region.resource);
            entry.has_value()) {
            region.owned_archive = std::make_shared<assets::PsxArchive>(
                assets::PsxArchive::parse(
                    package->decode(*entry),
                    package->entry(*entry).archive_path()));
            region.asset_path = package->entry(*entry).archive_path();
            region.asset_available = true;
            region.runtime = assets::PsxRuntimeEnvironment::build(
                *region.owned_archive, region.slot);
        }
    }
    regions_.push_back(std::move(region));
    return regions_.size() - 1;
}

void TrafficRuntimeList::populate_record(
    TrafficRuntimeRecord& record,
    const TrgFile& file,
    const NodeView& node,
    const Selection& selection,
    std::size_t sequence,
    std::uint8_t region_slot,
    std::size_t region_index) {
    if (node.index > std::numeric_limits<std::uint16_t>::max()) {
        throw FormatError("TRG traffic node index does not fit the runtime u16 field");
    }
    const std::array<std::int32_t, 3> source_position = file.node_position(node.index);
    const std::int64_t adjusted_y = static_cast<std::int64_t>(source_position[1])
        - 0x6e000;
    if (adjusted_y < std::numeric_limits<std::int32_t>::min()
        || adjusted_y > std::numeric_limits<std::int32_t>::max()) {
        throw FormatError("TRG traffic Y position overflows the runtime field");
    }
    const std::array<std::int32_t, 3> position{
        source_position[0],
        static_cast<std::int32_t>(adjusted_y),
        source_position[2],
    };
    const std::array<std::uint16_t, 3> parameters = file.node_orientation(node.index);

    record.source_node_ = node.index;
    record.resource_ = selection.resource;
    record.sound_id_ = selection.sound_id;
    record.sound_setup_disabled_ = selection.sound_setup_disabled;
    record.region_index_ = region_index;
    record.put32(0x08, static_cast<std::uint32_t>(position[0]));
    record.put32(0x0c, static_cast<std::uint32_t>(position[1]));
    record.put32(0x10, static_cast<std::uint32_t>(position[2]));
    record.put16(0x14, parameters[0]);
    record.put16(0x16, parameters[1]);
    record.put16(0x18, parameters[2]);
    record.put16(0x1a, 0);
    record.raw_[0x1f] = static_cast<std::byte>(region_slot);
    record.put16(0x3c, node.type == 1 ? file.node_subtype(node.index) : 0);
    record.put16(0xb0, static_cast<std::uint16_t>(node.index));
    record.put32(0x108, 0x10000);
    record.put16(0x1ac, 0x34);
    record.put16(0x1ae, 0x20);
    record.put16(0x0b4, 0x3c);
    record.put32(0x1e0, static_cast<std::uint32_t>(sequence));
    record.put32(0x1e4, 0);
}

void TrafficRuntimeList::build(
    const TrgFile& file,
    const assets::PsxAssetCatalog* catalog) {
    records_.clear();
    regions_.clear();
    for (const NodeView& node : file.nodes()) {
        if (node.type != 1) {
            continue;
        }
        const auto selection = selection_for(file.node_subtype(node.index));
        if (!selection.has_value()) {
            continue;
        }
        const std::size_t region_index = ensure_region(
            selection->resource, catalog, nullptr);
        TrafficRuntimeRecord record{};
        populate_record(
            record, file, node, *selection, records_.size(),
            regions_[region_index].asset_available
                ? regions_[region_index].slot
                : kTrafficNoRegionSlot,
            region_index);
        records_.push_back(std::move(record));
    }
}

void TrafficRuntimeList::build(
    const TrgFile& file,
    const assets::PkrArchive& package) {
    records_.clear();
    regions_.clear();
    for (const NodeView& node : file.nodes()) {
        if (node.type != 1) {
            continue;
        }
        const auto selection = selection_for(file.node_subtype(node.index));
        if (!selection.has_value()) {
            continue;
        }
        const std::size_t region_index = ensure_region(
            selection->resource, nullptr, &package);
        TrafficRuntimeRecord record{};
        populate_record(
            record, file, node, *selection, records_.size(),
            regions_[region_index].asset_available
                ? regions_[region_index].slot
                : kTrafficNoRegionSlot,
            region_index);
        records_.push_back(std::move(record));
    }
}

const TrafficRuntimeRecord* TrafficRuntimeList::record_for_node(
    std::size_t node) const noexcept {
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [node](const TrafficRuntimeRecord& record) {
            return record.source_node() == node;
        });
    return found == records_.end() ? nullptr : &*found;
}

bool TrafficRuntimeList::activate_node(
    std::size_t node,
    std::uint32_t argument) noexcept {
    for (TrafficRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            record.activate(argument);
            return true;
        }
    }
    return false;
}

bool TrafficRuntimeList::deactivate_node(std::size_t node) noexcept {
    for (TrafficRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            record.deactivate();
            return true;
        }
    }
    return false;
}

const TrafficAssetRegion& TrafficRuntimeList::region(std::size_t index) const {
    if (index >= regions_.size()) {
        throw FormatError("traffic PSX region index is outside the runtime table");
    }
    return regions_[index];
}

} // namespace opentony::trg
