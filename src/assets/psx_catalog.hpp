#pragma once

#include "psx_asset.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace opentony::assets {

// Indexes extracted PSX resources by filename stem (for example, "SkWare_L"
// resolves to SKWARE_L.PSX). The catalog owns lazy parsed archives so trigger
// factory records can resolve resources without coupling the runtime to a
// particular directory layout.
class PsxAssetCatalog final {
public:
    static PsxAssetCatalog scan(const std::string& root);
    static std::string key_for(std::string_view name);

    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] const std::string* path_for(std::string_view name) const;
    [[nodiscard]] const PsxArchive& load(std::string_view name) const;
    [[nodiscard]] std::size_t size() const noexcept { return paths_.size(); }

private:
    std::unordered_map<std::string, std::string> paths_;
    mutable std::unordered_map<std::string, std::shared_ptr<PsxArchive>> archives_;
};

} // namespace opentony::assets
