#include "resource_runtime.hpp"

#include "pkr_asset.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <utility>

namespace opentony::assets {
namespace {

[[nodiscard]] std::filesystem::path join_resource_path(
    const std::filesystem::path& root,
    std::string_view path) {
    const std::filesystem::path requested{std::string(path)};
    return root.empty() ? requested : root / requested;
}

[[nodiscard]] std::int64_t checked_signed_size(std::size_t value) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
        throw ResourceRuntimeError("resource size exceeds signed seek range");
    }
    return static_cast<std::int64_t>(value);
}

[[nodiscard]] std::size_t checked_element_bytes(
    std::size_t element_count,
    std::size_t element_size) {
    if (element_size != 0
        && element_count > std::numeric_limits<std::size_t>::max() / element_size) {
        throw ResourceRuntimeError("resource element read size overflows the host");
    }
    return element_count * element_size;
}

} // namespace

ResourceBackend::ResourceBackend(std::filesystem::path direct_root)
    : direct_root_(std::move(direct_root)) {}

ResourceBackend::ResourceBackend(const PkrArchive& package)
    : package_(&package) {}

ResourceHandle ResourceBackend::add_slot(
    std::string name,
    std::vector<std::byte> bytes,
    ResourceSourceKind source_kind) {
    for (ResourceHandle index = 0; index < slots_.size(); ++index) {
        if (!slots_[index].has_value()) {
            slots_[index].emplace(Slot{
                std::move(name), std::move(bytes), 0, source_kind});
            return index;
        }
    }
    slots_.emplace_back(Slot{
        std::move(name), std::move(bytes), 0, source_kind});
    return slots_.size() - 1;
}

std::vector<std::byte> ResourceBackend::read_file(std::string_view path) const {
    const std::filesystem::path resolved = join_resource_path(direct_root_, path);
    std::ifstream input(resolved, std::ios::binary | std::ios::ate);
    if (!input) {
        throw ResourceRuntimeError(
            "cannot open resource file: " + resolved.string());
    }
    const std::streamoff length = input.tellg();
    if (length < 0) {
        throw ResourceRuntimeError(
            "cannot determine resource file size: " + resolved.string());
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw ResourceRuntimeError(
                "cannot read resource file: " + resolved.string());
        }
    }
    return bytes;
}

ResourceHandle ResourceBackend::open(std::string_view path) {
    if (path.empty()) {
        throw ResourceRuntimeError("resource path is empty");
    }
    if (package_ != nullptr) {
        const PkrFileEntry* found = package_->find(path);
        if (found == nullptr) {
            throw ResourceRuntimeError(
                "PKR resource was not found: " + std::string(path));
        }
        const ResourceHandle index = static_cast<ResourceHandle>(
            std::distance(package_->entries().data(), found));
        return add_slot(
            found->archive_path(), package_->decode(index),
            ResourceSourceKind::PkrPackage);
    }
    return add_slot(
        std::string(path), read_file(path), ResourceSourceKind::DirectFile);
}

const ResourceBackend::Slot& ResourceBackend::slot(ResourceHandle handle) const {
    if (handle == kInvalidResourceHandle
        || handle >= slots_.size()
        || !slots_[handle].has_value()) {
        throw ResourceRuntimeError("resource handle is not open");
    }
    return *slots_[handle];
}

ResourceBackend::Slot& ResourceBackend::slot(ResourceHandle handle) {
    if (handle == kInvalidResourceHandle
        || handle >= slots_.size()
        || !slots_[handle].has_value()) {
        throw ResourceRuntimeError("resource handle is not open");
    }
    return *slots_[handle];
}

void ResourceBackend::seek(
    ResourceHandle handle,
    std::int64_t offset,
    ResourceSeekOrigin origin) {
    Slot& current = slot(handle);
    const std::int64_t base = [&]() {
        switch (origin) {
        case ResourceSeekOrigin::Begin:
            return std::int64_t{0};
        case ResourceSeekOrigin::Current:
            return checked_signed_size(current.position);
        case ResourceSeekOrigin::End:
            return checked_signed_size(current.bytes.size());
        }
        throw ResourceRuntimeError("resource seek origin is unsupported");
    }();
    if ((offset > 0 && base > std::numeric_limits<std::int64_t>::max() - offset)
        || (offset < 0 && base < std::numeric_limits<std::int64_t>::min() - offset)) {
        throw ResourceRuntimeError("resource seek overflows the signed range");
    }
    const std::int64_t target = base + offset;
    if (target < 0
        || target > checked_signed_size(current.bytes.size())) {
        throw ResourceRuntimeError("resource seek is outside the resource");
    }
    current.position = static_cast<std::size_t>(target);
}

