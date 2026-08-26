#include "skater_runtime.hpp"

namespace opentony::runtime {
namespace {

[[nodiscard]] std::uint32_t read_u32(const std::vector<std::byte>& bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(bytes[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 2])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset + 3])) << 24U));
}

void write_u32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) noexcept {
    bytes[offset] = static_cast<std::byte>(value & 0xffU);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8U) & 0xffU);
    bytes[offset + 2] = static_cast<std::byte>((value >> 16U) & 0xffU);
    bytes[offset + 3] = static_cast<std::byte>((value >> 24U) & 0xffU);
}

} // namespace

CameraRuntimeObject::CameraRuntimeObject(std::size_t parent_player_index) noexcept
    : raw_(kGameplayCameraObjectSize, std::byte{0}),
      parent_player_index_(parent_player_index) {
    set_mode(1);
}

std::uint32_t CameraRuntimeObject::mode() const noexcept {
    return read_u32(raw_, 0x504U);
}

std::uint32_t CameraRuntimeObject::update_tick() const noexcept {
    return read_u32(raw_, 0x510U);
}

void CameraRuntimeObject::set_mode(std::uint32_t value) noexcept {
    write_u32(raw_, 0x504U, value);
}

void CameraRuntimeObject::set_update_tick(std::uint32_t value) noexcept {
    write_u32(raw_, 0x510U, value);
}

SkaterRuntimeObject::SkaterRuntimeObject(
    std::size_t player_index,
    std::uint8_t psx_region_slot,
    std::uint16_t model_index,
    FixedPosition position) noexcept
    : raw_(kGameplaySkaterObjectSize, std::byte{0}),
      player_index_(player_index),
      camera_(player_index),
      player_(position) {
    raw_[0x1f] = static_cast<std::byte>(psx_region_slot);
    raw_[0x1a] = static_cast<std::byte>(model_index & 0xffU);
    raw_[0x1b] = static_cast<std::byte>(model_index >> 8U);
    write_u32(raw_, 0x2cc4U, static_cast<std::uint32_t>(player_index));
}

std::uint8_t SkaterRuntimeObject::psx_region_slot() const noexcept {
    return std::to_integer<std::uint8_t>(raw_[0x1f]);
}

std::uint16_t SkaterRuntimeObject::model_index() const noexcept {
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(raw_[0x1a])
        | (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(raw_[0x1b])) << 8U));
}

void SkaterRuntimeObject::set_psx_binding(
    std::uint8_t psx_region_slot,
    std::uint16_t model_index_value) noexcept {
    raw_[0x1f] = static_cast<std::byte>(psx_region_slot);
    raw_[0x1a] = static_cast<std::byte>(model_index_value & 0xffU);
    raw_[0x1b] = static_cast<std::byte>(model_index_value >> 8U);
}

} // namespace opentony::runtime
