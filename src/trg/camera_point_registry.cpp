#include "camera_point_registry.hpp"

#include <limits>
#include <stdexcept>

namespace opentony::trg {

void CameraPointRegistry::build(const TrgFile& file) {
    entries_.clear();
    for (const NodeView& node : file.nodes()) {
        if (node.type != 13) {
            continue;
        }
        if (entries_.size() >= kMaximumEntries) {
            throw FormatError("TRG type-13 camera-point registry exceeds 0x46 entries");
        }
        if (node.index > std::numeric_limits<std::uint16_t>::max()) {
            throw FormatError("TRG camera-point node index does not fit the retail u16 registry");
        }
        entries_.push_back(CameraPointEntry{
            entries_.size(),
            node.index,
            file.camera_point(node.index),
        });
    }
}

std::optional<CameraPointHandoff> CameraPointRegistry::select_nearest(
    const DistanceEvaluator& distance,
    std::int32_t limit) const {
    if (!distance || limit <= 0) {
        return std::nullopt;
    }
    const CameraPointEntry* selected = nullptr;
    std::int32_t selected_distance = std::numeric_limits<std::int32_t>::max();
    for (const CameraPointEntry& candidate : entries_) {
        const std::optional<std::int32_t> measured = distance(candidate);
        if (!measured.has_value() || *measured < 0 || *measured >= limit) {
            continue;
        }
        if (selected == nullptr || *measured < selected_distance) {
            selected = &candidate;
            selected_distance = *measured;
        }
    }
    if (selected == nullptr) {
        return std::nullopt;
    }
    const bool primary_mode = selected->record.selection_state_word == 1;
    return CameraPointHandoff{
        selected->registry_index,
        selected->source_node,
        selected->record.position,
        primary_mode ? 1U : 2U,
        primary_mode ? 0x400U : 0x800U,
        selected->record.transition_variant,
        selected_distance,
    };
}

const CameraPointEntry& CameraPointRegistry::entry(std::size_t index) const {
    if (index >= entries_.size()) {
        throw FormatError("camera-point registry index is outside the active entries");
    }
    return entries_[index];
}

} // namespace opentony::trg
