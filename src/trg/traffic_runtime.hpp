#pragma once

#include "trg_runtime.hpp"
#include "../assets/psx_catalog.hpp"
#include "../assets/psx_runtime.hpp"
#include "../assets/pkr_asset.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace opentony::trg {

inline constexpr std::size_t kTrafficRuntimeRecordSize = 0x1e8;
inline constexpr std::uint8_t kTrafficNoRegionSlot = 0xff;

struct TrafficAssetRegion final {
    std::string resource;
    std::uint8_t slot{kTrafficNoRegionSlot};
    std::string asset_path;
    bool asset_available{};
    // Package-backed regions own their decoded PSX archive here. Loose-file
    // regions point at the catalog-owned archive through runtime instead.
    std::shared_ptr<assets::PsxArchive> owned_archive;
    std::optional<assets::PsxRuntimeEnvironment> runtime;
};

// Value-preserving image of the 0x1e8-byte traffic constructor allocation.
// Process-local vtable/list/audio pointers remain zero; constructor scalar
// writes and the source-node/model-region bridge are retained at their
// proven offsets.
class TrafficRuntimeRecord final {
public:
    [[nodiscard]] std::size_t source_node() const noexcept { return source_node_; }
    [[nodiscard]] std::uint16_t subtype() const noexcept;
    [[nodiscard]] const std::string& resource() const noexcept { return resource_; }
    [[nodiscard]] std::int32_t sound_id() const noexcept { return sound_id_; }
    [[nodiscard]] bool sound_setup_disabled() const noexcept {
        return sound_setup_disabled_;
    }
    [[nodiscard]] std::uint8_t region_slot() const noexcept;
    [[nodiscard]] std::size_t region_index() const noexcept { return region_index_; }
    [[nodiscard]] std::uint16_t model_index() const noexcept;
    [[nodiscard]] std::array<std::int32_t, 3> position() const noexcept;
    [[nodiscard]] std::array<std::uint16_t, 3> constructor_parameters() const noexcept;
    [[nodiscard]] std::array<std::int32_t, 3> motion_response() const noexcept;
    [[nodiscard]] std::uint32_t activation_argument() const noexcept;
    [[nodiscard]] std::uint16_t activation_flags() const noexcept;
    [[nodiscard]] std::uint32_t interaction_latch() const noexcept;
    [[nodiscard]] std::span<const std::byte> raw_record() const noexcept { return raw_; }

    void set_motion_response(std::array<std::int32_t, 3> response) noexcept;
    void activate(std::uint32_t argument) noexcept;
    void deactivate() noexcept;
    void set_interaction_latch(std::uint32_t value) noexcept;

private:
    friend class TrafficRuntimeList;
    std::array<std::byte, kTrafficRuntimeRecordSize> raw_{};
    std::size_t source_node_{};
    std::string resource_;
    std::int32_t sound_id_{-1};
    bool sound_setup_disabled_{};
    std::size_t region_index_{CommandPointRuntime::npos};

    [[nodiscard]] std::uint16_t u16(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept;
    [[nodiscard]] std::int32_t s32(std::size_t offset) const noexcept;
    void put16(std::size_t offset, std::uint16_t value) noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

// Runtime counterpart of the type-1 traffic factory. It separates the
// 0x1e8-byte object records from the shared named PSX region table, matching
// the retail fact that destroying one car does not release C_TAXI.PSX or the
// other shared regions.
class TrafficRuntimeList final {
public:
    void build(
        const TrgFile& file,
        const assets::PsxAssetCatalog* catalog = nullptr);
    void build(const TrgFile& file, const assets::PkrArchive& package);

    [[nodiscard]] const std::vector<TrafficRuntimeRecord>& records() const noexcept {
        return records_;
    }
    [[nodiscard]] std::vector<TrafficRuntimeRecord>& records() noexcept {
        return records_;
    }
    [[nodiscard]] const std::vector<TrafficAssetRegion>& regions() const noexcept {
        return regions_;
    }
    [[nodiscard]] const TrafficRuntimeRecord* record_for_node(
        std::size_t node) const noexcept;
    [[nodiscard]] bool activate_node(
        std::size_t node,
        std::uint32_t argument) noexcept;
    [[nodiscard]] bool deactivate_node(std::size_t node) noexcept;
    [[nodiscard]] const TrafficAssetRegion& region(std::size_t index) const;

private:
    std::vector<TrafficRuntimeRecord> records_;
    std::vector<TrafficAssetRegion> regions_;

    struct Selection {
        const char* resource{};
        std::int32_t sound_id{-1};
        bool sound_setup_disabled{};
    };

    [[nodiscard]] static std::optional<Selection> selection_for(
        std::uint16_t subtype) noexcept;
    [[nodiscard]] std::size_t ensure_region(
        std::string resource,
        const assets::PsxAssetCatalog* catalog,
        const assets::PkrArchive* package);
    static void populate_record(
        TrafficRuntimeRecord& record,
        const TrgFile& file,
        const NodeView& node,
        const Selection& selection,
        std::size_t sequence,
        std::uint8_t region_slot,
        std::size_t region_index);
};

} // namespace opentony::trg
