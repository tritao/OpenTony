#pragma once

#include "pre_asset.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::assets {

inline constexpr std::size_t kRuntimePreManagerAllocationSize = 0x144;
inline constexpr std::size_t kRuntimePreSlotCount = 16;
inline constexpr std::size_t kRuntimePreNameSize = 0x10;

struct PreEmbeddedResourceView {
    std::size_t container_slot{};
    std::string container_name;
    std::string resource_name;
    std::span<const std::byte> payload{};
};

// Value-owned counterpart of the retail PRE manager. Retail keeps one loaded
// container buffer in each of sixteen slots and scans those buffers for the
// requested embedded resource. The native manager keeps the same ownership
// boundary while letting PreArchive validate the inline records eagerly.
class PreRuntimeManager final {
public:
    PreRuntimeManager() = default;

    static PreRuntimeManager create() { return PreRuntimeManager{}; }

    std::size_t load(std::string name, std::vector<std::byte> bytes);
    std::size_t load_file(std::string name, const std::string& path);

    [[nodiscard]] const PreArchive& container(std::size_t slot) const;
    [[nodiscard]] const std::string& container_name(std::size_t slot) const;
    [[nodiscard]] std::size_t loaded_count() const noexcept { return loaded_count_; }
    [[nodiscard]] std::size_t allocation_size() const noexcept {
        return kRuntimePreManagerAllocationSize;
    }

    // The retail lookup is case-insensitive and returns a pointer into the
    // retained PRE buffer. The returned span remains valid until this manager
    // unloads the owning container.
    [[nodiscard]] std::optional<PreEmbeddedResourceView> find_embedded(
        std::string_view resource_name) const;

    void unload(std::string_view name);

private:
    struct Slot {
        std::string name;
        PreArchive archive;
    };

    std::array<std::optional<Slot>, kRuntimePreSlotCount> slots_{};
    std::size_t loaded_count_{};

    static void validate_container_name(std::string_view name);
};

} // namespace opentony::assets
