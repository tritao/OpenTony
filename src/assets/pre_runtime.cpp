#include "pre_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] unsigned char lower_ascii(unsigned char value) noexcept {
    return static_cast<unsigned char>(std::tolower(value));
}

[[nodiscard]] bool equal_case_insensitive(
    std::string_view left,
    std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    return std::equal(
        left.begin(), left.end(), right.begin(),
        [](char left_character, char right_character) {
            return lower_ascii(static_cast<unsigned char>(left_character))
                == lower_ascii(static_cast<unsigned char>(right_character));
        });
}

} // namespace

void PreRuntimeManager::validate_container_name(std::string_view name) {
    if (name.empty()) {
        throw PreFormatError("PRE manager name is empty");
    }
    if (name.size() >= kRuntimePreNameSize) {
        throw PreFormatError("PRE manager name exceeds the 16-byte slot");
    }
}

std::size_t PreRuntimeManager::load(
    std::string name,
    std::vector<std::byte> bytes) {
    validate_container_name(name);
    std::size_t slot = slots_.size();
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (!slots_[index].has_value()) {
            slot = index;
            break;
        }
    }
    if (slot == slots_.size()) {
        throw PreFormatError("PRE manager has no free slots");
    }

    PreArchive archive = PreArchive::parse(std::move(bytes), name);
    slots_[slot].emplace(Slot{std::move(name), std::move(archive)});
    ++loaded_count_;
    return slot;
}

std::size_t PreRuntimeManager::load_file(
    std::string name,
    const std::string& path) {
    validate_container_name(name);
    std::size_t slot = slots_.size();
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        if (!slots_[index].has_value()) {
            slot = index;
            break;
        }
    }
    if (slot == slots_.size()) {
        throw PreFormatError("PRE manager has no free slots");
    }

    PreArchive archive = PreArchive::load(path);
    slots_[slot].emplace(Slot{std::move(name), std::move(archive)});
    ++loaded_count_;
    return slot;
}

const PreArchive& PreRuntimeManager::container(std::size_t slot) const {
    if (slot >= slots_.size() || !slots_[slot].has_value()) {
        throw PreFormatError("PRE manager slot is not loaded");
    }
    return slots_[slot]->archive;
}

const std::string& PreRuntimeManager::container_name(std::size_t slot) const {
    if (slot >= slots_.size() || !slots_[slot].has_value()) {
        throw PreFormatError("PRE manager slot is not loaded");
    }
    return slots_[slot]->name;
}

std::optional<PreEmbeddedResourceView> PreRuntimeManager::find_embedded(
    std::string_view resource_name) const {
    if (resource_name.empty()) {
        return std::nullopt;
    }
    for (std::size_t slot = 0; slot < slots_.size(); ++slot) {
        if (!slots_[slot].has_value()) {
            continue;
        }
        const PreArchive& archive = slots_[slot]->archive;
        for (std::size_t entry_index = 0; entry_index < archive.entries().size(); ++entry_index) {
            const PreEntry& entry = archive.entries()[entry_index];
            if (!equal_case_insensitive(entry.name, resource_name)) {
                continue;
            }
            return PreEmbeddedResourceView{
                slot,
                slots_[slot]->name,
                entry.name,
                archive.payload(entry_index),
            };
        }
    }
    return std::nullopt;
}

void PreRuntimeManager::unload(std::string_view name) {
    for (std::optional<Slot>& slot : slots_) {
        if (!slot.has_value() || !equal_case_insensitive(slot->name, name)) {
            continue;
        }
        slot.reset();
        --loaded_count_;
        return;
    }
    throw PreFormatError("PRE file is not loaded: " + std::string(name));
}

} // namespace opentony::assets
