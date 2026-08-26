#pragma once

#include "level_trigger_state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace opentony::trg {

enum class TriggerFactoryRuntimeKind : std::uint8_t {
    ObjectCb,
    Object192,
};

inline constexpr std::size_t kTriggerObjectCbRuntimeSize = 0x1f4;
inline constexpr std::size_t kTriggerObject192RuntimeSize = 0x218;

// Byte-preserving image of one of the two non-traffic type-1 factory
// allocations. Host pointers (vtable, links, context, and script cursor) are
// intentionally left zero; scalar constructor fields remain at their retail
// offsets and can be consumed without depending on a 32-bit process layout.
class TriggerFactoryRuntimeRecord final {
public:
    [[nodiscard]] TriggerFactoryRuntimeKind kind() const noexcept { return kind_; }
    [[nodiscard]] std::size_t source_node() const noexcept { return source_node_; }
    [[nodiscard]] std::size_t allocation_size() const noexcept;
    [[nodiscard]] std::uint16_t subtype() const noexcept;
    [[nodiscard]] std::uint16_t object_flags() const noexcept;
    [[nodiscard]] std::uint32_t activation_argument() const noexcept;
    [[nodiscard]] std::uint16_t activation_flags() const noexcept;
    [[nodiscard]] std::uint8_t active_byte() const noexcept;
    [[nodiscard]] std::uint16_t mode_word() const noexcept;
    [[nodiscard]] std::array<std::int32_t, 3> position() const noexcept;
    [[nodiscard]] std::array<std::uint16_t, 3> constructor_parameters() const noexcept;
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept;
    void activate(std::uint32_t argument) noexcept;
    void deactivate() noexcept;

private:
    friend class TriggerFactoryRuntimeList;
    TriggerFactoryRuntimeKind kind_{TriggerFactoryRuntimeKind::ObjectCb};
    std::size_t source_node_{};
    std::array<std::byte, kTriggerObject192RuntimeSize> raw_{};

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept;
    [[nodiscard]] std::int32_t s32(std::size_t offset) const noexcept;
    void put16(std::size_t offset, std::uint16_t value) noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

// Runtime allocation table for the two proven type-1 constructor families.
// It is separate from LevelSceneRegistry because these objects live on the
// trigger/object-manager lists, not in the static PSX scene-object array.
class TriggerFactoryRuntimeList final {
public:
    void build(const TrgFile& file, const LevelTriggerState* state = nullptr);
    void synchronize(const LevelTriggerState& state);

    [[nodiscard]] const std::vector<TriggerFactoryRuntimeRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] std::vector<TriggerFactoryRuntimeRecord>& records() noexcept {
        return records_;
    }
    [[nodiscard]] const TriggerFactoryRuntimeRecord* record_for_node(
        std::size_t node) const noexcept;
    [[nodiscard]] bool activate_node(
        std::size_t node,
        std::uint32_t argument) noexcept;
    [[nodiscard]] bool deactivate_node(std::size_t node) noexcept;

private:
    std::vector<TriggerFactoryRuntimeRecord> records_;

    static void populate(
        TriggerFactoryRuntimeRecord& record,
        const TrgFile& file,
        const NodeView& node,
        const TriggerObjectState* state);
};

} // namespace opentony::trg
