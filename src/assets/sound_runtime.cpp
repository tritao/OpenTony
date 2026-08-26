#include "sound_runtime.hpp"

#include <algorithm>

namespace opentony::assets {

SoundRuntimeSlot::SoundRuntimeSlot(std::size_t description_index) noexcept
    : description_index_(description_index) {}

std::uint32_t SoundRuntimeSlot::u32(std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 1U])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 2U])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 3U])) << 24U));
}

void SoundRuntimeSlot::put32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value & 0xffU);
    raw_[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
    raw_[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xffU);
    raw_[offset + 3U] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

std::uint32_t SoundRuntimeSlot::state_flags() const noexcept {
    return u32(0x20U);
}

void SoundRuntimeSlot::set_state_flags(std::uint32_t value) noexcept {
    put32(0x20U, value);
}

void SoundBankRuntime::build(std::span<const SoundDescription> descriptions) {
    descriptions_.clear();
    slots_.clear();
    description_slots_.clear();
    for (const SoundDescription& description : descriptions) {
        if (descriptions_.size() >= kMaximumSoundDescriptions) {
            throw SoundRuntimeError("sound bank exceeds the 0x42 description bound");
        }
        if (description.sound_id < 0) {
            break;
        }
        if (description.resource_name.empty() || description.resource_name.size()
            >= kSoundDescriptionNameSize) {
            throw SoundRuntimeError("sound resource name does not fit its 0x20-byte field");
        }
        if (std::any_of(
                descriptions_.begin(), descriptions_.end(),
                [&description](const SoundDescription& current) {
                    return current.sound_id == description.sound_id;
                })) {
            throw SoundRuntimeError("sound bank contains a duplicate sound id");
        }
        descriptions_.push_back(description);
    }
    description_slots_.resize(descriptions_.size());
}

const SoundRuntimeSlot& SoundBankRuntime::slot(std::size_t index) const {
    if (index >= slots_.size()) {
        throw SoundRuntimeError("sound runtime slot is outside the table");
    }
    return slots_[index];
}

std::optional<std::size_t> SoundBankRuntime::description_for_sound(
    std::int32_t sound_id) const noexcept {
    for (std::size_t index = 0; index < descriptions_.size(); ++index) {
        if (descriptions_[index].sound_id == sound_id) {
            return index;
        }
    }
    return std::nullopt;
}

std::string SoundBankRuntime::resource_path(std::int32_t sound_id) const {
    const auto index = description_for_sound(sound_id);
    if (!index.has_value()) {
        throw SoundRuntimeError("sound id is not present in the selected bank");
    }
    return "audio/" + descriptions_[*index].resource_name + ".wav";
}

std::size_t SoundBankRuntime::publish_loaded_sound(std::size_t description_index) {
    if (description_index >= descriptions_.size()) {
        throw SoundRuntimeError("sound description is outside the selected bank");
    }
    if (description_slots_[description_index].has_value()) {
        return *description_slots_[description_index];
    }
    if (slots_.size() >= kMaximumSoundSlots) {
        throw SoundRuntimeError("sound runtime table exceeds the 0x80 slot bound");
    }
    const std::size_t slot_index = slots_.size();
    slots_.push_back(SoundRuntimeSlot(description_index));
    description_slots_[description_index] = slot_index;
    return slot_index;
}

void SoundBankRuntime::mark_started(std::size_t slot_index) {
    if (slot_index >= slots_.size()) {
        throw SoundRuntimeError("sound runtime slot is outside the table");
    }
    SoundRuntimeSlot& current = slots_[slot_index];
    const std::uint32_t current_flags = current.state_flags() & ~0x2U;
    const SoundDescription& description = descriptions_[current.description_index()];
    current.set_state_flags(current_flags | ((description.post_start_flags != 0U) ? 0x2U : 0U));
}

void SoundBankRuntime::mark_stopped(std::size_t slot_index) noexcept {
    if (slot_index < slots_.size()) {
        slots_[slot_index].set_state_flags(0);
    }
}

} // namespace opentony::assets