std::size_t ResourceBackend::read(
    ResourceHandle handle,
    std::span<std::byte> destination) {
    Slot& current = slot(handle);
    const std::size_t available = current.bytes.size() - current.position;
    const std::size_t amount = std::min(destination.size(), available);
    if (amount != 0) {
        std::memcpy(
            destination.data(), current.bytes.data() + current.position, amount);
        current.position += amount;
    }
    return amount;
}

void ResourceBackend::read_exact(
    ResourceHandle handle,
    std::span<std::byte> destination) {
    const std::size_t amount = read(handle, destination);
    if (amount != destination.size()) {
        throw ResourceRuntimeError("resource read ended before the requested size");
    }
}

void ResourceBackend::close(ResourceHandle handle) {
    if (handle == kInvalidResourceHandle
        || handle >= slots_.size()
        || !slots_[handle].has_value()) {
        throw ResourceRuntimeError("resource handle is not open");
    }
    slots_[handle].reset();
}

bool ResourceBackend::is_open(ResourceHandle handle) const noexcept {
    return handle != kInvalidResourceHandle
        && handle < slots_.size()
        && slots_[handle].has_value();
}

std::size_t ResourceBackend::size(ResourceHandle handle) const {
    return slot(handle).bytes.size();
}

std::size_t ResourceBackend::position(ResourceHandle handle) const {
    return slot(handle).position;
}

ResourceSourceKind ResourceBackend::source_kind(ResourceHandle handle) const {
    return slot(handle).source_kind;
}

const std::string& ResourceBackend::name(ResourceHandle handle) const {
    return slot(handle).name;
}

ResourceStream ResourceStream::open(
    ResourceBackend& backend,
    std::string_view path) {
    return ResourceStream(backend, backend.open(path));
}

ResourceStream::ResourceStream(
    ResourceBackend& backend,
    ResourceHandle handle) noexcept
    : backend_(&backend), handle_(handle) {}

ResourceStream::~ResourceStream() {
    close();
}

ResourceStream::ResourceStream(ResourceStream&& other) noexcept
    : backend_(other.backend_), handle_(other.handle_) {
    other.backend_ = nullptr;
    other.handle_ = kInvalidResourceHandle;
}

ResourceStream& ResourceStream::operator=(ResourceStream&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    close();
    backend_ = other.backend_;
    handle_ = other.handle_;
    other.backend_ = nullptr;
    other.handle_ = kInvalidResourceHandle;
    return *this;
}

void ResourceStream::seek(std::int64_t offset, ResourceSeekOrigin origin) {
    if (!is_open()) {
        throw ResourceRuntimeError("resource stream is closed");
    }
    backend_->seek(handle_, offset, origin);
}

std::size_t ResourceStream::read(std::span<std::byte> destination) {
    if (!is_open()) {
        throw ResourceRuntimeError("resource stream is closed");
    }
    return backend_->read(handle_, destination);
}

std::size_t ResourceStream::read_elements(
    std::size_t element_count,
    std::size_t element_size,
    std::span<std::byte> destination) {
    const std::size_t amount = checked_element_bytes(element_count, element_size);
    if (destination.size() < amount) {
        throw ResourceRuntimeError(
            "resource element destination is smaller than the requested read");
    }
    if (!is_open()) {
        throw ResourceRuntimeError("resource stream is closed");
    }
    const std::span<std::byte> requested = destination.first(amount);
    backend_->read_exact(handle_, requested);
    return amount;
}

void ResourceStream::close() noexcept {
    if (backend_ == nullptr || handle_ == kInvalidResourceHandle) {
        return;
    }
    if (backend_->is_open(handle_)) {
        try {
            backend_->close(handle_);
        } catch (...) {
            // Destruction cannot report a backend error. Normal close paths
            // use the throwing ResourceBackend::close directly.
        }
    }
    backend_ = nullptr;
    handle_ = kInvalidResourceHandle;
}

bool ResourceStream::is_open() const noexcept {
    return backend_ != nullptr && backend_->is_open(handle_);
}

