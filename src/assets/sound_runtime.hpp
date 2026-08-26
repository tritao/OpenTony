#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace opentony::assets {

class SoundRuntimeError final : public std::runtime_error {
public:
    explicit SoundRuntimeError(const std::string& message)
        : std::runtime_error(message) {}
};

inline constexpr std::size_t kSoundDescriptionStride = 0x34U;
inline constexpr std::size_t kSoundDescriptionNameSize = 0x20U;
inline constexpr std::size_t kSoundRuntimeSlotSize = 0x28U;
inline constexpr std::size_t kMaximumSoundDescriptions = 0x42U;
inline constexpr std::size_t kMaximumSoundSlots = 0x80U;

// Runtime fields recovered from the selected VAB bank table. The original
// table is an inline 0x34-byte array; owning the bounded name keeps the same
// resource-name and post-start flag contract without borrowing executable
// memory.
struct SoundDescription {
    std::int32_t sound_id{-1};
    std::string resource_name;
    std::uint32_t post_start_flags{};
};

// The DirectSound pointer fields are deliberately represented by the owning
// slot index. Scalar state remains at the retail +0x20 offset.
class SoundRuntimeSlot final {
public:
    [[nodiscard]] std::size_t allocation_size() const noexcept { return raw_.size(); }
    [[nodiscard]] std::size_t description_index() const noexcept {
        return description_index_;
    }
    [[nodiscard]] std::uint32_t state_flags() const noexcept;
    [[nodiscard]] std::span<const std::byte> raw() const noexcept { return raw_; }

private:
    friend class SoundBankRuntime;
    explicit SoundRuntimeSlot(std::size_t description_index) noexcept;

    std::array<std::byte, kSoundRuntimeSlotSize> raw_{};
    std::size_t description_index_{};

    void set_state_flags(std::uint32_t value) noexcept;
    [[nodiscard]] std::uint32_t u32(std::size_t offset) const noexcept;
    void put32(std::size_t offset, std::uint32_t value) noexcept;
};

// Native counterpart of the PC bank/voice handoff at 0x004f2960,
// 0x004f2b40, and 0x004f2e20. It stops at the recovered 0x42 description
// bound, maps names to audio/<name>.wav, and keeps the runtime slot capacity
// and +0x20 state flag observable.
class SoundBankRuntime final {
public:
    void build(std::span<const SoundDescription> descriptions);

    [[nodiscard]] const std::vector<SoundDescription>& descriptions() const noexcept {
        return descriptions_;
    }
    [[nodiscard]] const std::vector<SoundRuntimeSlot>& slots() const noexcept {
        return slots_;
    }
    [[nodiscard]] const SoundRuntimeSlot& slot(std::size_t index) const;
    [[nodiscard]] std::optional<std::size_t> description_for_sound(
        std::int32_t sound_id) const noexcept;
    [[nodiscard]] std::string resource_path(std::int32_t sound_id) const;

    // Called by the future audio-device adapter after WAV decode and buffer
    // creation. Repeated publication for one description reuses its slot.
    [[nodiscard]] std::size_t publish_loaded_sound(std::size_t description_index);
    void mark_started(std::size_t slot_index);
    void mark_stopped(std::size_t slot_index) noexcept;

private:
    std::vector<SoundDescription> descriptions_;
    std::vector<SoundRuntimeSlot> slots_;
    std::vector<std::optional<std::size_t>> description_slots_;
};

} // namespace opentony::assets
