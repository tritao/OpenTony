#include "trigger_factory_runtime.hpp"

#include <algorithm>
#include <limits>

namespace opentony::trg {

std::size_t TriggerFactoryRuntimeRecord::allocation_size() const noexcept {
    return kind_ == TriggerFactoryRuntimeKind::ObjectCb
        ? kTriggerObjectCbRuntimeSize
        : kTriggerObject192RuntimeSize;
}

std::uint16_t TriggerFactoryRuntimeRecord::u16(std::size_t offset) const noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U));
}

std::uint32_t TriggerFactoryRuntimeRecord::u32(std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 3])) << 24U));
}

std::int32_t TriggerFactoryRuntimeRecord::s32(std::size_t offset) const noexcept {
    return static_cast<std::int32_t>(u32(offset));
}

void TriggerFactoryRuntimeRecord::put16(
    std::size_t offset,
    std::uint16_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value & 0xffU);
    raw_[offset + 1] = static_cast<std::byte>(value >> 8U);
}

void TriggerFactoryRuntimeRecord::put32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    put16(offset, static_cast<std::uint16_t>(value));
    put16(offset + 2, static_cast<std::uint16_t>(value >> 16U));
}

std::uint16_t TriggerFactoryRuntimeRecord::subtype() const noexcept {
    return kind_ == TriggerFactoryRuntimeKind::ObjectCb
        ? u16(0x3c)
        : u16(0x1f2);
}

std::uint16_t TriggerFactoryRuntimeRecord::object_flags() const noexcept {
    return u16(0x04);
}

std::uint32_t TriggerFactoryRuntimeRecord::activation_argument() const noexcept {
    return u32(0x64);
}

std::uint16_t TriggerFactoryRuntimeRecord::activation_flags() const noexcept {
    return u16(0x6a);
}

std::uint8_t TriggerFactoryRuntimeRecord::active_byte() const noexcept {
    return std::to_integer<std::uint8_t>(raw_[0x16c]);
}

std::uint16_t TriggerFactoryRuntimeRecord::mode_word() const noexcept {
    return u16(0x1a0);
}

std::array<std::int32_t, 3> TriggerFactoryRuntimeRecord::position() const noexcept {
    return {s32(0x08), s32(0x0c), s32(0x10)};
}

std::array<std::uint16_t, 3>
TriggerFactoryRuntimeRecord::constructor_parameters() const noexcept {
    return {u16(0x14), u16(0x16), u16(0x18)};
}

std::span<const std::byte> TriggerFactoryRuntimeRecord::raw_record() const noexcept {
    return std::span<const std::byte>(raw_).first(allocation_size());
}

void TriggerFactoryRuntimeRecord::activate(std::uint32_t argument) noexcept {
    put32(0x64, argument);
    put16(0x6a, static_cast<std::uint16_t>(u16(0x6a) | 1U));
}

void TriggerFactoryRuntimeRecord::deactivate() noexcept {
    put16(0x6a, static_cast<std::uint16_t>(u16(0x6a) & ~1U));
}

void TriggerFactoryRuntimeList::populate(
    TriggerFactoryRuntimeRecord& record,
    const TrgFile& file,
    const NodeView& node,
    const TriggerObjectState* state) {
    if (node.index > std::numeric_limits<std::uint16_t>::max()) {
        throw FormatError("TRG factory node index does not fit the runtime u16 field");
    }
    const std::uint16_t subtype = file.node_subtype(node.index);
    record.kind_ = subtype == 0xcb
        ? TriggerFactoryRuntimeKind::ObjectCb
        : TriggerFactoryRuntimeKind::Object192;
    record.source_node_ = node.index;
    const std::array<std::int32_t, 3> position = file.node_position(node.index);
    const std::array<std::uint16_t, 3> parameters = file.node_orientation(node.index);
    const std::uint16_t default_flags = subtype == 0xcb ? 0x0041 : 0x0111;
    record.put32(
        0x04,
        state == nullptr ? default_flags : state->flags);
    record.put32(0x08, static_cast<std::uint32_t>(position[0]));
    record.put32(0x0c, static_cast<std::uint32_t>(position[1]));
    record.put32(0x10, static_cast<std::uint32_t>(position[2]));
    record.put16(0x14, parameters[0]);
    record.put16(0x16, parameters[1]);
    record.put16(0x18, parameters[2]);
    record.put16(0xb0, static_cast<std::uint16_t>(node.index));
    record.raw_[0x16c] = std::byte{1};
    if (record.kind_ == TriggerFactoryRuntimeKind::ObjectCb) {
        record.put16(0x3c, subtype);
        record.put32(0x1ec, 0xffffffffU);
    } else {
        record.put16(0x1a0, 0x20);
        record.put16(0x1f2, subtype);
        record.put32(0x208, 0xffffffffU);
    }
}

void TriggerFactoryRuntimeList::build(
    const TrgFile& file,
    const LevelTriggerState* state) {
    records_.clear();
    for (const NodeView& node : file.nodes()) {
        if (node.type != 1) {
            continue;
        }
        const std::uint16_t subtype = file.node_subtype(node.index);
        if (subtype != 0xcb && subtype != 0x192) {
            continue;
        }
        const TriggerObjectState* source =
            state == nullptr ? nullptr : state->object(node.index);
        TriggerFactoryRuntimeRecord record{};
        populate(record, file, node, source);
        records_.push_back(std::move(record));
    }
}

void TriggerFactoryRuntimeList::synchronize(const LevelTriggerState& state) {
    for (TriggerFactoryRuntimeRecord& record : records_) {
        const TriggerObjectState* source = state.object(record.source_node());
        if (source != nullptr) {
            record.put32(0x04, source->flags);
        }
    }
}

const TriggerFactoryRuntimeRecord* TriggerFactoryRuntimeList::record_for_node(
    std::size_t node) const noexcept {
    const auto found = std::find_if(
        records_.begin(), records_.end(),
        [node](const TriggerFactoryRuntimeRecord& record) {
            return record.source_node() == node;
        });
    return found == records_.end() ? nullptr : &*found;
}

bool TriggerFactoryRuntimeList::activate_node(
    std::size_t node,
    std::uint32_t argument) noexcept {
    for (TriggerFactoryRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            record.activate(argument);
            return true;
        }
    }
    return false;
}

bool TriggerFactoryRuntimeList::deactivate_node(std::size_t node) noexcept {
    for (TriggerFactoryRuntimeRecord& record : records_) {
        if (record.source_node() == node) {
            record.deactivate();
            return true;
        }
    }
    return false;
}

} // namespace opentony::trg