std::size_t ResourceStream::size() const {
    if (!is_open()) {
        throw ResourceRuntimeError("resource stream is closed");
    }
    return backend_->size(handle_);
}

std::size_t ResourceStream::position() const {
    if (!is_open()) {
        throw ResourceRuntimeError("resource stream is closed");
    }
    return backend_->position(handle_);
}

ResourceLoader::ResourceLoader(
    ResourceBackend& backend,
    const PreRuntimeManager* pre) noexcept
    : backend_(&backend), pre_(pre) {}

ResourceLoader::~ResourceLoader() {
    close();
}

bool ResourceLoader::eligible_for_pre_lookup(std::string_view name) noexcept {
    return !name.empty()
        && name.size() < kEmbeddedNameLimit
        && name.find('/') == std::string_view::npos
        && name.find('\\') == std::string_view::npos;
}

void ResourceLoader::close_active() noexcept {
    if (!active_.has_value()) {
        return;
    }
    if (active_->backend_handle != kInvalidResourceHandle
        && backend_->is_open(active_->backend_handle)) {
        try {
            backend_->close(active_->backend_handle);
        } catch (...) {
            // ResourceLoader::close is the no-throw cleanup boundary.
        }
    }
    active_.reset();
}

std::size_t ResourceLoader::open(std::string_view name) {
    close_active();
    if (name.empty()) {
        throw ResourceRuntimeError("resource name is empty");
    }
    if (pre_ != nullptr && eligible_for_pre_lookup(name)) {
        const std::optional<PreEmbeddedResourceView> embedded =
            pre_->find_embedded(name);
        if (embedded.has_value()) {
            active_.emplace(ActiveLoad{
                std::string(name), ResourceSourceKind::PreEmbedded,
                embedded->payload, kInvalidResourceHandle, false});
            return embedded->payload.size();
        }
    }

    const ResourceHandle handle = backend_->open(name);
    active_.emplace(ActiveLoad{
        std::string(name), backend_->source_kind(handle), {}, handle, false});
    return backend_->size(handle);
}

const ResourceLoader::ActiveLoad& ResourceLoader::require_active() const {
    if (!active_.has_value()) {
        throw ResourceRuntimeError("no resource load is active");
    }
    return *active_;
}

ResourceLoader::ActiveLoad& ResourceLoader::require_active() {
    if (!active_.has_value()) {
        throw ResourceRuntimeError("no resource load is active");
    }
    return *active_;
}

void ResourceLoader::load(std::span<std::byte> destination) {
    ActiveLoad& current = require_active();
    if (current.load_started) {
        throw ResourceRuntimeError("resource load has already started");
    }
    const std::size_t expected_size = current.source_kind == ResourceSourceKind::PreEmbedded
        ? current.embedded_payload.size()
        : backend_->size(current.backend_handle);
    if (destination.size() < expected_size) {
        close_active();
        throw ResourceRuntimeError(
            "resource destination is smaller than the loaded resource");
    }
    try {
        if (current.source_kind == ResourceSourceKind::PreEmbedded) {
            if (expected_size != 0) {
                std::memcpy(
                    destination.data(), current.embedded_payload.data(), expected_size);
            }
        } else {
            backend_->read_exact(
                current.backend_handle, destination.first(expected_size));
        }
        current.load_started = true;
    } catch (...) {
        close_active();
        throw;
    }
}

void ResourceLoader::synchronize() {
    ActiveLoad& current = require_active();
    if (!current.load_started) {
        throw ResourceRuntimeError("resource load was not started");
    }
    close_active();
}

void ResourceLoader::close() noexcept {
    close_active();
}

std::vector<std::byte> ResourceLoader::load_owned(std::string_view name) {
    const std::size_t loaded_size = open(name);
    std::vector<std::byte> bytes(loaded_size);
    load(bytes);
    synchronize();
    return bytes;
}

bool ResourceLoader::load_started() const noexcept {
    return active_.has_value() && active_->load_started;
}

std::size_t ResourceLoader::size() const {
    const ActiveLoad& current = require_active();
    return current.source_kind == ResourceSourceKind::PreEmbedded
        ? current.embedded_payload.size()
        : backend_->size(current.backend_handle);
}

ResourceSourceKind ResourceLoader::source_kind() const {
    return require_active().source_kind;
}

const std::string& ResourceLoader::name() const {
    return require_active().name;
}

} // namespace opentony::assets
