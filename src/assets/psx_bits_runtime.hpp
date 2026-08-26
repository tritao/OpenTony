#pragma once

#include "psx_asset.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace opentony::assets {

inline constexpr std::size_t kPsxBitsGroupNameSize = 8;
inline constexpr std::size_t kPsxBitsEntrySize = 8;

struct PsxBitsGroup {
    std::string name;
    std::vector<std::array<std::byte, kPsxBitsEntrySize>> entries;
};

// Value-preserving reader for the proven PSX type-0x45 / PK_VERTEXCOLOURS
// named-resource table. Group payload entries are deliberately raw: the
// executable proves their named lookup and downstream ownership, but not all
// per-entry colour/effect meanings.
class PsxBitsRuntime final {
public:
    void build(const PsxArchive& archive);

    [[nodiscard]] const std::vector<PsxBitsGroup>& groups() const noexcept {
        return groups_;
    }
    [[nodiscard]] const PsxBitsGroup* find(const std::string& name) const noexcept;
    [[nodiscard]] const PsxArchive& source_archive() const noexcept { return *archive_; }

private:
    const PsxArchive* archive_{};
    std::vector<PsxBitsGroup> groups_;
};

} // namespace opentony::assets
