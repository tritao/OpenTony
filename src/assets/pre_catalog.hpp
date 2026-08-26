#pragma once

#include "pre_asset.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace opentony::assets {

class PreAssetCatalog final {
public:
    static PreAssetCatalog scan(const std::string& root);
    static std::string key_for(std::string_view name);

    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] const std::string* path_for(std::string_view name) const;
    [[nodiscard]] const PreArchive& load(std::string_view name) const;
    [[nodiscard]] std::size_t size() const noexcept { return paths_.size(); }

private:
    std::unordered_map<std::string, std::string> paths_;
    mutable std::unordered_map<std::string, std::shared_ptr<PreArchive>> archives_;
};

} // namespace opentony::assets
