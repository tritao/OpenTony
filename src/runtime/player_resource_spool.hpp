#pragma once

#include "../assets/psh_asset.hpp"
#include "../assets/psx_asset.hpp"
#include "../assets/pkr_asset.hpp"
#include "../assets/resource_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::runtime {

inline constexpr std::size_t kPlayerSpoolManagerSize = 0xa10U;
inline constexpr std::size_t kPlayerSpoolEntryCount = 0x40U;
inline constexpr std::size_t kPlayerSpoolEntrySize = 0x28U;
inline constexpr std::uint32_t kPlayerSpoolNoPadSize = 0xffffffffU;

enum class PlayerSpoolResourceKind : std::uint8_t {
    DirectPsx = 0,
    PshRegion = 1,
};

struct PlayerSpoolLoadedResource {
    std::size_t queue_index{};
    PlayerSpoolResourceKind kind{};
    assets::ResourceSourceKind source_kind{};
    std::size_t allocation_size{};
    std::optional<assets::PshManifest> psh;
    std::optional<assets::PsxArchive> psx;
};

// Native value-owned counterpart of the proven player resource manager at
// 0x004b5200. The raw manager keeps the 0x28-byte entry offsets and counters;
// parsed files are owned separately because the retail pointer fields are
// process-local allocations.
class PlayerResourceSpool final {
public:
    PlayerResourceSpool() noexcept;

    PlayerResourceSpool(const PlayerResourceSpool&) = delete;
    PlayerResourceSpool& operator=(const PlayerResourceSpool&) = delete;

    void reset() noexcept;

    [[nodiscard]] std::size_t enqueue(
        std::string_view base_name,
        PlayerSpoolResourceKind kind,
        std::int32_t heap_selector = 1,
        std::uint32_t request_size = kPlayerSpoolNoPadSize);

    [[nodiscard]] bool start_next();
    [[nodiscard]] std::size_t load_current(
        const std::string& asset_root,
        const assets::PreRuntimeManager* pre = nullptr);
    [[nodiscard]] std::size_t load_current(
        const assets::PkrArchive& package,
        const assets::PreRuntimeManager* pre = nullptr);
    void complete_current();
    // Drops the owned runtime result at one queue index. Direct-file release
    // clears processed; PSH release retains it while resetting the region
    // handle, matching the two retail release branches.
    void release(std::size_t queue_index);

    [[nodiscard]] std::size_t queued_count() const noexcept;
    [[nodiscard]] std::size_t consume_index() const noexcept;
    [[nodiscard]] std::uint32_t state() const noexcept;
    [[nodiscard]] bool busy() const noexcept { return state() != 0; }
    [[nodiscard]] std::size_t current_index() const noexcept;

    [[nodiscard]] bool processed(std::size_t index) const;
    [[nodiscard]] std::string name(std::size_t index) const;
    [[nodiscard]] PlayerSpoolResourceKind kind(std::size_t index) const;
    [[nodiscard]] std::int32_t heap_selector(std::size_t index) const;

    // 0x004b5370 stages its fourth argument at the next entry stride's base
    // word. Keep it as a named side channel instead of mislabeling a normal
    // entry field (the final entry would overlap manager counters).
    [[nodiscard]] std::uint32_t request_size_staging(
        std::size_t index) const;

    [[nodiscard]] const PlayerSpoolLoadedResource* loaded(
        std::size_t queue_index) const noexcept;
    [[nodiscard]] const std::array<std::byte, kPlayerSpoolManagerSize>& raw()
        const noexcept {
        return raw_;
    }

private:
    std::array<std::byte, kPlayerSpoolManagerSize> raw_{};
    std::array<std::uint32_t, kPlayerSpoolEntryCount> request_sizes_{};
    std::array<std::optional<PlayerSpoolLoadedResource>, kPlayerSpoolEntryCount>
        loaded_{};

    [[nodiscard]] std::size_t entry_offset(std::size_t index) const;
    [[nodiscard]] std::uint32_t read_u32(std::size_t offset) const noexcept;
    [[nodiscard]] std::uint8_t read_u8(std::size_t offset) const noexcept;
    void write_u32(std::size_t offset, std::uint32_t value) noexcept;
    void write_u8(std::size_t offset, std::uint8_t value) noexcept;
    [[nodiscard]] std::size_t load_current(
        assets::ResourceBackend& backend,
        std::string_view resource_path,
        const assets::PreRuntimeManager* pre);
};

} // namespace opentony::runtime
