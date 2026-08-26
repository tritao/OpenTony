#pragma once

#include "pkr_asset.hpp"
#include "pre_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::assets {

class ResourceRuntimeError final : public std::runtime_error {
public:
    explicit ResourceRuntimeError(const std::string& message)
        : std::runtime_error(message) {}
};

using ResourceHandle = std::size_t;
inline constexpr ResourceHandle kInvalidResourceHandle = static_cast<ResourceHandle>(-1);

enum class ResourceSourceKind : std::uint8_t {
    DirectFile,
    PkrPackage,
    PreEmbedded,
};

enum class ResourceSeekOrigin : std::uint8_t {
    Begin,
    Current,
    End,
};

// The retail backend exposes an abstract slot rather than a Win32 file
// descriptor. A native backend owns the bytes for one open resource so a
// direct file and a decoded PKR2 entry have the same seek/read lifetime.
class ResourceBackend final {
public:
    explicit ResourceBackend(std::filesystem::path direct_root = {});
    explicit ResourceBackend(const PkrArchive& package);

    [[nodiscard]] ResourceHandle open(std::string_view path);
    void seek(
        ResourceHandle handle,
        std::int64_t offset,
        ResourceSeekOrigin origin = ResourceSeekOrigin::Begin);
    [[nodiscard]] std::size_t read(
        ResourceHandle handle,
        std::span<std::byte> destination);
    void read_exact(ResourceHandle handle, std::span<std::byte> destination);
    void close(ResourceHandle handle);

    [[nodiscard]] bool is_open(ResourceHandle handle) const noexcept;
    [[nodiscard]] std::size_t size(ResourceHandle handle) const;
    [[nodiscard]] std::size_t position(ResourceHandle handle) const;
    [[nodiscard]] ResourceSourceKind source_kind(ResourceHandle handle) const;
    [[nodiscard]] const std::string& name(ResourceHandle handle) const;

private:
    struct Slot {
        std::string name;
        std::vector<std::byte> bytes;
        std::size_t position{};
        ResourceSourceKind source_kind{};
    };

    std::filesystem::path direct_root_;
    const PkrArchive* package_{};
    std::vector<std::optional<Slot>> slots_;

    [[nodiscard]] ResourceHandle add_slot(
        std::string name,
        std::vector<std::byte> bytes,
        ResourceSourceKind source_kind);
    [[nodiscard]] const Slot& slot(ResourceHandle handle) const;
    [[nodiscard]] Slot& slot(ResourceHandle handle);
    [[nodiscard]] std::vector<std::byte> read_file(std::string_view path) const;
};

// Native adapter for the observed 0x38-byte package stream. The retail
// buffering flags and transfer block are intentionally not exposed as
// semantics; reads still preserve the checked element multiplication and
// cursor contract at 0x00502387.
class ResourceStream final {
public:
    static ResourceStream open(ResourceBackend& backend, std::string_view path);

    ResourceStream(ResourceBackend& backend, ResourceHandle handle) noexcept;
    ~ResourceStream();

    ResourceStream(const ResourceStream&) = delete;
    ResourceStream& operator=(const ResourceStream&) = delete;
    ResourceStream(ResourceStream&& other) noexcept;
    ResourceStream& operator=(ResourceStream&& other) noexcept;

    void seek(
        std::int64_t offset,
        ResourceSeekOrigin origin = ResourceSeekOrigin::Begin);
    [[nodiscard]] std::size_t read(std::span<std::byte> destination);
    [[nodiscard]] std::size_t read_elements(
        std::size_t element_count,
        std::size_t element_size,
        std::span<std::byte> destination);
    void close() noexcept;

    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t position() const;

private:
    ResourceBackend* backend_{};
    ResourceHandle handle_{kInvalidResourceHandle};
};

// Counterpart of the game-owned 0x00449030/0x00449230/0x00449660 boundary.
// The native result is a value-owned byte vector. It deliberately leaves the
// retail allocator's count prefix and asynchronous spool implementation as
// platform adapters while preserving their ownership seam.
class ResourceLoader final {
public:
    // 0x004a9410 copies only a short resource name into its 16-byte local.
    // A separator-bearing path is therefore sent directly to the backend.
    inline static constexpr std::size_t kEmbeddedNameLimit = 0x10;

    ResourceLoader(ResourceBackend& backend, const PreRuntimeManager* pre = nullptr) noexcept;
    ~ResourceLoader();

    ResourceLoader(const ResourceLoader&) = delete;
    ResourceLoader& operator=(const ResourceLoader&) = delete;

    [[nodiscard]] std::size_t open(std::string_view name);
    void load(std::span<std::byte> destination);
    void synchronize();
    void close() noexcept;
    [[nodiscard]] std::vector<std::byte> load_owned(std::string_view name);

    [[nodiscard]] bool active() const noexcept { return active_.has_value(); }
    [[nodiscard]] bool load_started() const noexcept;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] ResourceSourceKind source_kind() const;
    [[nodiscard]] const std::string& name() const;

private:
    struct ActiveLoad {
        std::string name;
        ResourceSourceKind source_kind{};
        std::span<const std::byte> embedded_payload{};
        ResourceHandle backend_handle{kInvalidResourceHandle};
        bool load_started{};
    };

    ResourceBackend* backend_{};
    const PreRuntimeManager* pre_{};
    std::optional<ActiveLoad> active_;

    static bool eligible_for_pre_lookup(std::string_view name) noexcept;
    void close_active() noexcept;
    [[nodiscard]] const ActiveLoad& require_active() const;
    [[nodiscard]] ActiveLoad& require_active();
};

} // namespace opentony::assets
