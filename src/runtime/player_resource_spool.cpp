#include "player_resource_spool.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <utility>

namespace opentony::runtime {

namespace {

[[nodiscard]] std::string upper_copy(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::toupper(character));
        });
    return value;
}

[[nodiscard]] std::optional<std::size_t> package_resource_entry(
    const assets::PkrArchive& package,
    std::string_view base,
    std::string_view extension) {
    const std::string wanted = upper_copy(
        std::string(base) + std::string(extension));
    std::optional<std::size_t> fallback;
    for (std::size_t index = 0; index < package.entries().size(); ++index) {
        const auto& entry = package.entries()[index];
        if (upper_copy(entry.name) != wanted) {
            continue;
        }
        if (upper_copy(entry.directory) == "DATA") {
            return index;
        }
        if (!fallback.has_value()) {
            fallback = index;
        }
    }
    return fallback;
}

} // namespace

PlayerResourceSpool::PlayerResourceSpool() noexcept {
    reset();
}

void PlayerResourceSpool::reset() noexcept {
    raw_.fill(std::byte{0});
    request_sizes_.fill(0);
    request_flags_.fill(0);
    loaded_.clear();
    for (std::size_t index = 0; index < kPlayerSpoolEntryCount; ++index) {
        write_u8(entry_offset(index) + 0x04U, 0);
        write_u32(entry_offset(index) + 0x1cU, 0xffffffffU);
    }
}

std::size_t PlayerResourceSpool::entry_offset(std::size_t index) const {
    if (index >= kPlayerSpoolEntryCount) {
        throw std::out_of_range("player spool entry index is outside the manager");
    }
    return 0x04U + index * kPlayerSpoolEntrySize;
}

std::uint32_t PlayerResourceSpool::read_u32(
    std::size_t offset) const noexcept {
    return static_cast<std::uint32_t>(
        std::to_integer<std::uint8_t>(raw_[offset])
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 1U])) << 8U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 2U])) << 16U)
        | (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(raw_[offset + 3U])) << 24U));
}

std::uint8_t PlayerResourceSpool::read_u8(
    std::size_t offset) const noexcept {
    return std::to_integer<std::uint8_t>(raw_[offset]);
}

