#pragma once

#include "trg_runtime.hpp"
#include "../assets/psx_catalog.hpp"
#include "../assets/psx_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace opentony::trg {

inline constexpr std::size_t kPowerupRuntimeRecordSize = 0x100;

struct PowerupModelSelection {
    const char* resource{};
    std::uint32_t checksum{};
};

// A value-preserving image of the 0x100-byte type-5 object created by the
// retail powerup factory.  Unknown pointers (the list links and glow object)
// remain process-neutral; the source node, position, subtype, and resolved
// PSX model are exposed separately for safe native consumers.
class PowerupRuntimeRecord final {
public:
    [[nodiscard]] std::size_t source_node() const noexcept { return source_node_; }
    [[nodiscard]] std::uint16_t subtype() const noexcept;
    [[nodiscard]] const std::string& resource() const noexcept { return resource_; }
    [[nodiscard]] std::uint32_t model_name_checksum() const noexcept {
        return model_name_checksum_;
    }
    [[nodiscard]] std::optional<std::size_t> model_index() const noexcept {
        return model_index_;
    }
    [[nodiscard]] std::array<std::int32_t, 3> position() const noexcept;
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept { return raw_; }

private:
    friend class PowerupRuntimeList;
    std::array<std::byte, kPowerupRuntimeRecordSize> raw_{};
    std::size_t source_node_{};
    std::string resource_;
    std::uint32_t model_name_checksum_{};
    std::optional<std::size_t> model_index_;

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept;
    [[nodiscard]] std::int32_t s32(std::size_t offset) const noexcept;
    void put16(std::size_t offset, std::uint16_t value) noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

class PowerupRuntimeList final {
public:
    void build(
        const TrgFile& file,
        const assets::PsxAssetCatalog* catalog = nullptr);
    void resolve_runtime_models(
        const assets::PsxRuntimeEnvironment* items_runtime,
        const assets::PsxRuntimeEnvironment* medals_runtime);

    [[nodiscard]] const std::vector<PowerupRuntimeRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] const PowerupRuntimeRecord* record_for_node(
        std::size_t node) const noexcept;

private:
    std::vector<PowerupRuntimeRecord> records_;

    [[nodiscard]] static std::optional<PowerupModelSelection> selection_for(
        std::uint16_t subtype) noexcept;
};

} // namespace opentony::trg
