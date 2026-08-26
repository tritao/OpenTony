#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace opentony::assets {

class PreFormatError final : public std::runtime_error {
public:
    explicit PreFormatError(const std::string& message)
        : std::runtime_error(message) {}
};

struct PreEntry {
    std::string name;
    std::size_t data_offset{};
    std::uint32_t size{};
};

// Inline PC PRE resource container. The parser owns the original bytes so a
// later renderer/object loader can obtain bounded payload spans without
// re-reading or guessing record boundaries.
class PreArchive final {
public:
    static PreArchive load(const std::string& path);
    static PreArchive parse(std::vector<std::byte> bytes, std::string source = {});

    [[nodiscard]] const std::vector<PreEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] const PreEntry& entry(std::size_t index) const;
    [[nodiscard]] const PreEntry* find(std::string_view name) const noexcept;
    [[nodiscard]] std::span<const std::byte> payload(std::size_t index) const;
    [[nodiscard]] std::span<const std::byte> payload(std::string_view name) const;
    [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }

private:
    std::vector<std::byte> bytes_;
    std::string source_;
    std::vector<PreEntry> entries_;
};

} // namespace opentony::assets
