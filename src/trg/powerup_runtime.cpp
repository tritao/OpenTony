#include "powerup_runtime.hpp"

#include <algorithm>
#include <limits>

namespace opentony::trg {

std::uint16_t PowerupRuntimeRecord::u16(std::size_t offset) const noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U));
}

std::int32_t PowerupRuntimeRecord::s32(std::size_t offset) const noexcept {
    const std::uint32_t value = static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 3])) << 24U));
    return static_cast<std::int32_t>(value);
}

void PowerupRuntimeRecord::put16(std::size_t offset, std::uint16_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value & 0xffU);
    raw_[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void PowerupRuntimeRecord::put32(std::size_t offset, std::uint32_t value) noexcept {
    put16(offset, static_cast<std::uint16_t>(value));
    put16(offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

std::uint16_t PowerupRuntimeRecord::subtype() const noexcept {
    return u16(0x3c);
}

std::array<std::int32_t, 3> PowerupRuntimeRecord::position() const noexcept {
    return {s32(0x08), s32(0x0c), s32(0x10)};
}

std::optional<PowerupModelSelection> PowerupRuntimeList::selection_for(
    std::uint16_t subtype) noexcept {
    switch (subtype) {
    case 4:
        return PowerupModelSelection{"items", 0x2328a71cU};
    case 5:
        return PowerupModelSelection{"items", 0x311d55d4U};
    case 6:
        return PowerupModelSelection{"items", 0x2ebf22caU};
    case 10:
        return PowerupModelSelection{"items", 0x29b68a16U};
    case 15:
        return PowerupModelSelection{"items", 0x34524351U};
    case 16:
    case 18:
        return PowerupModelSelection{"items", 0x7c1b2c4aU};
    case 24:
        return PowerupModelSelection{"items", 0x694ed947U};
    case 25:
        return PowerupModelSelection{"items", 0x260f4f80U};
    case 26:
        return PowerupModelSelection{"items", 0xcc4e141fU};
    case 0x664:
        return PowerupModelSelection{"skmedals", 0x54636518U};
    case 0x665:
        return PowerupModelSelection{"skmedals", 0xba6d0434U};
    case 0x666:
        return PowerupModelSelection{"skmedals", 0x2364558eU};
    default:
        // Subtype 33 is selected through a level-dependent branch; no static
        // checksum is assigned until that producer is recovered.
        return std::nullopt;
    }
}

void PowerupRuntimeList::build(
    const TrgFile& file,
    const assets::PsxAssetCatalog* catalog) {
    records_.clear();
    for (const NodeView& node : file.nodes()) {
        if (node.type != 5) {
            continue;
        }
        if (node.index > std::numeric_limits<std::uint16_t>::max()) {
            throw FormatError("TRG powerup node index does not fit the runtime u16 field");
        }
        const std::uint16_t subtype = file.node_subtype(node.index);
        PowerupRuntimeRecord record{};
        record.source_node_ = node.index;
        record.resource_ = {};
        record.put32(0x08, static_cast<std::uint32_t>(file.node_position(node.index)[0]));
        record.put32(0x0c, static_cast<std::uint32_t>(file.node_position(node.index)[1]));
        record.put32(0x10, static_cast<std::uint32_t>(file.node_position(node.index)[2]));
        record.put16(0x3c, subtype);
        record.put16(0xb0, static_cast<std::uint16_t>(node.index));

        const auto selection = selection_for(subtype);
        if (selection.has_value()) {
            record.resource_ = selection->resource;
            record.model_name_checksum_ = selection->checksum;
            if (catalog != nullptr && catalog->contains(selection->resource)) {
                const assets::PsxArchive& archive = catalog->load(selection->resource);
                const auto found = std::find(
                    archive.model_names().begin(),
                    archive.model_names().end(),
                    selection->checksum);
                if (found != archive.model_names().end()) {
                    record.model_index_ = static_cast<std::size_t>(
                        std::distance(archive.model_names().begin(), found));
                    record.put16(0x1a, static_cast<std::uint16_t>(*record.model_index_));
                }
            }
        }
        records_.push_back(std::move(record));
    }
}

void PowerupRuntimeList::resolve_runtime_models(
    const assets::PsxRuntimeEnvironment* items_runtime,
    const assets::PsxRuntimeEnvironment* medals_runtime) {
    for (PowerupRuntimeRecord& record : records_) {
        if (record.model_index_.has_value()) {
            continue;
        }
        const assets::PsxRuntimeEnvironment* runtime = nullptr;
        if (record.resource_ == "items") {
            runtime = items_runtime;
        } else if (record.resource_ == "skmedals") {
            runtime = medals_runtime;
        }
        if (runtime == nullptr) {
            continue;
        }
        const auto found = std::find(
            runtime->source_archive().model_names().begin(),
            runtime->source_archive().model_names().end(),
            record.model_name_checksum_);
        if (found == runtime->source_archive().model_names().end()) {
            continue;
        }
        const std::size_t model_index = static_cast<std::size_t>(
            std::distance(runtime->source_archive().model_names().begin(), found));
        if (model_index > std::numeric_limits<std::uint16_t>::max()) {
            throw FormatError("TRG powerup model index does not fit the runtime field");
        }
        record.model_index_ = model_index;
        record.put16(0x1a, static_cast<std::uint16_t>(model_index));
    }
}

const PowerupRuntimeRecord* PowerupRuntimeList::record_for_node(
    std::size_t node) const noexcept {
    const auto found = std::find_if(
        records_.begin(),
        records_.end(),
        [node](const PowerupRuntimeRecord& record) {
            return record.source_node() == node;
        });
    return found == records_.end() ? nullptr : &*found;
}

} // namespace opentony::trg
