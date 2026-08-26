#include "rail_runtime.hpp"

#include "level_trigger_state.hpp"

#include <limits>

namespace opentony::trg {

void RailRuntimeList::build(const TrgFile& file) {
    records_.clear();
    for (const NodeView& node : file.nodes()) {
        if (node.type != 10 && node.type != 11) {
            continue;
        }
        if (node.index > std::numeric_limits<std::uint16_t>::max()) {
            throw FormatError("TRG rail node index does not fit the runtime u16 field");
        }
        RailRuntimeRecord record{};
        record.source_node_ = node.index;
        record.trigger_word_ = file.node_trigger_flags(node.index);
        // FUN_004aa8c0's constructor owns the initial state byte; the
        // state-changing consumers are the independently proven pulse/kill
        // helpers. Keep the constructor image zero until a level service
        // supplies the source object's initial state.
        record.set_state(0);
        const std::uint16_t source = static_cast<std::uint16_t>(node.index);
        record.raw_[0x06] = static_cast<std::byte>(source & 0xffU);
        record.raw_[0x07] = static_cast<std::byte>(source >> 8U);
        records_.push_back(record);
    }
}

void RailRuntimeList::synchronize_states(const LevelTriggerState& state) {
    for (RailRuntimeRecord& record : records_) {
        const TriggerObjectState* object = state.object(record.source_node_);
        if (object != nullptr) {
            record.set_state(object->trigger_state);
        }
    }
}

void RailRuntimeList::pulse_node(std::size_t node) {
    for (RailRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            record.set_state(1);
            return;
        }
    }
}

void RailRuntimeList::kill_node(std::size_t node) {
    for (RailRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            record.set_state(0);
            return;
        }
    }
}

const RailRuntimeRecord* RailRuntimeList::record_for_node(std::size_t node) const noexcept {
    for (const RailRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            return &record;
        }
    }
    return nullptr;
}

} // namespace opentony::trg
