#pragma once

#include "player_state.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace opentony::runtime {

inline constexpr std::size_t kGameplaySkaterObjectSize = 0x3538U;
inline constexpr std::size_t kGameplayCameraObjectSize = 0x674U;

// Value-owned camera allocation matching the runtime ownership edge at
// skater+0x29b0. Pointer fields are represented by object relationships; raw
// scalar fields retain their retail offsets for trace/export consumers.
class CameraRuntimeObject final {
public:
    explicit CameraRuntimeObject(std::size_t parent_player_index = 0) noexcept;

    [[nodiscard]] std::size_t allocation_size() const noexcept { return raw_.size(); }
    [[nodiscard]] std::size_t parent_player_index() const noexcept { return parent_player_index_; }
    [[nodiscard]] std::uint32_t mode() const noexcept;
    [[nodiscard]] std::uint32_t update_tick() const noexcept;
    [[nodiscard]] const std::vector<std::byte>& raw() const noexcept { return raw_; }

    void set_mode(std::uint32_t value) noexcept;
    void set_update_tick(std::uint32_t value) noexcept;

private:
    std::vector<std::byte> raw_;
    std::size_t parent_player_index_{};
};

// Native ownership shell for the 0x3538 gameplay skater allocation. It keeps
// the existing PlayerState semantic model alongside a raw offset-preserving
// record so asset/model, camera, physics, and renderer consumers can converge
// on one object without manufacturing process-local pointers.
class SkaterRuntimeObject final {
public:
    SkaterRuntimeObject(
        std::size_t player_index,
        std::uint8_t psx_region_slot,
        std::uint16_t model_index,
        FixedPosition position = {}) noexcept;

    [[nodiscard]] std::size_t allocation_size() const noexcept { return raw_.size(); }
    [[nodiscard]] std::size_t player_index() const noexcept { return player_index_; }
    [[nodiscard]] std::uint8_t psx_region_slot() const noexcept;
    [[nodiscard]] std::uint16_t model_index() const noexcept;
    [[nodiscard]] const CameraRuntimeObject& camera() const noexcept { return camera_; }
    [[nodiscard]] CameraRuntimeObject& camera() noexcept { return camera_; }
    [[nodiscard]] const PlayerState& player() const noexcept { return player_; }
    [[nodiscard]] PlayerState& player() noexcept { return player_; }
    [[nodiscard]] const SkaterRuntimeObject* peer() const noexcept { return peer_; }
    [[nodiscard]] const std::vector<std::byte>& raw() const noexcept { return raw_; }

    // The retail player setup writes the selected region/model fields before
    // the later animation and renderer consumers see this allocation.
    void set_psx_binding(
        std::uint8_t psx_region_slot,
        std::uint16_t model_index) noexcept;
    void set_peer(const SkaterRuntimeObject* peer) noexcept { peer_ = peer; }

private:
    std::vector<std::byte> raw_;
    std::size_t player_index_{};
    CameraRuntimeObject camera_;
    PlayerState player_;
    const SkaterRuntimeObject* peer_{};
};

} // namespace opentony::runtime