void PlayerResourceSpool::write_u32(
    std::size_t offset,
    std::uint32_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value & 0xffU);
    raw_[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xffU);
    raw_[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xffU);
    raw_[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

void PlayerResourceSpool::write_u8(
    std::size_t offset,
    std::uint8_t value) noexcept {
    raw_[offset] = static_cast<std::byte>(value);
}

std::size_t PlayerResourceSpool::enqueue(
    std::string_view base_name,
    PlayerSpoolResourceKind resource_kind,
    std::int32_t heap,
    std::uint32_t request_size,
    std::uint8_t flags) {
    if (base_name.empty() || base_name.size() > 8U) {
        throw std::invalid_argument(
            "player spool resource name must fit the retail eight-byte base");
    }
    const std::size_t count = queued_count();
    if (count >= kPlayerSpoolEntryCount) {
        throw std::overflow_error("player resource spool has no free entry");
    }
    const std::size_t offset = entry_offset(count);
    write_u8(offset + 0x04U, 0);
    for (std::size_t index = 0; index < 19U; ++index) {
        write_u8(
            offset + 0x05U + index,
            index < base_name.size()
                ? static_cast<std::uint8_t>(base_name[index])
                : 0);
    }
    write_u8(offset + 0x18U, static_cast<std::uint8_t>(resource_kind));
    write_u32(offset + 0x1cU, 0xffffffffU);
    write_u32(offset + 0x24U, static_cast<std::uint32_t>(heap));
    request_sizes_[count] = request_size;
    request_flags_[count] = flags;
    write_u32(0xa04U, static_cast<std::uint32_t>(count + 1U));
    return count;
}

bool PlayerResourceSpool::start_next() {
    if (consume_index() >= queued_count()) {
        write_u32(0xa0cU, 0);
        return false;
    }
    write_u32(0xa0cU, 1);
    return true;
}

std::size_t PlayerResourceSpool::load_current(const std::string& asset_root) {
    if (state() != 1U || current_index() >= queued_count()) {
        throw std::logic_error("player spool has no active load");
    }
    const std::size_t index = current_index();
    const std::string base = name(index);
    const std::filesystem::path root(asset_root);
    PlayerSpoolLoadedResource resource{};
    resource.queue_index = index;
    resource.kind = kind(index);
    if (resource.kind == PlayerSpoolResourceKind::PshRegion) {
        resource.psh = assets::PshManifest::load(
            (root / (base + ".PSH")).string());
    } else {
        resource.psx = assets::PsxArchive::load(
            (root / (base + ".PSX")).string());
    }
    loaded_.push_back(std::move(resource));
    write_u8(entry_offset(index) + 0x04U, 1);
    write_u32(0xa0cU, 2);
    return index;
}

std::size_t PlayerResourceSpool::load_current(
    const assets::PkrArchive& package) {
    if (state() != 1U || current_index() >= queued_count()) {
        throw std::logic_error("player spool has no active load");
    }
    const std::size_t index = current_index();
    const std::string base = name(index);
    PlayerSpoolLoadedResource resource{};
    resource.queue_index = index;
    resource.kind = kind(index);
    const std::string extension = resource.kind == PlayerSpoolResourceKind::PshRegion
        ? ".PSH"
        : ".PSX";
    const auto entry = package_resource_entry(package, base, extension);
    if (!entry.has_value()) {
        throw std::runtime_error(
            "player spool package resource was not found: " + base + extension);
    }
    const std::string source = package.entry(*entry).archive_path();
    if (resource.kind == PlayerSpoolResourceKind::PshRegion) {
        resource.psh = assets::PshManifest::parse(
            package.decode(*entry), source, base);
    } else {
        resource.psx = assets::PsxArchive::parse(
            package.decode(*entry), source);
    }
    loaded_.push_back(std::move(resource));
    write_u8(entry_offset(index) + 0x04U, 1);
    write_u32(0xa0cU, 2);
    return index;
}

void PlayerResourceSpool::complete_current() {
    if (state() != 2U || current_index() >= queued_count()) {
        throw std::logic_error("player spool has no loaded entry to complete");
    }
    const std::size_t index = current_index();
    write_u32(entry_offset(index) + 0x1cU, 0xffffffffU);
    write_u32(0xa08U, static_cast<std::uint32_t>(index + 1U));
    write_u32(0xa0cU, consume_index() >= queued_count() ? 0U : 2U);
}

std::size_t PlayerResourceSpool::queued_count() const noexcept {
    return read_u32(0xa04U);
}

std::size_t PlayerResourceSpool::consume_index() const noexcept {
    return read_u32(0xa08U);
}

std::uint32_t PlayerResourceSpool::state() const noexcept {
    return read_u32(0xa0cU);
}

std::size_t PlayerResourceSpool::current_index() const noexcept {
    return consume_index();
}

bool PlayerResourceSpool::processed(std::size_t index) const {
    return read_u8(entry_offset(index) + 0x04U) != 0;
}

std::string PlayerResourceSpool::name(std::size_t index) const {
    const std::size_t offset = entry_offset(index) + 0x05U;
    std::string result;
    result.reserve(18U);
    for (std::size_t character = 0; character < 19U; ++character) {
        const std::uint8_t value = read_u8(offset + character);
        if (value == 0) {
            break;
        }
        result.push_back(static_cast<char>(value));
    }
    return result;
}

PlayerSpoolResourceKind PlayerResourceSpool::kind(std::size_t index) const {
    return static_cast<PlayerSpoolResourceKind>(
        read_u8(entry_offset(index) + 0x18U));
}

std::int32_t PlayerResourceSpool::heap_selector(std::size_t index) const {
    return static_cast<std::int32_t>(
        read_u32(entry_offset(index) + 0x24U));
}

std::uint8_t PlayerResourceSpool::request_flags(std::size_t index) const {
    if (index >= kPlayerSpoolEntryCount) {
        throw std::out_of_range("player spool flag index is outside the manager");
    }
    return request_flags_[index];
}

std::uint32_t PlayerResourceSpool::request_size_staging(
    std::size_t index) const {
    if (index >= kPlayerSpoolEntryCount) {
        throw std::out_of_range("player spool request index is outside the manager");
    }
    return request_sizes_[index];
}

const PlayerSpoolLoadedResource* PlayerResourceSpool::loaded(
    std::size_t queue_index) const noexcept {
    for (const PlayerSpoolLoadedResource& resource : loaded_) {
        if (resource.queue_index == queue_index) {
            return &resource;
        }
    }
    return nullptr;
}

} // namespace opentony::runtime
